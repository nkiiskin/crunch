#ifndef CRUNCH_ELF_H
#define CRUNCH_ELF_H

/* Linux x86-64 ELF definitions used by the native tools. */
#include <stdint.h>

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

#define EI_NIDENT 16
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type, e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff, e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word sh_name, sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link, sh_info;
    Elf64_Xword sh_addralign, sh_entsize;
} Elf64_Shdr;

typedef struct {
    Elf64_Word p_type, p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr, p_paddr;
    Elf64_Xword p_filesz, p_memsz, p_align;
} Elf64_Phdr;

typedef struct {
    Elf64_Word st_name;
    unsigned char st_info, st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr d_ptr;
    } d_un;
} Elf64_Dyn;

#define ELFMAG "\x7f" "ELF"
#define SELFMAG 4
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFOSABI_NONE 0
#define ELFOSABI_SYSV 0
#define EV_CURRENT 1
#define EM_X86_64 62
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_DYNSYM 11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP 17
#define SHT_X86_64_UNWIND 0x70000001
#define SHT_GNU_versym 0x6fffffff
#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MERGE 0x10
#define SHF_STRINGS 0x20
#define SHF_INFO_LINK 0x40
#define SHF_GROUP 0x200
#define SHF_TLS 0x400
#define SHF_COMPRESSED 0x800
#define SHF_GNU_RETAIN 0x200000
#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_XINDEX 0xffff
#define GRP_COMDAT 1

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_GNU_UNIQUE 10
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_TLS 6
#define STT_GNU_IFUNC 10
#define STV_DEFAULT 0
#define STV_INTERNAL 1
#define STV_HIDDEN 2
#define STV_PROTECTED 3
#define ELF64_ST_BIND(info) ((info) >> 4)
#define ELF64_ST_TYPE(info) ((info) & 15)
#define ELF64_ST_INFO(bind, type) (((bind) << 4) + ((type) & 15))
#define ELF64_ST_VISIBILITY(other) ((other) & 3)
#define ELF64_R_SYM(info) ((info) >> 32)
#define ELF64_R_TYPE(info) ((info) & 0xffffffffUL)
#define ELF64_R_INFO(symbol, type) (((Elf64_Xword)(symbol) << 32) + (type))

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_PHDR 6
#define PT_GNU_STACK 0x6474e551
#define PT_GNU_RELRO 0x6474e552
#define PF_X 1
#define PF_W 2
#define PF_R 4

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_SONAME 14
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH 29
#define DT_FLAGS_1 0x6ffffffb
#define DF_1_PIE 0x08000000

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_PLT32 4
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_16 12
#define R_X86_64_PC16 13
#define R_X86_64_8 14
#define R_X86_64_PC8 15
#define R_X86_64_DTPMOD64 16
#define R_X86_64_DTPOFF64 17
#define R_X86_64_TPOFF64 18
#define R_X86_64_TLSGD 19
#define R_X86_64_TLSLD 20
#define R_X86_64_DTPOFF32 21
#define R_X86_64_GOTTPOFF 22
#define R_X86_64_TPOFF32 23
#define R_X86_64_PC64 24
#define R_X86_64_GOTPC32_TLSDESC 34
#define R_X86_64_TLSDESC_CALL 35
#define R_X86_64_GOTPCRELX 41
#define R_X86_64_REX_GOTPCRELX 42

#endif
