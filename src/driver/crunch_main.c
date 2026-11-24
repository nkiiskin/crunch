#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "version.h"

#ifndef CRUNCH_AS_PATH
#define CRUNCH_AS_PATH "/usr/bin/as"
#endif

/* Runtime directories are unset in a build tree, where libraries sit beside the tools. */
#ifndef CRUNCH_LIB_DIR
#define CRUNCH_LIB_DIR NULL
#endif
#ifndef CRUNCH_SHARED_LIB_DIR
#define CRUNCH_SHARED_LIB_DIR CRUNCH_LIB_DIR
#endif

#ifndef CRUNCH_DEFAULT_LIBRARY_PATH
#define CRUNCH_DEFAULT_LIBRARY_PATH "/usr/local/lib"
#endif

#ifndef CRUNCH_LD_PATH
#define CRUNCH_LD_PATH "/usr/bin/ld"
#endif

#ifndef CRUNCH_AR_PATH
#define CRUNCH_AR_PATH "/usr/bin/ar"
#endif

#ifndef CRUNCH_DEFAULT_CONF_FILE
#define CRUNCH_DEFAULT_CONF_FILE "/etc/crunch/crunch.conf"
#endif

#ifndef CRUNCH_DEFAULT_LINKER
#define CRUNCH_DEFAULT_LINKER "gnu"
#endif

#ifndef CRUNCH_DEFAULT_ASSEMBLER
#define CRUNCH_DEFAULT_ASSEMBLER "gnu"
#endif

#ifndef CRUNCH_DEFAULT_ARCHIVER
#define CRUNCH_DEFAULT_ARCHIVER "crunch"
#endif

enum { MAX_STAGES = 5 };

static char **process_environment;

/**
 * @brief Looks up a variable in the unchanged process environment.
 * @param name Environment variable name.
 * @return Value within the environment, or NULL when absent.
 */
static const char *environment_value(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; process_environment && process_environment[i]; ++i)
        if (!strncmp(process_environment[i], name, length) &&
            process_environment[i][length] == '=')
            return process_environment[i] + length + 1;
    return NULL;
}

typedef enum {
    STOP_PREPROCESS,
    STOP_IR,
    STOP_ASSEMBLY,
    STOP_OBJECT,
    STOP_EXECUTABLE
} StopStage;

typedef struct {
    const char **argv;
    size_t argc;
    size_t capacity;
    int failed;
} Stage;

typedef struct {
    StopStage stop;
    const char **inputs;
    unsigned char *library_inputs;
    size_t input_count;
    const char **library_dirs;
    size_t library_dir_count;
    const char **runtime_paths;
    size_t runtime_path_count;
    const char *output;
    const char **include_dirs;
    size_t include_dir_count;
    char **system_include_dirs;
    size_t system_include_count;
    const char **defines;
    size_t define_count;
    int verbose;
    int use_crunch_as;
    int c11_numeric;
    int archive;
    int dynamic_link;
    int static_link;
    int glibc;
    int pie;
    int relro;
    int warn_abi_mismatch;
    int warn_unused_variable;
    int warn_case_fall_through;
} Options;

typedef struct {
    int native_as;
    int native_ld;
    char *assembler_path;
    char *linker_path;
    char *archiver_path;
    char *library_path;
} ToolConfig;

/**
 * @brief Appends a validated colon-separated configuration path list.
 * @param list Allocated path list to extend, or NULL initially.
 * @param value Nonempty path list to append.
 * @return One on success, zero after reporting an allocation failure.
 */
static int append_config_list(char **list, const char *value)
{
    size_t old = *list ? strlen(*list) : 0, added = strlen(value);
    if (added > SIZE_MAX - old - 2) {
        fprintf(stderr, "crunch: configuration path is too long\n");
        return 0;
    }
    char *joined = realloc(*list, old + (old ? 1 : 0) + added + 1);
    if (!joined) {
        fprintf(stderr, "crunch: out of memory\n");
        return 0;
    }
    if (old) joined[old++] = ':';
    memcpy(joined + old, value, added + 1);
    *list = joined;
    return 1;
}

/**
 * @brief Removes surrounding whitespace from a configuration field.
 * @param text Mutable null-terminated field.
 * @return Pointer to the trimmed field within text.
 */
static char *trim_config(char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

/**
 * @brief Reads driver tool choices from the common configuration file.
 * @param config Output selections and allocated executable paths.
 * @return One on success, including an absent file; zero on error.
 */
static int load_tool_config(ToolConfig *config)
{
    const char *path = environment_value("CRUNCH_CONF_FILE");
    if (!path) path = CRUNCH_DEFAULT_CONF_FILE;
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno == ENOENT) return 1;
        fprintf(stderr, "crunch: cannot read configuration file '%s': %s\n", path, strerror(errno));
        return 0;
    }
    char *line = NULL;
    size_t capacity = 0;
    unsigned long number = 0;
    int ok = 1;
    while (ok && getline(&line, &capacity, file) != -1) {
        ++number;
        char *key = trim_config(line);
        if (!*key || *key == '#') continue;
        char *equals = strchr(key, '=');
        if (!equals) {
            fprintf(stderr, "crunch: %s:%lu: expected 'key = value'\n", path, number);
            ok = 0;
            break;
        }
        *equals = '\0';
        key = trim_config(key);
        char *value = trim_config(equals + 1);
        if (!strcmp(key, "assembler") || !strcmp(key, "linker")) {
            if (strcmp(value, "gnu") && strcmp(value, "crunch")) {
                fprintf(stderr, "crunch: %s:%lu: %s must be gnu or crunch\n", path, number, key);
                ok = 0;
            } else if (!strcmp(key, "assembler")) {
                config->native_as = !strcmp(value, "crunch");
            } else {
                config->native_ld = !strcmp(value, "crunch");
            }
        } else if (!strcmp(key, "assembler_path") || !strcmp(key, "linker_path") ||
                   !strcmp(key, "archiver_path")) {
            if (value[0] != '/') {
                fprintf(stderr, "crunch: %s:%lu: %s must be an absolute path\n", path, number, key);
                ok = 0;
            } else {
                char **target = !strcmp(key, "assembler_path") ? &config->assembler_path :
                                !strcmp(key, "linker_path") ? &config->linker_path :
                                &config->archiver_path;
                char *copy = strdup(value);
                if (!copy) {
                    fprintf(stderr, "crunch: out of memory\n");
                    ok = 0;
                } else {
                    free(*target);
                    *target = copy;
                }
            }
        } else if (!strcmp(key, "include_path") || !strcmp(key, "library_path")) {
            size_t length = strlen(value);
            if (!length || value[0] == ':' || value[length - 1] == ':' || strstr(value, "::")) {
                fprintf(stderr, "crunch: %s:%lu: empty directory in %s\n", path, number, key);
                ok = 0;
            } else if (!strcmp(key, "library_path")) {
                ok = append_config_list(&config->library_path, value);
            }
        } else {
            fprintf(stderr, "crunch: %s:%lu: unknown key '%s'\n", path, number, key);
            ok = 0;
        }
    }
    if (ok && ferror(file)) {
        fprintf(stderr, "crunch: failed reading '%s'\n", path);
        ok = 0;
    }
    free(line);
    if (fclose(file) != 0 && ok) {
        fprintf(stderr, "crunch: failed closing '%s': %s\n", path, strerror(errno));
        ok = 0;
    }
    return ok;
}

/**
 * @brief Prints command-line usage and option descriptions.
 * @param stream Stream receiving the help text.
 * @return No value; writes a single usage message to stream.
 */
static void usage(FILE *stream)
{
    fputs("usage: crunch [options] source-assembly-object-or-archive-file...\n"
          "  -o FILE         output file ('-' for standard output in intermediate modes)\n"
          "  -I DIR          add a header and assembly include directory (repeatable)\n"
          "  -L DIR          add a library search directory (repeatable)\n"
          "  -l NAME         link a library at this position (repeatable)\n"
          "  -D NAME[=VALUE] define a preprocessor macro (repeatable)\n"
          "  -E              stop after preprocessing\n"
          "  --emit-ir       stop after generating Crunch IR\n"
          "  --c11-numeric   compatibility option (C numeric rules are the default)\n"
          "  -S              stop after generating assembly\n"
          "  -c              produce one combined relocatable object\n"
          "  --archive       create a static archive from .o inputs (requires -o)\n"
          "  --dynamic       link an executable with shared libcrunch (default)\n"
          "  --static        link an executable with static libcrunch\n"
          "  --glibc         compile for glibc and use system C startup and linking\n"
          "  -Wabi-mismatch  warn about visible cross-object ABI differences\n"
          "  -Wunused-variable  warn about unused local variables\n"
          "  -Wcase-fall-through  warn about switch case fall-through\n"
          "  -Wall           enable all optional warnings\n"
          "  -pie, --pie     create a position-independent dynamic executable\n"
          "  --rpath PATH    add a runtime search path for dynamic output (repeatable)\n"
          "  -z relro        protect relocation data in dynamic output\n"
          "  --with-crunch-as  use the experimental Crunch assembler\n"
          "  -v              print the commands that are run\n"
          "  -h, --help      show this help\n"
          "  --version       show the version and build information\n"
          "GNU assembly inputs use .s or preprocessed .S; .o and .a inputs are linked.\n"
          "With an assembly input,\n"
          "an output ending in .o selects relocatable output unless an earlier stage is\n"
          "requested. Executables require a defined _start symbol.\n"
          "Default output is a.out; -E, --emit-ir, and -S write to standard\n"
          "output unless -o is given; -c writes a.o (or source.o with --glibc).\n"
          "Intermediate-output\n"
          "modes accept one Crunch source; -E also accepts one .S file.\n", stream);
}

/**
 * @brief Prints the configured tools used by the driver's default modes.
 * @return No value; writes selections to stdout and configuration errors to stderr.
 */
static void print_help_tools(void)
{
    ToolConfig config = {
        .native_as = strcmp(CRUNCH_DEFAULT_ASSEMBLER, "crunch") == 0,
        .native_ld = strcmp(CRUNCH_DEFAULT_LINKER, "crunch") == 0
    };
    if (load_tool_config(&config)) {
        printf("\nTools:\n"
               "  assembler: %s", config.native_as ? "crunch" : "gnu");
        if (config.assembler_path) printf(" (%s)", config.assembler_path);
        printf("\n  linker:    crunch (dynamic executables");
        if (config.native_ld && config.linker_path) printf(": %s", config.linker_path);
        printf(")\n             %s (static and relocatable", config.native_ld ? "crunch" : "gnu");
        if (config.linker_path) printf(": %s", config.linker_path);
        printf(")\n  archiver:  ");
        if (config.archiver_path) printf("%s (configured path)\n", config.archiver_path);
        else printf("%s\n", CRUNCH_DEFAULT_ARCHIVER);
    } else {
        fputs("\nTools: unavailable (configuration error)\n", stdout);
    }
    free(config.assembler_path);
    free(config.linker_path);
    free(config.archiver_path);
    free(config.library_path);
}

static int is_shared_input(const char *path);
static int is_object_input(const char *path);
static int is_assembly_input(const char *path);

/**
 * @brief Prints the compiler version, build time, and host information.
 * @note This function takes no arguments.
 * @return No value; writes version information to standard output.
 */
static void print_version(void)
{
    struct utsname host;
    printf("crunch %s\nbuilt %s %s\n", CRUNCH_VERSION, __DATE__, __TIME__);
    if (uname(&host) == 0) printf("%s %s\n", host.sysname, host.machine);
}

/**
 * @brief Appends one argument to a compiler pipeline stage.
 * @param stage Stage whose null-terminated argument vector is extended.
 * @param argument Argument string to append; it remains owned by the caller.
 * @return No value; records allocation failure on the stage after reporting it.
 */
static void stage_add(Stage *stage, const char *argument)
{
    if (stage->failed) return;
    if (stage->argc + 1 >= stage->capacity) {
        size_t capacity = stage->capacity ? stage->capacity * 2 : 16;
        if (capacity < stage->capacity || capacity > SIZE_MAX / sizeof *stage->argv) {
            fprintf(stderr, "crunch: too many command-line arguments\n");
            stage->failed = 1;
            return;
        }
        const char **argv = realloc(stage->argv, capacity * sizeof *argv);
        if (!argv) {
            fprintf(stderr, "crunch: out of memory\n");
            stage->failed = 1;
            return;
        }
        stage->argv = argv;
        stage->capacity = capacity;
    }
    stage->argv[stage->argc++] = argument;
    stage->argv[stage->argc] = NULL;
}

/**
 * @brief Prints a pipeline stage command for verbose diagnostics.
 * @param stage Stage whose argument vector is displayed.
 * @return No value; writes the command to standard error.
 */
static void print_command(const Stage *stage)
{
    fputs("crunch:", stderr);
    for (size_t i = 0; i < stage->argc; ++i) fprintf(stderr, " %s", stage->argv[i]);
    fputc('\n', stderr);
}

/* Directory containing this executable; the stage tools are installed beside it. */
/**
 * @brief Resolves the directory containing the currently running executable.
 * @param buffer Writable buffer receiving the directory path.
 * @param size Capacity of buffer, including the terminating NUL.
 * @return One on success, or zero if the executable path cannot be resolved.
 */
static int own_directory(char *buffer, size_t size)
{
    ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    if (length <= 0) return 0;
    buffer[length] = '\0';
    char *slash = strrchr(buffer, '/');
    if (!slash) return 0;
    *slash = '\0';
    return 1;
}

/**
 * @brief Interprets a wait status and identifies SIGPIPE termination.
 * @param status Status value returned by waitpid.
 * @param sigpipe_only Output set when the stage terminated specifically by SIGPIPE.
 * @return Nonzero if the stage failed or was signaled; otherwise zero.
 */
static int stage_status_failed(int status, int *sigpipe_only)
{
    *sigpipe_only = 0;
    if (WIFEXITED(status)) return WEXITSTATUS(status) != 0;
    if (WIFSIGNALED(status)) {
        *sigpipe_only = WTERMSIG(status) == SIGPIPE;
        return 1;
    }
    return 1;
}

/**
 * @brief Copies captured stage diagnostics to standard error.
 * @param capture Temporary stream containing the captured diagnostics.
 * @return No value; rewinds capture and writes its contents to stderr.
 */
static void replay_stderr(FILE *capture)
{
    char buffer[4096];
    size_t length;
    rewind(capture);
    while ((length = fread(buffer, 1, sizeof buffer, capture)) > 0)
        fwrite(buffer, 1, length, stderr);
}

/**
 * @brief Discovers the selected C compiler's ordered system header paths.
 * @param options Driver options receiving allocated directory names.
 * @param cc Absolute path to the system C compiler.
 * @return One on success, zero after reporting a discovery failure.
 */
static int discover_system_includes(Options *options, const char *cc)
{
    if (cc[0] != '/') {
        fprintf(stderr, "crunch: CRUNCH_GLIBC_CC must be an absolute path\n");
        return 0;
    }
    size_t count = 0;
    while (process_environment && process_environment[count]) ++count;
    char **env = calloc(count + 2, sizeof *env);
    FILE *capture = tmpfile();
    int null_fd = open("/dev/null", O_RDWR, 0);
    if (!env || !capture || null_fd < 0) {
        fprintf(stderr, "crunch: cannot prepare system include discovery: %s\n", strerror(errno));
        free(env);
        if (capture) fclose(capture);
        if (null_fd >= 0) close(null_fd);
        return 0;
    }
    size_t used = 0;
    for (size_t i = 0; i < count; ++i)
        if (strncmp(process_environment[i], "LC_ALL=", 7))
            env[used++] = process_environment[i];
    env[used] = "LC_ALL=C";
    char *args[] = {(char *)cc, "-E", "-x", "c", "-", "-v", NULL};
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        if (dup2(null_fd, STDIN_FILENO) < 0 || dup2(null_fd, STDOUT_FILENO) < 0 ||
            dup2(fileno(capture), STDERR_FILENO) < 0)
            _exit(127);
        close(null_fd);
        execve(cc, args, env);
        fprintf(stderr, "crunch: cannot run '%s': %s\n", cc, strerror(errno));
        _exit(127);
    }
    close(null_fd);
    free(env);
    if (pid < 0) {
        fprintf(stderr, "crunch: cannot start system include discovery: %s\n", strerror(errno));
        fclose(capture);
        return 0;
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "crunch: system include discovery failed for '%s'\n", cc);
        replay_stderr(capture);
        fclose(capture);
        return 0;
    }
    rewind(capture);
    char line[PATH_MAX + 128];
    int inside = 0, finished = 0, valid = 1;
    while (fgets(line, sizeof line, capture)) {
        size_t length = strlen(line);
        if (length && line[length - 1] != '\n' && !feof(capture)) {
            if (inside) { valid = 0; break; }
            int ch;
            while ((ch = fgetc(capture)) != '\n' && ch != EOF) {}
            continue;
        }
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (!inside) {
            if (!strcmp(line, "#include <...> search starts here:")) inside = 1;
            continue;
        }
        if (!strcmp(line, "End of search list.")) {
            finished = 1;
            break;
        }
        if (line[0] != ' ' || !line[1] ||
            options->system_include_count >= SIZE_MAX / sizeof *options->system_include_dirs) {
            valid = 0;
            break;
        }
        char *path = strdup(line + 1);
        char **grown = path ? realloc(options->system_include_dirs,
                                      (options->system_include_count + 1) * sizeof *grown) : NULL;
        if (!path || !grown) {
            free(path);
            valid = 0;
            break;
        }
        options->system_include_dirs = grown;
        options->system_include_dirs[options->system_include_count++] = path;
    }
    if (ferror(capture)) valid = 0;
    fclose(capture);
    if (!valid || !finished || !options->system_include_count) {
        fprintf(stderr, "crunch: cannot parse system include paths from '%s'\n", cc);
        return 0;
    }
    return 1;
}

/** @brief Runs compiler stages connected by pipes.
   The final stage writes to out_fd, or to the
   inherited standard output when out_fd is negative. Diagnostics of a stage are
   shown only if every earlier stage succeeded, so a failing stage does not
   produce misleading follow-up errors from stages fed its truncated output.
   @param stages Pipeline stages, in execution order.
   @param count Number of stages in the pipeline.
   @param out_fd Final-stage output descriptor, or a negative value for stdout.
   @param verbose Whether each executed command is printed.
   @return Zero on success; nonzero if setup or any stage fails. */
static int run_pipeline(Stage *stages, int count, int out_fd, int verbose)
{
    pid_t pids[MAX_STAGES] = {0};
    FILE *captured[MAX_STAGES] = {0};
    int previous_read = -1, started = 0, failed = 0;

    for (int i = 0; i < count; ++i) {
        if (!stages[i].argv[0][0]) {
            fprintf(stderr, "crunch: selected GNU tool was not configured; set its path in crunch.conf\n");
            return 1;
        }
    }
    for (int i = 0; i < count; ++i) {
        if (verbose) print_command(&stages[i]);
        int fds[2] = {-1, -1};
        if (i + 1 < count && pipe(fds) != 0) {
            fprintf(stderr, "crunch: cannot create pipe: %s\n", strerror(errno));
            failed = 1;
            break;
        }
        if (i > 0) {
            captured[i] = tmpfile();
            if (!captured[i]) {
                fprintf(stderr, "crunch: cannot create temporary file: %s\n",
                        strerror(errno));
                if (fds[0] >= 0) { close(fds[0]); close(fds[1]); }
                failed = 1;
                break;
            }
        }
        fflush(NULL);
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "crunch: cannot start '%s': %s\n",
                    stages[i].argv[0], strerror(errno));
            if (fds[0] >= 0) { close(fds[0]); close(fds[1]); }
            failed = 1;
            break;
        }
        if (pid == 0) {
            if (previous_read >= 0) {
                dup2(previous_read, STDIN_FILENO);
                close(previous_read);
            }
            if (fds[1] >= 0) {
                dup2(fds[1], STDOUT_FILENO);
                close(fds[0]);
                close(fds[1]);
            } else if (out_fd >= 0) {
                dup2(out_fd, STDOUT_FILENO);
            }
            if (out_fd > STDERR_FILENO) close(out_fd);
            if (captured[i]) dup2(fileno(captured[i]), STDERR_FILENO);
            execve(stages[i].argv[0], (char *const *)stages[i].argv, process_environment);
            fprintf(stderr, "crunch: cannot run '%s': %s\n",
                    stages[i].argv[0], strerror(errno));
            _exit(127);
        }
        pids[started++] = pid;
        if (previous_read >= 0) close(previous_read);
        if (fds[1] >= 0) close(fds[1]);
        previous_read = fds[0];
    }
    if (previous_read >= 0) close(previous_read);

    int reported = 0, earlier_failed = 0;
    for (int i = 0; i < started; ++i) {
        int status = 0, sigpipe_only = 0;
        while (waitpid(pids[i], &status, 0) < 0 && errno == EINTR) {}
        if (captured[i]) {
            if (!earlier_failed) replay_stderr(captured[i]);
            fclose(captured[i]);
        }
        if (!stage_status_failed(status, &sigpipe_only)) continue;
        failed = 1;
        earlier_failed = 1;
        if (sigpipe_only || reported) continue;
        reported = 1;
        if (WIFSIGNALED(status))
            fprintf(stderr, "crunch: '%s' terminated by signal %d\n",
                    stages[i].argv[0], WTERMSIG(status));
        else
            fprintf(stderr, "crunch: '%s' failed with exit status %d\n",
                    stages[i].argv[0], WEXITSTATUS(status));
    }
    return failed;
}

/**
 * @brief Parses driver options and records the selected input and output modes.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @param options Output structure populated with parsed options.
 * @return One on success; zero after reporting invalid options or allocation failure.
 */
static int parse_options(int argc, char **argv, Options *options)
{
    options->include_dirs = calloc((size_t)argc + 1, sizeof *options->include_dirs);
    options->defines = calloc((size_t)argc + 1, sizeof *options->defines);
    options->inputs = calloc((size_t)argc + 1, sizeof *options->inputs);
    options->library_inputs = calloc((size_t)argc + 1, sizeof *options->library_inputs);
    options->library_dirs = calloc((size_t)argc + 1, sizeof *options->library_dirs);
    options->runtime_paths = calloc((size_t)argc + 1, sizeof *options->runtime_paths);
    if (!options->include_dirs || !options->defines || !options->inputs ||
        !options->library_inputs || !options->library_dirs || !options->runtime_paths) {
        fprintf(stderr, "crunch: out of memory\n");
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            usage(stdout);
            print_help_tools();
            exit(0);
        } else if (strcmp(argument, "--version") == 0) {
            print_version();
            exit(0);
        } else if (strcmp(argument, "-E") == 0) {
            options->stop = STOP_PREPROCESS;
        } else if (strcmp(argument, "--emit-ir") == 0) {
            options->stop = STOP_IR;
        } else if (strcmp(argument, "-S") == 0) {
            options->stop = STOP_ASSEMBLY;
        } else if (strcmp(argument, "-c") == 0) {
            options->stop = STOP_OBJECT;
        } else if (strcmp(argument, "--archive") == 0) {
            options->archive = 1;
        } else if (strcmp(argument, "--dynamic") == 0) {
            options->dynamic_link = 1;
        } else if (strcmp(argument, "--static") == 0) {
            options->static_link = 1;
        } else if (strcmp(argument, "--glibc") == 0) {
            options->glibc = 1;
        } else if (strcmp(argument, "-Wabi-mismatch") == 0) {
            options->warn_abi_mismatch = 1;
        } else if (strcmp(argument, "-Wunused-variable") == 0) {
            options->warn_unused_variable = 1;
        } else if (strcmp(argument, "-Wcase-fall-through") == 0) {
            options->warn_case_fall_through = 1;
        } else if (strcmp(argument, "-Wall") == 0) {
            options->warn_abi_mismatch = 1;
            options->warn_unused_variable = 1;
            options->warn_case_fall_through = 1;
        } else if (!strcmp(argument, "-pie") || !strcmp(argument, "--pie")) {
            options->pie = 1;
        } else if (!strncmp(argument, "-z", 2)) {
            const char *value = argument + 2;
            if (!*value) {
                if (i + 1 >= argc || !argv[i + 1][0] || argv[i + 1][0] == '-') {
                    fprintf(stderr, "crunch: option '-z' requires an argument\n");
                    return 0;
                }
                value = argv[++i];
            }
            if (strcmp(value, "relro")) {
                fprintf(stderr, "crunch: unsupported -z option '%s'\n", value);
                return 0;
            }
            options->relro = 1;
        } else if (!strcmp(argument, "-rpath") || !strcmp(argument, "--rpath")) {
            if (i + 1 >= argc || !argv[i + 1][0] || argv[i + 1][0] == '-') {
                fprintf(stderr, "crunch: option '%s' requires a nonempty argument\n", argument);
                return 0;
            }
            options->runtime_paths[options->runtime_path_count++] = argv[++i];
        } else if (!strncmp(argument, "-rpath=", 7) || !strncmp(argument, "--rpath=", 8)) {
            const char *path = strchr(argument, '=') + 1;
            if (!*path) {
                fprintf(stderr, "crunch: option '%s' requires a nonempty argument\n", argument);
                return 0;
            }
            options->runtime_paths[options->runtime_path_count++] = path;
        } else if (strcmp(argument, "-v") == 0) {
            options->verbose = 1;
        } else if (strcmp(argument, "--c11-numeric") == 0) {
            options->c11_numeric = 1;
        } else if (strcmp(argument, "--with-crunch-as") == 0) {
            options->use_crunch_as = 1;
        } else if (strcmp(argument, "-o") == 0 || strcmp(argument, "-I") == 0 ||
                   strcmp(argument, "-D") == 0 || strcmp(argument, "-L") == 0 ||
                   strcmp(argument, "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "crunch: option '%s' requires an argument\n", argument);
                return 0;
            }
            if ((argument[1] == 'L' || argument[1] == 'l') &&
                (!argv[i + 1][0] || argv[i + 1][0] == '-')) {
                fprintf(stderr, "crunch: option '%s' requires a nonempty argument\n", argument);
                return 0;
            }
            if (argument[1] == 'o') options->output = argv[++i];
            else if (argument[1] == 'D') options->defines[options->define_count++] = argv[++i];
            else if (argument[1] == 'L') options->library_dirs[options->library_dir_count++] = argv[++i];
            else if (argument[1] == 'l') {
                options->library_inputs[options->input_count] = 1;
                options->inputs[options->input_count++] = argv[++i];
            }
            else options->include_dirs[options->include_dir_count++] = argv[++i];
        } else if (strncmp(argument, "-o", 2) == 0 && argument[2] != '\0') {
            options->output = argument + 2;
        } else if (strncmp(argument, "-D", 2) == 0 && argument[2] != '\0') {
            options->defines[options->define_count++] = argument + 2;
        } else if (strncmp(argument, "-I", 2) == 0 && argument[2] != '\0') {
            options->include_dirs[options->include_dir_count++] = argument + 2;
        } else if (strncmp(argument, "-L", 2) == 0 && argument[2] != '\0') {
            options->library_dirs[options->library_dir_count++] = argument + 2;
        } else if (strncmp(argument, "-l", 2) == 0 && argument[2] != '\0') {
            options->library_inputs[options->input_count] = 1;
            options->inputs[options->input_count++] = argument + 2;
        } else if (argument[0] == '-' && argument[1] != '\0') {
            fprintf(stderr, "crunch: unknown option '%s'\n", argument);
            return 0;
        } else {
            options->inputs[options->input_count++] = argument;
        }
    }
    if (!options->input_count) {
        fprintf(stderr, "crunch: no input file given\n");
        return 0;
    }
    if (options->stop == STOP_EXECUTABLE && !options->archive &&
        options->output && is_object_input(options->output)) {
        for (size_t i = 0; i < options->input_count; ++i) {
            if (!options->library_inputs[i] && is_assembly_input(options->inputs[i])) {
                options->stop = STOP_OBJECT;
                break;
            }
        }
    }
    if (options->dynamic_link && options->static_link) {
        fprintf(stderr, "crunch: --dynamic and --static cannot be combined\n");
        return 0;
    }
    if (options->glibc && (options->archive || options->dynamic_link || options->static_link ||
                           options->pie || options->relro || options->runtime_path_count)) {
        fprintf(stderr, "crunch: --glibc cannot be combined with --archive, --dynamic, "
                        "--static, -pie, -z, or --rpath\n");
        return 0;
    }
    if ((options->dynamic_link || options->static_link) &&
        (options->archive || options->stop != STOP_EXECUTABLE)) {
        fprintf(stderr, "crunch: --dynamic and --static require executable output\n");
        return 0;
    }
    if (!options->static_link && !options->archive && options->stop == STOP_EXECUTABLE)
        options->dynamic_link = 1;
    if (options->runtime_path_count && !options->dynamic_link) {
        fprintf(stderr, "crunch: --rpath requires dynamic executable output\n");
        return 0;
    }
    if (options->relro && !options->dynamic_link) {
        fprintf(stderr, "crunch: -z relro requires dynamic executable output\n");
        return 0;
    }
    if (options->pie && !options->dynamic_link) {
        fprintf(stderr, "crunch: -pie requires dynamic executable output\n");
        return 0;
    }
    if (options->archive && (options->stop != STOP_EXECUTABLE || !options->output ||
                             !strcmp(options->output, "-"))) {
        fprintf(stderr, "crunch: --archive requires -o FILE and cannot be combined with stage options\n");
        return 0;
    }
    if ((options->archive || options->stop <= STOP_ASSEMBLY) && options->library_dir_count) {
        fprintf(stderr, "crunch: -L is only valid for linking\n");
        return 0;
    }
    for (size_t i = 0; i < options->input_count; ++i) {
        if (!options->library_inputs[i]) continue;
        const char *name = options->inputs[i];
        size_t length = strlen(name);
        if (options->archive || options->stop <= STOP_ASSEMBLY) {
            fprintf(stderr, "crunch: -l is only valid for linking\n");
            return 0;
        }
        if (options->glibc && options->stop != STOP_EXECUTABLE) {
            fprintf(stderr, "crunch: --glibc accepts libraries only when linking an executable\n");
            return 0;
        }
        if (!length || strchr(name, '/') ||
            (name[0] == ':' &&
             !((length > 3 && !strcmp(name + length - 2, ".a")) ||
               (options->dynamic_link && is_shared_input(name + 1))))) {
            fprintf(stderr, "crunch: invalid %slibrary name '%s'\n",
                    options->dynamic_link ? "" : "static ", name);
            return 0;
        }
    }
    if (options->stop <= STOP_ASSEMBLY && options->input_count != 1) {
        fprintf(stderr, "crunch: -E, --emit-ir, and -S accept only one source file\n");
        return 0;
    }
    if (options->stop >= STOP_OBJECT && options->output && !strcmp(options->output, "-")) {
        fprintf(stderr, "crunch: object and executable outputs cannot be standard output\n");
        return 0;
    }
    size_t stdin_count = 0;
    for (size_t i = 0; i < options->input_count; i++)
        if (!strcmp(options->inputs[i], "-"))
            stdin_count++;
    if (stdin_count > 1) {
        fprintf(stderr, "crunch: standard input may be used only once\n");
        return 0;
    }
    return 1;
}

/**
 * @brief Identifies object inputs by their filename suffix.
 * @param path Input path to examine.
 * @return Nonzero for a path ending in .o.
 */
static int is_object_input(const char *path)
{
    size_t length = strlen(path);
    return length >= 2 && !strcmp(path + length - 2, ".o");
}

/**
 * @brief Identifies static archive inputs by their filename suffix.
 * @param path Input path to examine.
 * @return Nonzero for a path ending in .a.
 */
static int is_archive_input(const char *path)
{
    size_t length = strlen(path);
    return length >= 2 && !strcmp(path + length - 2, ".a");
}

/**
 * @brief Identifies shared-library inputs by their filename suffix.
 * @param path Input path to examine.
 * @return Nonzero for a name ending in .so or a numeric version suffix.
 */
static int is_shared_input(const char *path)
{
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *suffix = strstr(name, ".so");
    if (!suffix) return 0;
    suffix += 3;
    if (!*suffix) return 1;
    if (*suffix++ != '.' || !isdigit((unsigned char)*suffix)) return 0;
    for (; *suffix; suffix++)
        if (!isdigit((unsigned char)*suffix) && *suffix != '.') return 0;
    return 1;
}

/**
 * @brief Searches one directory for a requested library file.
 * @param directory Directory bytes, not necessarily NUL-terminated.
 * @param length Number of directory bytes.
 * @param name Library name, optionally prefixed with a colon for an exact filename.
 * @param shared Nonzero to search for a shared library instead of an archive.
 * @param failed Output set on an error other than a missing candidate.
 * @return Allocated library path, or NULL if absent or on error.
 */
static char *library_candidate(const char *directory, size_t length, const char *name,
                               int shared, int *failed)
{
    const char *prefix = name[0] == ':' ? "" : "lib";
    const char *suffix = name[0] == ':' ? "" : shared ? ".so" : ".a";
    if (name[0] == ':') ++name;
    size_t n = strlen(name), p = strlen(prefix), s = strlen(suffix);
    if (n > SIZE_MAX - p - s - 5 || length > SIZE_MAX - n - p - s - 5) {
        fprintf(stderr, "crunch: library path is too long\n");
        *failed = 1;
        return NULL;
    }
    size_t size = length + n + p + s + 5;
    char *path = malloc(size);
    if (!path) {
        fprintf(stderr, "crunch: out of memory\n");
        *failed = 1;
        return NULL;
    }
    char *at = path;
    if (directory[0] != '/') { *at++ = '.'; *at++ = '/'; }
    memcpy(at, directory, length);
    at += length;
    *at++ = '/';
    memcpy(at, prefix, p);
    at += p;
    memcpy(at, name, n);
    at += n;
    memcpy(at, suffix, s + 1);
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISREG(st.st_mode)) return path;
        fprintf(stderr, "crunch: library '%s' is not a regular file\n", path);
        *failed = 1;
    } else if (errno != ENOENT && errno != ENOTDIR) {
        fprintf(stderr, "crunch: cannot inspect library '%s': %s\n", path, strerror(errno));
        *failed = 1;
    }
    free(path);
    return NULL;
}

/**
 * @brief Searches one directory, preferring shared objects for dynamic links.
 * @param directory Directory bytes, not necessarily NUL-terminated.
 * @param length Number of directory bytes.
 * @param name Library name supplied by -l.
 * @param dynamic Nonzero to prefer shared objects.
 * @param failed Output set on an error other than a missing candidate.
 * @return Allocated library path, or NULL if absent or on error.
 */
static char *library_in_directory(const char *directory, size_t length, const char *name,
                                  int dynamic, int *failed)
{
    if (dynamic && name[0] != ':') {
        char *path = library_candidate(directory, length, name, 1, failed);
        if (path || *failed) return path;
    }
    return library_candidate(directory, length, name, 0, failed);
}

/**
 * @brief Resolves a library against command-line and configured search directories.
 * @param options Parsed command-line search paths.
 * @param config Configured search path or the compiled-in default.
 * @param name Library name supplied by -l.
 * @return Allocated library path, or NULL after reporting failure.
 */
static char *find_library(const Options *options, const ToolConfig *config, const char *name)
{
    int failed = 0;
    for (size_t i = 0; i < options->library_dir_count; ++i) {
        const char *dir = options->library_dirs[i];
        char *path = library_in_directory(dir, strlen(dir), name, options->dynamic_link, &failed);
        if (path || failed) return path;
    }
    const char *dirs = config->library_path ? config->library_path : CRUNCH_DEFAULT_LIBRARY_PATH;
    while (*dirs) {
        const char *end = strchr(dirs, ':');
        size_t length = end ? (size_t)(end - dirs) : strlen(dirs);
        char *path = library_in_directory(dirs, length, name, options->dynamic_link, &failed);
        if (path || failed) return path;
        if (!end) break;
        dirs = end + 1;
    }
    fprintf(stderr, "crunch: cannot find %slibrary -l%s\n",
            options->dynamic_link ? "" : "static ", name);
    return NULL;
}

/**
 * @brief Identifies GNU assembly inputs by their filename suffix.
 * @param path Input path to examine.
 * @return Nonzero for a path ending in .s or .S.
 */
static int is_assembly_input(const char *path)
{
    size_t length = strlen(path);
    return length >= 2 && (!strcmp(path + length - 2, ".s") ||
                           !strcmp(path + length - 2, ".S"));
}

/**
 * @brief Identifies assembly inputs that require preprocessing.
 * @param path Input path to examine.
 * @return Nonzero for a path ending in .S.
 */
static int is_preprocessed_assembly(const char *path)
{
    size_t length = strlen(path);
    return length >= 2 && !strcmp(path + length - 2, ".S");
}

/**
 * @brief Rejects output paths that refer to an input file.
 * @param options Parsed input options.
 * @param output Output path to check.
 * @return Nonzero if output is safe; zero after reporting a collision.
 */
static int output_is_safe(const Options *options, const char *output)
{
    struct stat target, input;
    int target_exists = stat(output, &target) == 0;
    for (size_t i = 0; i < options->input_count; i++) {
        const char *path = options->inputs[i];
        if (!strcmp(path, "-"))
            continue;
        if (!strcmp(path, output) ||
            (target_exists && stat(path, &input) == 0 &&
             target.st_dev == input.st_dev && target.st_ino == input.st_ino)) {
            fprintf(stderr, "crunch: output '%s' is also an input file\n", output);
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Creates a private file or directory from a six-X template.
 * @param path Mutable path ending in six X characters.
 * @param directory Nonzero for a directory, zero for a file.
 * @return Zero for a directory, a descriptor for a file, or -1 on failure.
 */
static int create_private_path(char *path, int directory)
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t length = strlen(path);
    if (length < 6 || strcmp(path + length - 6, "XXXXXX")) {
        errno = EINVAL;
        return -1;
    }
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        unsigned char bytes[6];
        size_t used = 0;
        while (used < sizeof bytes) {
            ssize_t n = getrandom(bytes + used, sizeof bytes - used, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) {
                if (!n) errno = EIO;
                return -1;
            }
            used += (size_t)n;
        }
        for (size_t i = 0; i < sizeof bytes; ++i)
            path[length - 6 + i] = alphabet[bytes[i] % 62];
        int result = directory ? mkdir(path, 0700) : open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (result >= 0 || errno != EEXIST) return result;
    }
    errno = EEXIST;
    return -1;
}

/**
 * @brief Creates a named temporary object in the configured temporary directory.
 * @note This function takes no arguments.
 * @return Allocated path, or NULL after reporting an error.
 */
static char *temporary_object(void)
{
    const char *dir = environment_value("TMPDIR");
    if (!dir || !dir[0]) dir = "/tmp";
    size_t length = strlen(dir);
    if (length > SIZE_MAX - sizeof "/crunch-XXXXXX") {
        fprintf(stderr, "crunch: temporary directory path is too long\n");
        return NULL;
    }
    char *path = malloc(length + sizeof "/crunch-XXXXXX");
    if (!path) {
        fprintf(stderr, "crunch: out of memory\n");
        return NULL;
    }
    snprintf(path, length + sizeof "/crunch-XXXXXX", "%s/crunch-XXXXXX", dir);
    int fd = create_private_path(path, 0);
    if (fd < 0) {
        fprintf(stderr, "crunch: cannot create temporary file: %s\n", strerror(errno));
        free(path);
        return NULL;
    }
    close(fd);
    return path;
}

/**
 * @brief Reserves a private directory beside the requested output.
 * @param output Destination path whose parent will hold the temporary directory.
 * @param directory_length Output offset of the directory's terminating NUL.
 * @return Allocated path for the staged output, or NULL after reporting an error.
 */
static char *temporary_output(const char *output, size_t *directory_length)
{
    const char *slash = strrchr(output, '/');
    size_t prefix_length = slash ? (size_t)(slash - output) + 1 : 2;
    static const char suffix[] = ".crunch-output-XXXXXX/output";
    if (prefix_length > SIZE_MAX - sizeof suffix) {
        fprintf(stderr, "crunch: output directory path is too long\n");
        return NULL;
    }
    char *path = malloc(prefix_length + sizeof suffix);
    if (!path) {
        fprintf(stderr, "crunch: out of memory\n");
        return NULL;
    }
    if (slash) memcpy(path, output, prefix_length);
    else memcpy(path, "./", prefix_length);
    memcpy(path + prefix_length, suffix, sizeof suffix);
    *directory_length = prefix_length + sizeof ".crunch-output-XXXXXX" - 1;
    path[*directory_length] = '\0';
    if (create_private_path(path, 1) < 0) {
        fprintf(stderr, "crunch: cannot create temporary output directory for '%s': %s\n",
                output, strerror(errno));
        free(path);
        return NULL;
    }
    if (chmod(path, 0700) != 0) {
        fprintf(stderr, "crunch: cannot set temporary directory permissions for '%s': %s\n",
                output, strerror(errno));
        if (rmdir(path) != 0)
            fprintf(stderr, "crunch: cannot remove temporary output directory '%s': %s\n",
                    path, strerror(errno));
        free(path);
        return NULL;
    }
    memcpy(path + *directory_length, "/output", sizeof "/output");
    return path;
}

/**
 * @brief Opens a named intermediate output, staging regular files for replacement.
 * @param output Destination path, excluding the standard-output marker.
 * @param staged Output receiving the allocated temporary path, if one is needed.
 * @param directory_length Output receiving the temporary directory path length.
 * @return Open output descriptor, or -1 after reporting an error.
 */
static int open_intermediate_output(const char *output, char **staged,
                                    size_t *directory_length)
{
    struct stat target;
    int existing = 0;
    if (stat(output, &target) == 0) {
        if (!S_ISREG(target.st_mode)) {
            int fd = open(output, O_WRONLY | O_TRUNC, 0);
            if (fd < 0)
                fprintf(stderr, "crunch: cannot open output '%s': %s\n",
                        output, strerror(errno));
            return fd;
        }
        int fd = open(output, O_WRONLY | O_NONBLOCK, 0);
        if (fd < 0) {
            fprintf(stderr, "crunch: cannot open output '%s': %s\n", output, strerror(errno));
            return -1;
        }
        if (fstat(fd, &target) != 0) {
            fprintf(stderr, "crunch: cannot inspect output '%s': %s\n", output, strerror(errno));
            close(fd);
            return -1;
        }
        if (close(fd) != 0) {
            fprintf(stderr, "crunch: cannot close output '%s': %s\n", output, strerror(errno));
            return -1;
        }
        if (!S_ISREG(target.st_mode)) {
            fprintf(stderr, "crunch: output '%s' changed while opening it\n", output);
            return -1;
        }
        existing = 1;
    } else if (errno != ENOENT) {
        fprintf(stderr, "crunch: cannot inspect output '%s': %s\n", output, strerror(errno));
        return -1;
    }

    *staged = temporary_output(output, directory_length);
    if (!*staged) return -1;
    int fd = open(*staged, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        fprintf(stderr, "crunch: cannot create temporary output for '%s': %s\n",
                output, strerror(errno));
        return -1;
    }
    if (existing && fchmod(fd, target.st_mode & 0777) != 0) {
        fprintf(stderr, "crunch: cannot set output permissions for '%s': %s\n",
                output, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/**
 * @brief Atomically replaces a destination with a completed staged output.
 * @param staged Path of the completed temporary file.
 * @param output Destination path.
 * @return Zero on success, or one after reporting a replacement failure.
 */
static int replace_output(const char *staged, const char *output)
{
    if (rename(staged, output) == 0) return 0;
    fprintf(stderr, "crunch: cannot replace output '%s': %s\n", output, strerror(errno));
    return 1;
}

/**
 * @brief Configures the preprocessor stage for a Crunch or assembly source.
 * @param stage Stage receiving the arguments.
 * @param options Driver options supplying include paths and definitions.
 * @param input Source path or standard-input marker.
 * @param pp_path Preprocessor executable path.
 * @return No value; records argument allocation failure on the stage.
 */
static void prepare_preprocessor(Stage *stage, const Options *options,
                                 const char *input, const char *pp_path)
{
    stage_add(stage, pp_path);
    if (is_preprocessed_assembly(input)) stage_add(stage, "--assembly");
    if (options->glibc) {
        int hosted_override = 0;
        static const char hosted_name[] = "__STDC_HOSTED__";
        stage_add(stage, "--glibc-includes");
        for (size_t i = 0; i < options->define_count; ++i) {
            const char *definition = options->defines[i];
            size_t length = sizeof hosted_name - 1;
            if (!strncmp(definition, hosted_name, length) &&
                (definition[length] == '\0' || definition[length] == '=')) {
                hosted_override = 1;
                break;
            }
        }
        if (!hosted_override) {
            stage_add(stage, "-D");
            stage_add(stage, "__STDC_HOSTED__=1");
        }
    }
    for (size_t i = 0; i < options->system_include_count; ++i) {
        stage_add(stage, "--system-include");
        stage_add(stage, options->system_include_dirs[i]);
    }
    for (size_t i = 0; i < options->include_dir_count; ++i) {
        stage_add(stage, "-I");
        stage_add(stage, options->include_dirs[i]);
    }
    for (size_t i = 0; i < options->define_count; ++i) {
        stage_add(stage, "-D");
        stage_add(stage, options->defines[i]);
    }
    stage_add(stage, input);
}

/**
 * @brief Runs the requested frontend and backend stages for one source.
 * @param options Driver options shared by the source files.
 * @param input Source path or standard-input marker.
 * @param pp_path Preprocessor executable path.
 * @param ir_path IR generator executable path.
 * @param opt_path IR optimizer executable path.
 * @param asm_path Assembly generator executable path.
 * @param assembler_path Object assembler executable path.
 * @param module Nonzero to generate module IR.
 * @param object Path receiving assembled output, or NULL for an earlier stage.
 * @param out_fd Output descriptor for an earlier stage, or negative for stdout.
 * @return Zero on success, nonzero on stage failure.
 */
static int compile_source(const Options *options, const char *input, const char *pp_path,
                          const char *ir_path, const char *opt_path, const char *asm_path,
                          const char *assembler_path, int module,
                          const char *object, int out_fd)
{
    Stage stages[MAX_STAGES] = {0};
    int count = 0;
    prepare_preprocessor(&stages[count++], options, input, pp_path);
    if (options->stop >= STOP_IR) {
        stage_add(&stages[count], ir_path);
        if (module) stage_add(&stages[count], "--module");
        if (options->c11_numeric) stage_add(&stages[count], "--c11-numeric");
        if (options->warn_unused_variable) stage_add(&stages[count], "-Wunused-variable");
        if (options->warn_case_fall_through) stage_add(&stages[count], "-Wcase-fall-through");
        stage_add(&stages[count++], "/dev/stdin");
    }
    if (options->stop >= STOP_ASSEMBLY) {
        stage_add(&stages[count], opt_path);
        stage_add(&stages[count++], "/dev/stdin");
        stage_add(&stages[count], asm_path);
        if (options->glibc) stage_add(&stages[count], "--no-entry");
        stage_add(&stages[count], "--source-file");
        const char *filename = strrchr(input, '/');
        stage_add(&stages[count], !strcmp(input, "-") ? "<stdin>" : filename ? filename + 1 : input);
        stage_add(&stages[count++], "/dev/stdin");
    }
    if (object) {
        stage_add(&stages[count], assembler_path);
        stage_add(&stages[count], "-o");
        stage_add(&stages[count++], object);
    }
    int status = 0;
    for (int i = 0; i < count; i++)
        if (stages[i].failed) status = 1;
    if (!status) status = run_pipeline(stages, count, out_fd, options->verbose);
    for (int i = 0; i < count; i++) free(stages[i].argv);
    return status;
}

/**
 * @brief Optionally preprocesses and assembles an input into a temporary object.
 * @param options Driver options controlling verbose output.
 * @param input Assembly source path.
 * @param object Path receiving assembled output.
 * @param pp_path Preprocessor executable path.
 * @param assembler_path Object assembler executable path.
 * @return Zero on success, nonzero on preprocessing or assembler failure.
 */
static int assemble_source(const Options *options, const char *input, const char *object,
                           const char *pp_path, const char *assembler_path)
{
    Stage stages[2] = {0};
    int count = 0;
    if (is_preprocessed_assembly(input))
        prepare_preprocessor(&stages[count++], options, input, pp_path);
    stage_add(&stages[count], assembler_path);
    for (size_t i = 0; i < options->include_dir_count; ++i) {
        stage_add(&stages[count], "-I");
        stage_add(&stages[count], options->include_dirs[i]);
    }
    stage_add(&stages[count], "-o");
    stage_add(&stages[count], object);
    if (!is_preprocessed_assembly(input)) stage_add(&stages[count], input);
    ++count;
    int status = 0;
    for (int i = 0; i < count; ++i)
        if (stages[i].failed) status = 1;
    if (!status) status = run_pipeline(stages, count, -1, options->verbose);
    for (int i = 0; i < count; ++i) free(stages[i].argv);
    return status;
}

/**
 * @brief Parses driver options and runs the requested compiler pipeline.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @param envp Process environment vector.
 * @return Process exit status: zero for success, otherwise nonzero.
 */
int main(int argc, char **argv, char **envp)
{
    process_environment = envp;
    Options options = {.stop = STOP_EXECUTABLE};
    if (!parse_options(argc, argv, &options)) {
        usage(stderr);
        free(options.inputs);
        free(options.library_inputs);
        free(options.library_dirs);
        free(options.runtime_paths);
        free(options.include_dirs);
        free(options.defines);
        return 2;
    }

    char directory[PATH_MAX];
    if (!own_directory(directory, sizeof directory)) {
        fprintf(stderr, "crunch: cannot locate the crunch stage tools\n");
        return 1;
    }
    char pp_path[PATH_MAX + 16], ir_path[PATH_MAX + 16], opt_path[PATH_MAX + 16], asm_path[PATH_MAX + 16];
    char crunch_as_path[PATH_MAX + 16], crunch_ld_path[PATH_MAX + 16], crunch_ar_path[PATH_MAX + 16];
    snprintf(pp_path, sizeof pp_path, "%s/crunch-pp", directory);
    snprintf(ir_path, sizeof ir_path, "%s/crunch-ir", directory);
    snprintf(opt_path, sizeof opt_path, "%s/crunch-opt", directory);
    snprintf(asm_path, sizeof asm_path, "%s/crunch-asm", directory);
    snprintf(crunch_as_path, sizeof crunch_as_path, "%s/crunch-as", directory);
    snprintf(crunch_ld_path, sizeof crunch_ld_path, "%s/crunch-ld", directory);
    snprintf(crunch_ar_path, sizeof crunch_ar_path, "%s/crunch-ar", directory);

    const char *output = options.output;
    char default_object[PATH_MAX];
    int out_fd = -1, status = 1;
    char **temporary = NULL;
    char **resolved_libraries = NULL;
    char **searched_shared_names = NULL;
    char *link_search_dirs = NULL;
    char *staged_output = NULL;
    size_t output_directory_length = 0;
    ToolConfig config = {
        .native_as = strcmp(CRUNCH_DEFAULT_ASSEMBLER, "crunch") == 0,
        .native_ld = strcmp(CRUNCH_DEFAULT_LINKER, "crunch") == 0
    };
    if (!load_tool_config(&config)) goto cleanup;
    const char *assembler_path = options.use_crunch_as ? crunch_as_path :
        config.assembler_path ? config.assembler_path :
        config.native_as ? crunch_as_path : CRUNCH_AS_PATH;
    const char *linker_path = config.linker_path ? config.linker_path :
        config.native_ld ? crunch_ld_path : CRUNCH_LD_PATH;
    const char *glibc_cc = environment_value("CRUNCH_GLIBC_CC");
    if (!glibc_cc) glibc_cc = "/usr/bin/cc";
    if (options.glibc && options.stop == STOP_EXECUTABLE) {
        linker_path = glibc_cc;
        if (linker_path[0] != '/') {
            fprintf(stderr, "crunch: CRUNCH_GLIBC_CC must be an absolute path\n");
            goto cleanup;
        }
    } else if (options.dynamic_link)
        linker_path = config.native_ld && config.linker_path ? config.linker_path : crunch_ld_path;
    const char *archiver_path = config.archiver_path ? config.archiver_path :
        strcmp(CRUNCH_DEFAULT_ARCHIVER, "crunch") == 0 ? crunch_ar_path : CRUNCH_AR_PATH;

    if (options.warn_abi_mismatch && options.glibc && options.stop == STOP_EXECUTABLE) {
        fprintf(stderr, "crunch: warning: ABI mismatch checking is unavailable in glibc "
                        "link mode. Option discarded.\n");
        options.warn_abi_mismatch = 0;
    } else if (options.warn_abi_mismatch && !options.archive && options.stop >= STOP_OBJECT &&
               !options.dynamic_link && !config.native_ld) {
        fprintf(stderr, "crunch: warning: ABI mismatch checking requires Crunch's linker "
                        "(configure --enable-native-only or select linker = crunch). "
                        "Option discarded.\n");
        options.warn_abi_mismatch = 0;
    }

    if (options.archive) {
        for (size_t i = 0; i < options.input_count; ++i) {
            if (!is_object_input(options.inputs[i])) {
                fprintf(stderr, "crunch: --archive accepts only .o inputs: '%s'\n", options.inputs[i]);
                goto cleanup;
            }
            const char *name = strrchr(options.inputs[i], '/');
            name = name ? name + 1 : options.inputs[i];
            for (size_t j = 0; j < i; ++j) {
                const char *other = strrchr(options.inputs[j], '/');
                other = other ? other + 1 : options.inputs[j];
                if (!strcmp(name, other)) {
                    fprintf(stderr, "crunch: duplicate archive member name '%s'\n", name);
                    goto cleanup;
                }
            }
        }
        if (!output_is_safe(&options, output)) goto cleanup;
        staged_output = temporary_output(output, &output_directory_length);
        if (!staged_output) goto cleanup;
        Stage archive = {0};
        stage_add(&archive, archiver_path);
        stage_add(&archive, "rcs");
        stage_add(&archive, staged_output);
        for (size_t i = 0; i < options.input_count; ++i) stage_add(&archive, options.inputs[i]);
        status = archive.failed ? 1 : run_pipeline(&archive, 1, -1, options.verbose);
        free(archive.argv);
        if (!status) status = replace_output(staged_output, output);
        goto cleanup;
    }

    resolved_libraries = calloc(options.input_count, sizeof *resolved_libraries);
    searched_shared_names = calloc(options.input_count, sizeof *searched_shared_names);
    if (!resolved_libraries || !searched_shared_names) {
        fprintf(stderr, "crunch: out of memory\n");
        goto cleanup;
    }
    for (size_t i = 0; i < options.input_count; ++i) {
        if (!options.library_inputs[i]) continue;
        if (options.glibc) continue;
        resolved_libraries[i] = find_library(&options, &config, options.inputs[i]);
        if (!resolved_libraries[i]) goto cleanup;
        options.inputs[i] = resolved_libraries[i];
    }

    for (size_t i = 0; i < options.input_count; i++) {
        if (is_shared_input(options.inputs[i]) && !options.dynamic_link) {
            fprintf(stderr, "crunch: shared input '%s' requires dynamic executable output\n", options.inputs[i]);
            goto cleanup;
        }
    }
    if (options.stop <= STOP_ASSEMBLY &&
        (is_object_input(options.inputs[0]) || is_archive_input(options.inputs[0]) ||
         (is_assembly_input(options.inputs[0]) &&
          !(options.stop == STOP_PREPROCESS && is_preprocessed_assembly(options.inputs[0]))))) {
        fprintf(stderr, "crunch: intermediate-output modes require a Crunch source file"
                        " (-E also accepts .S)\n");
        goto cleanup;
    }
    if (!output && options.stop == STOP_OBJECT) {
        output = "a.o";
        if (options.glibc && options.input_count == 1 && !options.library_inputs[0] &&
            strcmp(options.inputs[0], "-") && !is_object_input(options.inputs[0]) &&
            !is_archive_input(options.inputs[0]) && !is_shared_input(options.inputs[0])) {
            const char *base = strrchr(options.inputs[0], '/');
            base = base ? base + 1 : options.inputs[0];
            const char *extension = strrchr(base, '.');
            if (extension && extension != base && strlen(base) + 2 < sizeof default_object) {
                size_t length = (size_t)(extension - base);
                memcpy(default_object, base, length);
                memcpy(default_object + length, ".o", 3);
                output = default_object;
            }
        }
    }
    if (!output && options.stop == STOP_EXECUTABLE) output = "a.out";
    if (output && strcmp(output, "-") && !output_is_safe(&options, output))
        goto cleanup;

    if (options.glibc) {
        int needs_preprocessor = 0;
        for (size_t i = 0; i < options.input_count; ++i) {
            const char *input = options.inputs[i];
            if (!options.library_inputs[i] && !is_object_input(input) &&
                !is_archive_input(input) &&
                (!is_assembly_input(input) || is_preprocessed_assembly(input))) {
                needs_preprocessor = 1;
                break;
            }
        }
        if (needs_preprocessor && !discover_system_includes(&options, glibc_cc))
            goto cleanup;
    }

    if (options.stop <= STOP_ASSEMBLY) {
        if (output && strcmp(output, "-")) {
            out_fd = open_intermediate_output(output, &staged_output, &output_directory_length);
            if (out_fd < 0) goto cleanup;
        }
        status = compile_source(&options, options.inputs[0], pp_path, ir_path, opt_path, asm_path, assembler_path,
                                options.glibc, NULL, out_fd);
        if (out_fd >= 0 && close(out_fd) != 0) {
            fprintf(stderr, "crunch: cannot close output '%s': %s\n", output, strerror(errno));
            status = 1;
        }
        if (!status && staged_output) status = replace_output(staged_output, output);
        goto cleanup;
    }

    temporary = calloc(options.input_count, sizeof *temporary);
    if (!temporary) {
        fprintf(stderr, "crunch: out of memory\n");
        goto cleanup;
    }
    int module = options.glibc || options.stop == STOP_OBJECT || options.input_count > 1;
    for (size_t i = 0; i < options.input_count; i++) {
        if (options.glibc && options.library_inputs[i]) continue;
        if (is_object_input(options.inputs[i]) || is_archive_input(options.inputs[i]) ||
            (options.dynamic_link && is_shared_input(options.inputs[i]))) continue;
        temporary[i] = temporary_object();
        if (!temporary[i]) goto cleanup;
        if (is_assembly_input(options.inputs[i])
                ? assemble_source(&options, options.inputs[i], temporary[i], pp_path, assembler_path)
                : compile_source(&options, options.inputs[i], pp_path, ir_path, opt_path, asm_path,
                                 assembler_path, module, temporary[i], -1))
            goto cleanup;
    }
    staged_output = temporary_output(output, &output_directory_length);
    if (!staged_output) goto cleanup;
    Stage link = {0};
    stage_add(&link, linker_path);
    if (options.glibc && options.stop == STOP_EXECUTABLE) {
        char support_path[PATH_MAX + 32];
        const char *library_dir = CRUNCH_LIB_DIR ? CRUNCH_LIB_DIR : directory;
        int length = snprintf(support_path, sizeof support_path, "%s/libcrunch-support.a",
                              library_dir);
        struct stat target_stat, support_stat;
        if (length < 0 || (size_t)length >= sizeof support_path) {
            fprintf(stderr, "crunch: compiler support library path is too long\n");
            free(link.argv);
            goto cleanup;
        }
        if (!strcmp(output, support_path) ||
            (stat(output, &target_stat) == 0 && stat(support_path, &support_stat) == 0 &&
             target_stat.st_dev == support_stat.st_dev &&
             target_stat.st_ino == support_stat.st_ino)) {
            fprintf(stderr, "crunch: output '%s' is also the compiler support library\n", output);
            free(link.argv);
            goto cleanup;
        }
        stage_add(&link, "-no-pie");
        stage_add(&link, "-o");
        stage_add(&link, staged_output);
        for (size_t i = 0; i < options.library_dir_count; ++i) {
            stage_add(&link, "-L");
            stage_add(&link, options.library_dirs[i]);
        }
        for (size_t i = 0; i < options.input_count; ++i) {
            if (options.library_inputs[i]) {
                stage_add(&link, "-l");
                stage_add(&link, options.inputs[i]);
            } else {
                stage_add(&link, temporary[i] ? temporary[i] : options.inputs[i]);
            }
        }
        stage_add(&link, support_path);
        status = link.failed ? 1 : run_pipeline(&link, 1, -1, options.verbose);
        free(link.argv);
        if (!status) status = replace_output(staged_output, output);
        goto cleanup;
    }
    if (options.warn_abi_mismatch) stage_add(&link, "-Wabi-mismatch");
    if (config.native_ld || options.dynamic_link) stage_add(&link, "-nostdlib");
    if (options.dynamic_link) {
        stage_add(&link, "-dynamic");
        if (options.pie) stage_add(&link, "-pie");
        stage_add(&link, "--forbid-defined=errno");
        stage_add(&link, "--unique-shared-symbol=errno");
    }
    if (options.relro) {
        stage_add(&link, "-z");
        stage_add(&link, "relro");
    }
    if (options.stop == STOP_OBJECT) {
        stage_add(&link, "-r");
    } else {
        stage_add(&link, "--require-defined=_start");
    }
    stage_add(&link, "-o");
    stage_add(&link, staged_output);
    if (options.dynamic_link) {
        for (size_t i = 0; i < options.library_dir_count; i++) {
            stage_add(&link, "-L");
            stage_add(&link, options.library_dirs[i]);
        }
        link_search_dirs = strdup(config.library_path ? config.library_path : CRUNCH_DEFAULT_LIBRARY_PATH);
        if (!link_search_dirs) {
            fprintf(stderr, "crunch: out of memory\n");
            free(link.argv);
            goto cleanup;
        }
        char *save = NULL;
        for (char *dir = strtok_r(link_search_dirs, ":", &save); dir;
             dir = strtok_r(NULL, ":", &save)) {
            stage_add(&link, "-L");
            stage_add(&link, dir);
        }
    }
    for (size_t i = 0; i < options.input_count; i++) {
        if (options.dynamic_link && options.library_inputs[i] && is_shared_input(options.inputs[i])) {
            const char *name = strrchr(options.inputs[i], '/');
            name = name ? name + 1 : options.inputs[i];
            size_t length = strlen(name);
            searched_shared_names[i] = malloc(length + 2);
            if (!searched_shared_names[i]) {
                fprintf(stderr, "crunch: out of memory\n");
                free(link.argv);
                goto cleanup;
            }
            searched_shared_names[i][0] = ':';
            memcpy(searched_shared_names[i] + 1, name, length + 1);
            stage_add(&link, "-l");
            stage_add(&link, searched_shared_names[i]);
        } else {
            stage_add(&link, temporary[i] ? temporary[i] : options.inputs[i]);
        }
    }
    char library_path[PATH_MAX + 16];
    char fallback_library_path[PATH_MAX + 32];
    if (options.stop == STOP_EXECUTABLE) {
        const char *library_dir = CRUNCH_LIB_DIR;
        if (options.dynamic_link) library_dir = CRUNCH_SHARED_LIB_DIR;
        if (!library_dir) library_dir = directory;
        int length = snprintf(library_path, sizeof library_path, "%s/libcrunch.%s", library_dir,
                              options.dynamic_link ? "so" : "a");
        if (length < 0 || (size_t)length >= sizeof library_path) {
            fprintf(stderr, "crunch: runtime library path is too long\n");
            free(link.argv);
            goto cleanup;
        }
        if (options.dynamic_link) {
            if (library_dir[0] != '/' || strchr(library_dir, ':') || strchr(library_dir, '$')) {
                fprintf(stderr, "crunch: shared runtime directory cannot be embedded in RUNPATH: '%s'\n",
                        library_dir);
                free(link.argv);
                goto cleanup;
            }
            if (access(library_path, F_OK) != 0) {
                fprintf(stderr, "crunch: cannot access shared runtime '%s': %s\n",
                        library_path, strerror(errno));
                free(link.argv);
                goto cleanup;
            }
            for (size_t i = 0; i < options.runtime_path_count; i++) {
                stage_add(&link, "-rpath");
                stage_add(&link, options.runtime_paths[i]);
            }
            stage_add(&link, "-rpath");
            stage_add(&link, library_dir);
            int fallback_length = snprintf(fallback_library_path, sizeof fallback_library_path,
                                           "%s/libcrunch-fallback.a", library_dir);
            if (fallback_length < 0 || (size_t)fallback_length >= sizeof fallback_library_path) {
                fprintf(stderr, "crunch: fallback library path is too long\n");
                free(link.argv);
                goto cleanup;
            }
            stage_add(&link, fallback_library_path);
        }
        struct stat target_stat, runtime_stat;
        if (options.dynamic_link &&
            (!strcmp(output, library_path) ||
             (stat(output, &target_stat) == 0 && stat(library_path, &runtime_stat) == 0 &&
              target_stat.st_dev == runtime_stat.st_dev && target_stat.st_ino == runtime_stat.st_ino))) {
            fprintf(stderr, "crunch: output '%s' is also the shared runtime input\n", output);
            free(link.argv);
            goto cleanup;
        }
        stage_add(&link, library_path);
    }
    status = link.failed ? 1 : run_pipeline(&link, 1, -1, options.verbose);
    free(link.argv);
    if (!status) status = replace_output(staged_output, output);

cleanup:
    free(link_search_dirs);
    if (searched_shared_names) {
        for (size_t i = 0; i < options.input_count; ++i) free(searched_shared_names[i]);
        free(searched_shared_names);
    }
    free(config.assembler_path);
    free(config.linker_path);
    free(config.archiver_path);
    free(config.library_path);
    if (staged_output) {
        if (unlink(staged_output) != 0 && errno != ENOENT) {
            fprintf(stderr, "crunch: cannot remove temporary output '%s': %s\n",
                    staged_output, strerror(errno));
            status = 1;
        }
        staged_output[output_directory_length] = '\0';
        if (rmdir(staged_output) != 0) {
            fprintf(stderr, "crunch: cannot remove temporary output directory '%s': %s\n",
                    staged_output, strerror(errno));
            status = 1;
        }
        free(staged_output);
    }
    if (temporary) {
        for (size_t i = 0; i < options.input_count; i++) {
            if (temporary[i]) {
                unlink(temporary[i]);
                free(temporary[i]);
            }
        }
        free(temporary);
    }
    if (resolved_libraries) {
        for (size_t i = 0; i < options.input_count; ++i) free(resolved_libraries[i]);
        free(resolved_libraries);
    }
    free(options.inputs);
    free(options.library_inputs);
    free(options.library_dirs);
    free(options.runtime_paths);
    free(options.include_dirs);
    for (size_t i = 0; i < options.system_include_count; ++i)
        free(options.system_include_dirs[i]);
    free(options.system_include_dirs);
    free(options.defines);
    return status ? 1 : 0;
}
