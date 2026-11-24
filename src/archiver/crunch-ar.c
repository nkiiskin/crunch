/*
 * crunch-ar - An experimental static library archiving utility.
 */
#define _GNU_SOURCE
#include "version.h"
#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __CRUNCH__
#include <sys/random.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ARMAG "!<arch>\n"
#define SARMAG 8
#define ARFMAG "`\n"

typedef struct {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
} ArHdr;

typedef struct {
    char *name;
    uint8_t *data;
    size_t size;
    time_t mtime;
    uid_t uid;
    gid_t gid;
    mode_t mode;
} Member;

static const char *prog_name = "crunch-ar";
static int verbose = 0;
static int create_symtab = 0;
static char *temporary_archive;

static void cleanup_temporary(void) {
    if (temporary_archive) unlink(temporary_archive);
}

static void die(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s: error: ", prog_name);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    cleanup_temporary();
    exit(1);
}

static void *xcalloc(size_t n, size_t s) {
    void *p = calloc(n ? n : 1, s ? s : 1);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size ? size : 1);
    if (!p) die("out of memory");
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *p = strdup(s);
    if (!p) die("out of memory");
    return p;
}

static void header_number(char *field, size_t width, uint64_t value, int octal) {
    char text[32];
    size_t n = 0;
    unsigned base = octal ? 8 : 10;
    do {
        text[n++] = '0' + value % base;
        value /= base;
    } while (value);
    if (n > width) die("archive header value does not fit");
    for (size_t i = 0; i < n; i++) field[i] = text[n - i - 1];
}

static void store_be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static int create_temporary_archive(char *path, size_t path_len) {
#ifdef __CRUNCH__
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int attempt = 0; attempt < 256; attempt++) {
        unsigned char bytes[6];
        size_t used = 0;
        while (used < sizeof bytes) {
            ssize_t n = getrandom(bytes + used, sizeof bytes - used, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) return -1;
            if (n == 0) { errno = EIO; return -1; }
            used += (size_t)n;
        }
        for (size_t i = 0; i < sizeof bytes; i++)
            path[path_len + 5 + i] = alphabet[bytes[i] % 62];
        int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || errno != EEXIST) return fd;
    }
    errno = EEXIST;
    return -1;
#else
    (void)path_len;
    return mkstemp(path);
#endif
}

static void write_archive_bytes(int fd, const void *data, size_t size, const char *path) {
    const unsigned char *p = data;
    while (size) {
        size_t chunk = size > 1073741824u ? 1073741824u : size;
        ssize_t n = write(fd, p, chunk);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) die("write error on `%s'", path);
        p += n;
        size -= (size_t)n;
    }
}

static uint8_t *read_file(const char *path, size_t *len, struct stat *st_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) die("cannot open `%s': %s", path, strerror(errno));
    struct stat st;
    if (fstat(fileno(fp), &st) != 0) die("cannot stat `%s': %s", path, strerror(errno));
    if (st_out) *st_out = st;
    if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX - 8)
        die("file `%s' is too large", path);
    size_t sz = (size_t)st.st_size;
    uint8_t *buf = xcalloc(sz + 8, 1);
    if (sz > 0 && fread(buf, 1, sz, fp) != sz) die("read error on `%s'", path);
    if (fclose(fp) != 0) die("read error on `%s'", path);
    *len = sz;
    return buf;
}

static Member **read_archive(const char *ar_path, size_t *nmembers, int allow_missing) {
    *nmembers = 0;
    FILE *fp = fopen(ar_path, "rb");
    if (!fp) {
        if (allow_missing && errno == ENOENT) return NULL;
        die("cannot open `%s': %s", ar_path, strerror(errno));
    }
    char magic[SARMAG];
    if (fread(magic, 1, SARMAG, fp) != SARMAG || memcmp(magic, ARMAG, SARMAG) != 0) {
        die("`%s': file format not recognized", ar_path);
    }
    size_t cap = 16;
    Member **members = xcalloc(cap, sizeof(Member *));
    char *longnames = NULL;
    size_t longnames_len = 0;
    ArHdr hdr;
    size_t got;
    while ((got = fread(&hdr, 1, sizeof(ArHdr), fp)) != 0) {
        if (got != sizeof hdr) die("`%s': truncated member header", ar_path);
        if (memcmp(hdr.ar_fmag, ARFMAG, 2) != 0) die("`%s': corrupt member header", ar_path);
        char sz_str[11];
        memcpy(sz_str, hdr.ar_size, 10); sz_str[10] = '\0';
        if (!isdigit((unsigned char)sz_str[0])) die("`%s': invalid member size", ar_path);
        char *end;
        errno = 0;
        unsigned long long parsed = strtoull(sz_str, &end, 10);
        if (errno || end == sz_str || parsed > SIZE_MAX - 8) die("`%s': invalid member size", ar_path);
        while (*end == ' ') end++;
        if (*end) die("`%s': invalid member size", ar_path);
        size_t sz = (size_t)parsed;
        uint8_t *data = xcalloc(sz + 8, 1);
        if (sz > 0 && fread(data, 1, sz, fp) != sz) die("`%s': truncated data", ar_path);
        if ((sz & 1) && fgetc(fp) == EOF) die("`%s': missing member padding", ar_path);

        if (memcmp(hdr.ar_name, "//              ", 16) == 0) {
            free(longnames);
            longnames = (char *)data;
            longnames_len = sz;
            continue;
        } else if (memcmp(hdr.ar_name, "/               ", 16) == 0 ||
                   memcmp(hdr.ar_name, "/SYM64/         ", 16) == 0) {
            free(data);
            continue;
        }

        Member *m = xcalloc(1, sizeof(Member));
        m->data = data; m->size = sz;
        char buf[16];
        memcpy(buf, hdr.ar_date, 12); buf[12] = 0; m->mtime = (time_t)strtoull(buf, NULL, 10);
        memcpy(buf, hdr.ar_uid, 6);   buf[6] = 0;  m->uid   = (uid_t)strtoul(buf, NULL, 10);
        memcpy(buf, hdr.ar_gid, 6);   buf[6] = 0;  m->gid   = (gid_t)strtoul(buf, NULL, 10);
        memcpy(buf, hdr.ar_mode, 8);  buf[8] = 0;  m->mode  = (mode_t)strtoul(buf, NULL, 8);

        if (hdr.ar_name[0] == '/' && isdigit((unsigned char)hdr.ar_name[1])) {
            char offset_buf[16];
            memcpy(offset_buf, hdr.ar_name + 1, 15); offset_buf[15] = 0;
            errno = 0; char *offset_end;
            unsigned long long value = strtoull(offset_buf, &offset_end, 10);
            while (*offset_end == ' ') offset_end++;
            if (errno || *offset_end || value >= longnames_len)
                die("bad extended filename offset");
            size_t off = (size_t)value;
            if (!longnames || off >= longnames_len) die("bad extended filename offset");
            const char *name_end = memchr(longnames + off, '\n', longnames_len - off);
            if (!name_end || name_end == longnames + off || name_end[-1] != '/')
                die("bad extended filename");
            size_t nlen = (size_t)(name_end - (longnames + off) - 1);
            if (memchr(longnames + off, '/', nlen))
                die("archive member paths are unsupported");
            m->name = xcalloc(nlen + 1, 1);
            memcpy(m->name, longnames + off, nlen);
        } else {
            if (!memcmp(hdr.ar_name, "#1/", 3)) die("BSD extended filenames are unsupported");
            if (memchr(hdr.ar_name, 0, sizeof hdr.ar_name)) die("invalid archive member name");
            char namebuf[17];
            memcpy(namebuf, hdr.ar_name, 16); namebuf[16] = '\0';
            char *slash = strchr(namebuf, '/');
            if (slash) *slash = '\0';
            for (int k = strlen(namebuf) - 1; k >= 0 && isspace((unsigned char)namebuf[k]); k--) namebuf[k] = '\0';
            m->name = xstrdup(namebuf);
        }
        if (!*m->name || !strcmp(m->name, ".") || !strcmp(m->name, "..") ||
            strchr(m->name, '\n') || strchr(m->name, '\r'))
            die("invalid archive member name");
        if (*nmembers >= cap) {
            cap *= 2;
            members = xrealloc(members, cap * sizeof(Member *));
        }
        members[(*nmembers)++] = m;
    }
    if (ferror(fp) || fclose(fp) != 0) die("`%s': archive read error", ar_path);
    if (longnames) free(longnames);
    return members;
}

typedef struct { const char *name; size_t member; } IndexSymbol;

static int member_range(const Member *m, uint64_t offset, uint64_t size) {
    return offset <= m->size && size <= m->size - offset;
}

static Elf64_Shdr member_section(const Member *m, const Elf64_Ehdr *eh, size_t index) {
    Elf64_Shdr sh;
    memcpy(&sh, m->data + eh->e_shoff + index * sizeof sh, sizeof sh);
    return sh;
}

static void build_symbol_table(Member **members, size_t nmembers, uint8_t **sym_data_out, size_t *sym_data_sz) {
    IndexSymbol *symbols = NULL;
    size_t nsyms = 0, capacity = 0, strtab_sz = 0;
    for (size_t i = 0; i < nmembers; i++) {
        Member *m = members[i];
        if (m->size < SELFMAG || memcmp(m->data, ELFMAG, SELFMAG)) continue;
        if (m->size < sizeof(Elf64_Ehdr)) die("%s: truncated ELF object", m->name);
        Elf64_Ehdr eh;
        memcpy(&eh, m->data, sizeof eh);
        if (eh.e_ident[EI_CLASS] != ELFCLASS64 || eh.e_ident[EI_DATA] != ELFDATA2LSB ||
            eh.e_ident[EI_VERSION] != EV_CURRENT || eh.e_version != EV_CURRENT ||
            eh.e_machine != EM_X86_64 || eh.e_type != ET_REL || !eh.e_shoff || !eh.e_shnum ||
            eh.e_shentsize != sizeof(Elf64_Shdr) ||
            !member_range(m, eh.e_shoff, (uint64_t)eh.e_shnum * sizeof(Elf64_Shdr)))
            die("%s: unsupported or malformed ELF object", m->name);
        for (size_t s = 0; s < eh.e_shnum; s++) {
            Elf64_Shdr sh = member_section(m, &eh, s);
            if (sh.sh_type != SHT_SYMTAB) continue;
            if (sh.sh_link >= eh.e_shnum || sh.sh_entsize != sizeof(Elf64_Sym) ||
                sh.sh_size % sizeof(Elf64_Sym) || sh.sh_info > sh.sh_size / sizeof(Elf64_Sym) ||
                !member_range(m, sh.sh_offset, sh.sh_size))
                die("%s: invalid ELF symbol table", m->name);
            Elf64_Shdr strings = member_section(m, &eh, sh.sh_link);
            if (strings.sh_type != SHT_STRTAB || !strings.sh_size ||
                !member_range(m, strings.sh_offset, strings.sh_size) ||
                m->data[strings.sh_offset + strings.sh_size - 1])
                die("%s: invalid ELF string table", m->name);
            const char *strtab = (const char *)m->data + strings.sh_offset;
            size_t count = (size_t)(sh.sh_size / sizeof(Elf64_Sym));
            for (size_t k = sh.sh_info; k < count; k++) {
                Elf64_Sym sym;
                memcpy(&sym, m->data + sh.sh_offset + k * sizeof sym, sizeof sym);
                if (sym.st_name >= strings.sh_size) die("%s: invalid ELF symbol name", m->name);
                if (sym.st_shndx == SHN_XINDEX) die("%s: extended ELF symbol indices are unsupported", m->name);
                if (sym.st_shndx == SHN_UNDEF || ELF64_ST_BIND(sym.st_info) == STB_LOCAL ||
                    !sym.st_name || !strtab[sym.st_name]) continue;
                size_t len = strlen(strtab + sym.st_name) + 1;
                if (nsyms == UINT32_MAX || len > UINT32_MAX || strtab_sz > UINT32_MAX - len)
                    die("archive symbol table is too large");
                if (nsyms == capacity) {
                    size_t next = capacity ? capacity * 2 : 64;
                    if (next < capacity || next > SIZE_MAX / sizeof *symbols) die("too many symbols");
                    symbols = xrealloc(symbols, next * sizeof *symbols);
                    capacity = next;
                }
                symbols[nsyms++] = (IndexSymbol){strtab + sym.st_name, i};
                strtab_sz += len;
            }
        }
    }
    if (nsyms > (UINT32_MAX - 4u) / 4u ||
        strtab_sz > UINT32_MAX - 4u - nsyms * 4u)
        die("archive symbol table is too large");
    size_t total_sz = 4 + nsyms * 4 + strtab_sz;
    uint8_t *buf = xcalloc(total_sz, 1);
    store_be32(buf, (uint32_t)nsyms);
    uint64_t current = SARMAG + sizeof(ArHdr) + total_sz + (total_sz & 1);
    size_t longnames_sz = 0;
    for (size_t i = 0; i < nmembers; i++) {
        size_t len = strlen(members[i]->name);
        if (len >= 16) {
            if (len > UINT32_MAX - 2 || longnames_sz > UINT32_MAX - len - 2)
                die("archive filename table is too large");
            longnames_sz += len + 2;
        }
    }
    if (longnames_sz) current += sizeof(ArHdr) + longnames_sz + (longnames_sz & 1);
    size_t sym_idx = 0;
    char *str_ptr = (char *)buf + 4 + nsyms * 4;
    for (size_t i = 0; i < nmembers; i++) {
        if (current > UINT32_MAX || members[i]->size > UINT32_MAX ||
            current + sizeof(ArHdr) + members[i]->size + (members[i]->size & 1) > UINT32_MAX)
            die("archive is too large for a 32-bit symbol index");
        while (sym_idx < nsyms && symbols[sym_idx].member == i) {
            store_be32(buf + 4 + sym_idx * 4, (uint32_t)current);
            size_t len = strlen(symbols[sym_idx].name) + 1;
            memcpy(str_ptr, symbols[sym_idx].name, len);
            str_ptr += len;
            sym_idx++;
        }
        current += sizeof(ArHdr) + members[i]->size + (members[i]->size & 1);
    }
    free(symbols);
    *sym_data_out = buf;
    *sym_data_sz = total_sz;
}

static void write_archive(const char *ar_path, Member **members, size_t nmembers) {
    size_t path_len = strlen(ar_path);
    if (path_len > SIZE_MAX - sizeof ".tmp.XXXXXX") die("archive path too long");
    char *temp = xcalloc(path_len + sizeof ".tmp.XXXXXX", 1);
    memcpy(temp, ar_path, path_len);
    memcpy(temp + path_len, ".tmp.XXXXXX", sizeof ".tmp.XXXXXX");
    struct stat old;
    int existed = stat(ar_path, &old) == 0;
    if (!existed && errno != ENOENT) die("cannot stat `%s': %s", ar_path, strerror(errno));
    int fd = create_temporary_archive(temp, path_len);
    if (fd < 0) die("cannot create temporary archive for `%s': %s", ar_path, strerror(errno));
    temporary_archive = temp;
    mode_t mask = umask(0);
    umask(mask);
    mode_t mode = existed ? old.st_mode & 0777 : 0666 & ~mask;
    if (fchmod(fd, mode) != 0) die("cannot set archive permissions: %s", strerror(errno));
    write_archive_bytes(fd, ARMAG, SARMAG, ar_path);
    if (create_symtab) {
        uint8_t *sym_data = NULL;
        size_t sym_sz = 0;
        build_symbol_table(members, nmembers, &sym_data, &sym_sz);
        ArHdr hdr; memset(&hdr, ' ', sizeof(ArHdr));
        memcpy(hdr.ar_name, "/               ", 16);
        header_number(hdr.ar_date, sizeof hdr.ar_date, 0, 0);
        header_number(hdr.ar_uid, sizeof hdr.ar_uid, 0, 0);
        header_number(hdr.ar_gid, sizeof hdr.ar_gid, 0, 0);
        header_number(hdr.ar_mode, sizeof hdr.ar_mode, 0, 1);
        header_number(hdr.ar_size, sizeof hdr.ar_size, sym_sz, 0);
        memcpy(hdr.ar_fmag, ARFMAG, 2);
        write_archive_bytes(fd, &hdr, sizeof(ArHdr), ar_path);
        write_archive_bytes(fd, sym_data, sym_sz, ar_path);
        if (sym_sz & 1) write_archive_bytes(fd, "\n", 1, ar_path);
        free(sym_data);
    }
    size_t longnames_sz = 0;
    for (size_t i = 0; i < nmembers; i++) {
        if (strlen(members[i]->name) >= 16) longnames_sz += strlen(members[i]->name) + 2;
    }
    char *longnames_buf = NULL;
    if (longnames_sz > 0) {
        longnames_buf = xcalloc(longnames_sz + 1, 1);
        char *ptr = longnames_buf;
        for (size_t i = 0; i < nmembers; i++) {
            if (strlen(members[i]->name) >= 16) {
                size_t len = strlen(members[i]->name);
                memcpy(ptr, members[i]->name, len);
                ptr[len] = '/';
                ptr[len + 1] = '\n';
                ptr += len + 2;
            }
        }
        ArHdr hdr; memset(&hdr, ' ', sizeof(ArHdr));
        memcpy(hdr.ar_name, "//              ", 16);
        header_number(hdr.ar_date, sizeof hdr.ar_date, 0, 0);
        header_number(hdr.ar_uid, sizeof hdr.ar_uid, 0, 0);
        header_number(hdr.ar_gid, sizeof hdr.ar_gid, 0, 0);
        header_number(hdr.ar_mode, sizeof hdr.ar_mode, 0, 1);
        header_number(hdr.ar_size, sizeof hdr.ar_size, longnames_sz, 0);
        memcpy(hdr.ar_fmag, ARFMAG, 2);
        write_archive_bytes(fd, &hdr, sizeof(ArHdr), ar_path);
        write_archive_bytes(fd, longnames_buf, longnames_sz, ar_path);
        if (longnames_sz & 1) write_archive_bytes(fd, "\n", 1, ar_path);
    }
    size_t long_offset = 0;
    for (size_t i = 0; i < nmembers; i++) {
        Member *m = members[i];
        ArHdr hdr; memset(&hdr, ' ', sizeof(ArHdr));
        if (strlen(m->name) >= 16) {
            hdr.ar_name[0] = '/';
            header_number(hdr.ar_name + 1, sizeof hdr.ar_name - 1, long_offset, 0);
            long_offset += strlen(m->name) + 2;
        } else {
            size_t len = strlen(m->name);
            memcpy(hdr.ar_name, m->name, len);
            hdr.ar_name[len] = '/';
        }
        header_number(hdr.ar_date, sizeof hdr.ar_date, 0, 0);
        header_number(hdr.ar_uid, sizeof hdr.ar_uid, 0, 0);
        header_number(hdr.ar_gid, sizeof hdr.ar_gid, 0, 0);
        header_number(hdr.ar_mode, sizeof hdr.ar_mode, m->mode & 07777, 1);
        header_number(hdr.ar_size, sizeof hdr.ar_size, m->size, 0);
        memcpy(hdr.ar_fmag, ARFMAG, 2);
        write_archive_bytes(fd, &hdr, sizeof(ArHdr), ar_path);
        write_archive_bytes(fd, m->data, m->size, ar_path);
        if (m->size & 1) write_archive_bytes(fd, "\n", 1, ar_path);
    }
    if (close(fd) != 0) die("write error on `%s'", ar_path);
    if (rename(temporary_archive, ar_path) != 0)
        die("cannot replace `%s': %s", ar_path, strerror(errno));
    free(temporary_archive);
    temporary_archive = NULL;
    if (longnames_buf) free(longnames_buf);
}

static void free_members(Member **members, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(members[i]->name);
        free(members[i]->data);
        free(members[i]);
    }
    free(members);
}

static const char *member_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("crunch-ar " CRUNCH_VERSION);
        return 0;
    }
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        puts("Usage: crunch-ar {d|q|r|t|x}[cvs] archive [members...]\n"
             "       crunch-ar s archive");
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "Usage: %s {d|q|r|t|x}[cvs] archive [members...]\n", prog_name);
        return 1;
    }
    const char *key = argv[1];
    char op = 0;
    for (const char *p = key; *p; p++) {
        if (*p == '-') continue;
        if (*p == 'v') verbose = 1;
        else if (*p == 's') create_symtab = 1;
        else if (*p == 'c') {}
        else if (strchr("drtxq", *p)) {
            if (op && op != *p) die("multiple archive operations specified");
            op = *p;
        }
        else die("invalid option `%c'", *p);
    }
    if (!op && create_symtab) op = 's';
    const char *ar_path = argv[2];
    char **files = &argv[3];
    int nfiles = argc - 3;
    if (op == 's' && nfiles) die("symbol indexing does not accept member operands");
    if ((op == 't' || op == 'x') && create_symtab)
        die("symbol indexing cannot be combined with listing or extraction");
    switch (op) {
        case 't': {
            size_t n = 0;
            Member **m = read_archive(ar_path, &n, 0);
            for (size_t i = 0; i < n; i++) {
                int match = nfiles == 0;
                for (int j = 0; j < nfiles; j++) if (!strcmp(m[i]->name, member_basename(files[j]))) match = 1;
                if (match && verbose) {
                    char mode[7];
                    unsigned value = (unsigned)(m[i]->mode & 07777);
                    for (int k = 5; k >= 0; k--) { mode[k] = '0' + value % 8; value /= 8; }
                    mode[6] = '\0';
                    printf("%s %u/%u %zu %lld %s\n", mode,
                           (unsigned)m[i]->uid, (unsigned)m[i]->gid, m[i]->size,
                           (long long)m[i]->mtime, m[i]->name);
                } else if (match) printf("%s\n", m[i]->name);
            }
            if (fflush(stdout) != 0) die("error writing archive contents");
            free_members(m, n);
            break;
        }
        case 'x': {
            size_t n = 0;
            Member **m = read_archive(ar_path, &n, 0);
            int *found = xcalloc((size_t)nfiles, sizeof *found);
            for (size_t i = 0; i < n; i++) {
                int match = (nfiles == 0);
                for (int j = 0; j < nfiles; j++) if (!strcmp(m[i]->name, member_basename(files[j]))) {
                    match = 1; found[j] = 1;
                }
                if (!match) continue;
                if (!*m[i]->name || strchr(m[i]->name, '/') || !strcmp(m[i]->name, ".") ||
                    !strcmp(m[i]->name, ".."))
                    die("unsafe archive member name `%s'", m[i]->name);
                FILE *fp = fopen(m[i]->name, "wb");
                if (!fp) die("cannot extract `%s': %s", m[i]->name, strerror(errno));
                int failed = fwrite(m[i]->data, 1, m[i]->size, fp) != m[i]->size;
                if (fclose(fp) != 0) failed = 1;
                if (failed || chmod(m[i]->name, m[i]->mode & 0777) != 0)
                    die("cannot finish extracting `%s': %s", m[i]->name, strerror(errno));
                if (verbose) printf("x - %s\n", m[i]->name);
            }
            free_members(m, n);
            int missing = 0;
            for (int j = 0; j < nfiles; j++) if (!found[j]) {
                fprintf(stderr, "%s: no entry `%s' in archive\n", prog_name, files[j]);
                missing = 1;
            }
            free(found);
            if (missing) return 1;
            break;
        }
        case 'r':
        case 'q': {
            size_t nmembers = 0;
            Member **members = read_archive(ar_path, &nmembers, 1);
            create_symtab = 1;
            for (int j = 0; j < nfiles; j++) {
                struct stat st;
                size_t sz = 0;
                uint8_t *data = read_file(files[j], &sz, &st);
                const char *base = member_basename(files[j]);
                if (!*base) die("empty archive member name");
                if (strchr(base, '\n') || strchr(base, '\r'))
                    die("archive member name contains a newline");
                int found = 0;
                for (size_t i = 0; op == 'r' && i < nmembers; i++) {
                    if (strcmp(members[i]->name, base) == 0) {
                        free(members[i]->data);
                        members[i]->data = data;
                        members[i]->size = sz;
                        members[i]->mtime = st.st_mtime;
                        members[i]->uid = st.st_uid;
                        members[i]->gid = st.st_gid;
                        members[i]->mode = st.st_mode;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    Member *m = xcalloc(1, sizeof(Member));
                    m->name = xstrdup(base);
                    m->data = data; m->size = sz;
                    m->mtime = st.st_mtime; m->uid = st.st_uid; m->gid = st.st_gid; m->mode = st.st_mode;
                    if (nmembers == SIZE_MAX / sizeof(Member *)) die("too many archive members");
                    members = xrealloc(members, (nmembers + 1) * sizeof(Member *));
                    members[nmembers++] = m;
                }
                if (verbose) printf("%c - %s\n", found ? 'r' : 'a', base);
            }
            write_archive(ar_path, members, nmembers);
            free_members(members, nmembers);
            break;
        }
        case 'd':
        case 's': {
            size_t nmembers = 0;
            Member **members = read_archive(ar_path, &nmembers, 0);
            create_symtab = 1;
            int *matched = xcalloc((size_t)nfiles, sizeof *matched);
            size_t new_count = 0;
            for (size_t i = 0; i < nmembers; i++) {
                int match = 0;
                for (int j = 0; j < nfiles; j++)
                    if (!matched[j] && !strcmp(members[i]->name, member_basename(files[j]))) {
                        matched[j] = 1; match = 1; break;
                    }
                if (match) {
                    if (verbose) printf("d - %s\n", members[i]->name);
                    free(members[i]->name);
                    free(members[i]->data);
                    free(members[i]);
                } else {
                    members[new_count++] = members[i];
                }
            }
            free(matched);
            write_archive(ar_path, members, new_count);
            free_members(members, new_count);
            break;
        }
        default:
            die("no valid archive operation specified");
    }
    return 0;
}