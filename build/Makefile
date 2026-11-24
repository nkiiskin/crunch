CONFIG_MK ?= config.mk
ifeq ($(wildcard $(CONFIG_MK)),)
ifneq ($(MAKECMDGOALS),clean)
$(error Build is not configured; run ../configure from the project root first)
endif
endif

-include $(CONFIG_MK)

CC ?= cc
PYTHON ?= python3
SHELL_TOOL ?= sh
SEARCH_LIB_DIR ?= /usr/local/lib
CPPFLAGS ?= -iquote ../include
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
BUILD_DIR := out
COMMON := ../src/common/source.c ../src/common/diagnostic.c ../src/common/constant_expr.c
LEXER := ../src/lexer/lexer.c ../src/lexer/token.c

.PHONY: all clean test
PP_CPPFLAGS = -DCRUNCH_DEFAULT_INCLUDE_DIR='"$(CRUNCH_INCLUDE_DIR)"' -DCRUNCH_DEFAULT_CONF_FILE='"$(CRUNCH_CONF_FILE)"'
LINKER_CONFIG := $(dir $(CONFIG_MK))crunch-linker-config.h
PARSER := ../src/parser/parser.c ../src/parser/ast.c ../src/parser/ast_json.c
SEMA := ../src/sema/sema.c ../src/sema/symbol.c

LIB_SOURCES := $(wildcard ../lib/*.s)
LIB_OBJECTS := $(patsubst ../lib/%.s,$(BUILD_DIR)/lib/%.o,$(LIB_SOURCES))
LIB_OBJECTS += $(BUILD_DIR)/lib/parse_real.o $(BUILD_DIR)/lib/format_extended.o $(BUILD_DIR)/lib/scan.o
SYSCALL_SOURCES := ../lib/syscalls/crunch_open.s ../lib/syscalls/crunch_openat.s ../lib/syscalls/crunch_read.s ../lib/syscalls/crunch_write.s ../lib/syscalls/crunch_close.s ../lib/syscalls/crunch_stat.s ../lib/syscalls/crunch_fstat.s ../lib/syscalls/crunch_lstat.s ../lib/syscalls/crunch_readlink.s ../lib/syscalls/crunch_getdents64.s ../lib/syscalls/crunch_unlink.s ../lib/syscalls/crunch_rename.s ../lib/syscalls/crunch_mkdir.s ../lib/syscalls/crunch_rmdir.s ../lib/syscalls/crunch_lseek.s ../lib/syscalls/crunch_mmap.s ../lib/syscalls/crunch_munmap.s ../lib/syscalls/crunch_mremap.s
SYSCALL_SOURCES += ../lib/syscalls/crunch_fstatat.s ../lib/syscalls/crunch_readlinkat.s ../lib/syscalls/crunch_unlinkat.s ../lib/syscalls/crunch_mkdirat.s ../lib/syscalls/crunch_renameat.s ../lib/syscalls/crunch_symlinkat.s
SYSCALL_SOURCES += ../lib/syscalls/crunch_access.s ../lib/syscalls/crunch_dup.s ../lib/syscalls/crunch_dup2.s ../lib/syscalls/crunch_pipe.s ../lib/syscalls/crunch_poll.s ../lib/syscalls/crunch_select.s ../lib/syscalls/crunch_fork.s ../lib/syscalls/crunch_getpid.s ../lib/syscalls/crunch_kill.s
SYSCALL_OBJECTS := $(patsubst ../lib/syscalls/%.s,$(BUILD_DIR)/lib/%.o,$(SYSCALL_SOURCES))
NATIVE_LIB_OBJECTS := $(patsubst ../lib/%.s,$(BUILD_DIR)/lib-native/%.o,$(LIB_SOURCES))
NATIVE_LIB_OBJECTS += $(BUILD_DIR)/lib-native/parse_real.o $(BUILD_DIR)/lib-native/format_extended.o $(BUILD_DIR)/lib-native/scan.o
NATIVE_SYSCALL_OBJECTS := $(patsubst ../lib/syscalls/%.s,$(BUILD_DIR)/lib-native/%.o,$(SYSCALL_SOURCES))
FALLBACK_NAMES := $(shell $(PYTHON) ../build/runtime-fallbacks.py ../include/common/runtime_routines.h --list)
ifeq ($(strip $(FALLBACK_NAMES)),)
$(error Unable to generate runtime fallback names)
endif
GNU_FALLBACK_OBJECTS := $(addprefix $(BUILD_DIR)/lib-fallback/fallback_,$(addsuffix .o,$(FALLBACK_NAMES)))
NATIVE_FALLBACK_OBJECTS := $(addprefix $(BUILD_DIR)/lib-fallback-native/fallback_,$(addsuffix .o,$(FALLBACK_NAMES)))

RUNTIME_OBJECTS := $(LIB_OBJECTS) $(SYSCALL_OBJECTS)
FALLBACK_OBJECTS := $(GNU_FALLBACK_OBJECTS)
IR_POW_OBJECT := $(BUILD_DIR)/lib/pow.o
ifeq ($(CRUNCH_RUNTIME_ASSEMBLER),crunch)
RUNTIME_OBJECTS := $(NATIVE_LIB_OBJECTS) $(NATIVE_SYSCALL_OBJECTS)
FALLBACK_OBJECTS := $(NATIVE_FALLBACK_OBJECTS)
IR_POW_OBJECT := $(BUILD_DIR)/lib-native/pow.o
endif

SHARED_RUNTIME_OBJECTS := $(filter-out %/errno.o %/print.o %/pow.o %/end.o %/run_initializers.o \
	%/printf.o %/fprintf.o %/snprintf.o %/standard_streams.o,$(RUNTIME_OBJECTS)) \
	$(BUILD_DIR)/lib-shared/errno.o $(BUILD_DIR)/lib-shared/print.o $(BUILD_DIR)/lib-shared/pow.o \
	$(BUILD_DIR)/lib-shared/end.o \
	$(BUILD_DIR)/lib-shared/run_initializers.o $(BUILD_DIR)/lib-shared/printf.o \
	$(BUILD_DIR)/lib-shared/fprintf.o $(BUILD_DIR)/lib-shared/snprintf.o \
	$(BUILD_DIR)/lib-shared/standard_streams.o
SHARED_AS := $(AS)
SHARED_AS_DEP :=
ifeq ($(CRUNCH_RUNTIME_ASSEMBLER),crunch)
SHARED_AS := $(BUILD_DIR)/crunch-as
SHARED_AS_DEP := $(BUILD_DIR)/crunch-as
endif

ARCHIVE_TOOL := $(CRUNCH_AR)
ARCHIVE_DEP :=
ifeq ($(CRUNCH_DEFAULT_ARCHIVER),crunch)
ARCHIVE_TOOL := $(BUILD_DIR)/crunch-ar
ARCHIVE_DEP := $(BUILD_DIR)/crunch-ar
endif

all: $(BUILD_DIR)/libcrunch.a $(BUILD_DIR)/libcrunch-fallback.a $(BUILD_DIR)/libcrunch-support.a $(BUILD_DIR)/libcrunch.so $(BUILD_DIR)/crunch $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-lexer $(BUILD_DIR)/crunch-parser $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm $(BUILD_DIR)/crunch-as $(BUILD_DIR)/crunch-ld $(BUILD_DIR)/crunch-ar $(BUILD_DIR)/test-sema

.PHONY: native-runtime
native-runtime: $(BUILD_DIR)/libcrunch-native.a

$(BUILD_DIR):
	mkdir -p $@

# Build the conversion routine with Crunch so native-only builds need no GNU tools.
$(BUILD_DIR)/generated/parse_real.s: ../lib/parse_real.bc ../include/errno.h ../include/stdint.h $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/generated
	@trap 'rm -f "$@.pp.tmp" "$@.ir.tmp" "$@.opt.tmp" "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-pp -I ../include "$<" > "$@.pp.tmp" && \
	  $(BUILD_DIR)/crunch-ir --module "$@.pp.tmp" > "$@.ir.tmp" && \
	  $(BUILD_DIR)/crunch-opt "$@.ir.tmp" > "$@.opt.tmp" && \
	  $(BUILD_DIR)/crunch-asm "$@.opt.tmp" > "$@.tmp" && \
	  printf '.hidden _crunch_real_to_double\n.hidden _crunch_real_to_float\n.hidden _crunch_real_to_extended\n' >> "$@.tmp" && \
	  mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib/parse_real.o: $(BUILD_DIR)/generated/parse_real.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib
	$(AS) -o $@ $<

$(BUILD_DIR)/lib-native/parse_real.o: $(BUILD_DIR)/generated/parse_real.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-as -o "$@.tmp" "$<" && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/generated/format_extended.s: ../lib/format_extended.bc $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/generated
	@trap 'rm -f "$@.pp.tmp" "$@.ir.tmp" "$@.opt.tmp" "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-pp -I ../include "$<" > "$@.pp.tmp" && \
	  $(BUILD_DIR)/crunch-ir --module "$@.pp.tmp" > "$@.ir.tmp" && \
	  $(BUILD_DIR)/crunch-opt "$@.ir.tmp" > "$@.opt.tmp" && \
	  $(BUILD_DIR)/crunch-asm "$@.opt.tmp" > "$@.tmp" && \
	  printf '.hidden _crunch_format_extended\n' >> "$@.tmp" && \
	  mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib/format_extended.o: $(BUILD_DIR)/generated/format_extended.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib
	$(AS) -o $@ $<

$(BUILD_DIR)/lib-native/format_extended.o: $(BUILD_DIR)/generated/format_extended.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-as -o "$@.tmp" "$<" && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/generated/scan.s: ../lib/scan.bc ../include/stdio.h ../include/stdlib.h $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/generated
	@trap 'rm -f "$@.pp.tmp" "$@.ir.tmp" "$@.opt.tmp" "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-pp -I ../include "$<" > "$@.pp.tmp" && \
	  $(BUILD_DIR)/crunch-ir --module "$@.pp.tmp" > "$@.ir.tmp" && \
	  $(BUILD_DIR)/crunch-opt "$@.ir.tmp" > "$@.opt.tmp" && \
	  $(BUILD_DIR)/crunch-asm "$@.opt.tmp" > "$@.tmp" && \
	  printf '.hidden _crunch_scan_engine\n' >> "$@.tmp" && \
	  mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib/scan.o: $(BUILD_DIR)/generated/scan.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib
	$(AS) -o $@ $<

$(BUILD_DIR)/lib-native/scan.o: $(BUILD_DIR)/generated/scan.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-as -o "$@.tmp" "$<" && mv -f "$@.tmp" "$@"

# The Crunch standard library: one object per source so the linker pulls in only
# the members a program uses.
$(BUILD_DIR)/lib/%.o: ../lib/%.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib
	$(AS) -o $@ $<

$(BUILD_DIR)/lib/%.o: ../lib/syscalls/%.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib
	$(AS) -o $@ $<

$(BUILD_DIR)/generated/runtime-fallback/%.s: ../build/runtime-fallbacks.py ../include/common/runtime_routines.h | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/generated/runtime-fallback
	$(PYTHON) ../build/runtime-fallbacks.py ../include/common/runtime_routines.h $* > $@

$(BUILD_DIR)/lib-fallback/fallback_%.o: $(BUILD_DIR)/generated/runtime-fallback/%.s | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-fallback
	$(AS) -o $@ $<

$(BUILD_DIR)/lib-fallback-native/fallback_%.o: $(BUILD_DIR)/generated/runtime-fallback/%.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-fallback-native
	$(BUILD_DIR)/crunch-as -o $@ $<

$(BUILD_DIR)/libcrunch.a: $(RUNTIME_OBJECTS) $(FALLBACK_OBJECTS) $(CONFIG_MK) $(ARCHIVE_DEP)
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(ARCHIVE_TOOL) rcs "$@.tmp" $(RUNTIME_OBJECTS) $(FALLBACK_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/libcrunch-fallback.a: $(FALLBACK_OBJECTS) $(CONFIG_MK) $(ARCHIVE_DEP)
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(ARCHIVE_TOOL) rcs "$@.tmp" $(FALLBACK_OBJECTS) && mv -f "$@.tmp" "$@"

SUPPORT_OBJECTS := $(IR_POW_OBJECT) $(addprefix $(dir $(IR_POW_OBJECT)),memcpy.o memset.o memcmp.o)

$(BUILD_DIR)/libcrunch-support.a: $(SUPPORT_OBJECTS) $(CONFIG_MK) $(ARCHIVE_DEP) Makefile
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(ARCHIVE_TOOL) rcs "$@.tmp" $(SUPPORT_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib-native/%.o: ../lib/%.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-as -o "$@.tmp" "$<" && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib-native/%.o: ../lib/syscalls/%.s $(BUILD_DIR)/crunch-as | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-as -o "$@.tmp" "$<" && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/libcrunch-native.a: $(NATIVE_LIB_OBJECTS) $(NATIVE_SYSCALL_OBJECTS) $(NATIVE_FALLBACK_OBJECTS) $(CONFIG_MK) $(ARCHIVE_DEP)
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(ARCHIVE_TOOL) rcs "$@.tmp" $(NATIVE_LIB_OBJECTS) $(NATIVE_SYSCALL_OBJECTS) $(NATIVE_FALLBACK_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/lib-shared/%.o: ../lib/%.s $(CONFIG_MK) $(SHARED_AS_DEP) | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/lib-shared
	@trap 'rm -f "$@.tmp" "$@.s.tmp"' 0 1 2 15; \
	  { printf '.set CRUNCH_SHARED,1\n' && cat "$<"; } > "$@.s.tmp" && \
	  $(SHARED_AS) -o "$@.tmp" "$@.s.tmp" && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/libcrunch.so.1: $(SHARED_RUNTIME_OBJECTS) $(BUILD_DIR)/crunch-ld
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  $(BUILD_DIR)/crunch-ld -nostdlib -shared --no-undefined -soname libcrunch.so.1 -o "$@.tmp" $(SHARED_RUNTIME_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/libcrunch.so: $(BUILD_DIR)/libcrunch.so.1
	@ln -sfn libcrunch.so.1 "$@"

$(BUILD_DIR)/crunch: ../src/driver/crunch_main.c ../include/version.h $(CONFIG_MK) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -DCRUNCH_AS_PATH='"$(AS)"' -DCRUNCH_LD_PATH='"$(LD)"' -DCRUNCH_AR_PATH='"$(CRUNCH_AR)"' -DCRUNCH_DEFAULT_CONF_FILE='"$(CRUNCH_CONF_FILE)"' -DCRUNCH_DEFAULT_ASSEMBLER='"$(CRUNCH_DEFAULT_ASSEMBLER)"' -DCRUNCH_DEFAULT_LINKER='"$(CRUNCH_DEFAULT_LINKER)"' -DCRUNCH_DEFAULT_ARCHIVER='"$(CRUNCH_DEFAULT_ARCHIVER)"' -DCRUNCH_DEFAULT_LIBRARY_PATH='"$(SEARCH_LIB_DIR)"' $(CFLAGS) $< -o $@

$(BUILD_DIR)/crunch-lexer: ../src/driver/lexer_main.c $(COMMON) $(LEXER) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/crunch-pp: ../src/driver/pp_main.c ../include/version.h $(CONFIG_MK) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(PP_CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

$(BUILD_DIR)/crunch-parser: ../src/driver/parser_main.c $(PARSER) ../src/lexer/token.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@


$(BUILD_DIR)/crunch-ir: ../src/driver/ir_main.c ../src/ir/ir.c $(COMMON) $(LEXER) $(PARSER) $(SEMA) $(IR_POW_OBJECT) $(CONFIG_MK) ../include/common/compiler.h ../include/common/runtime_routines.h ../include/common/runtime_declaration.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.o,$^) -o $@

$(BUILD_DIR)/crunch-opt: ../src/driver/opt_main.c ../src/optimizer/optimizer.c ../include/optimizer/optimizer.h ../include/common/compiler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

$(BUILD_DIR)/crunch-asm: ../src/driver/asm_main.c ../src/common/diagnostic.c ../src/codegen/x86_64.c ../include/common/compiler.h ../include/common/runtime_routines.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

$(BUILD_DIR)/crunch-as: ../src/driver/crunch_as_main.c ../src/assembler/assembler.c ../include/assembler/assembler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

$(BUILD_DIR)/crunch-ld: ../src/linker/crunch-ld.c ../src/linker/relocatable.inc ../include/version.h $(CONFIG_MK) $(LINKER_CONFIG) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -iquote "$(dir $(LINKER_CONFIG))" -DCRUNCH_DEFAULT_CONF_FILE='"$(CRUNCH_CONF_FILE)"' -DCRUNCH_DEFAULT_RUNTIME_DIR='"$(SEARCH_LIB_DIR)/crunch"' $(CFLAGS) $< -o $@

$(BUILD_DIR)/crunch-ar: ../src/archiver/crunch-ar.c ../include/version.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(BUILD_DIR)/test-sema: ../tests/unit/sema/test_sema.c $(COMMON) $(LEXER) $(PARSER) $(SEMA) ../include/common/runtime_declaration.h ../include/common/runtime_routines.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o $@

$(BUILD_DIR)/test-lexer: ../tests/unit/lexer/test_lexer.c $(LEXER) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test-native/libcrunch.a: $(NATIVE_LIB_OBJECTS) $(NATIVE_SYSCALL_OBJECTS) $(NATIVE_FALLBACK_OBJECTS) $(BUILD_DIR)/crunch-ar
	mkdir -p $(BUILD_DIR)/test-native
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(BUILD_DIR)/crunch-ar rcs "$@.tmp" $(NATIVE_LIB_OBJECTS) $(NATIVE_SYSCALL_OBJECTS) $(NATIVE_FALLBACK_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/test-gnu/libcrunch.a: $(LIB_OBJECTS) $(SYSCALL_OBJECTS) $(GNU_FALLBACK_OBJECTS) $(CONFIG_MK)
	mkdir -p $(BUILD_DIR)/test-gnu
	@trap 'rm -f "$@.tmp"' 0 1 2 15; \
	  rm -f "$@.tmp" && $(CRUNCH_AR) rcs "$@.tmp" $(LIB_OBJECTS) $(SYSCALL_OBJECTS) $(GNU_FALLBACK_OBJECTS) && mv -f "$@.tmp" "$@"

$(BUILD_DIR)/crunch-test-native: ../src/driver/crunch_main.c ../include/version.h $(CONFIG_MK) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -DCRUNCH_AS_PATH='"$(AS)"' -DCRUNCH_LD_PATH='"$(LD)"' -DCRUNCH_AR_PATH='"$(CRUNCH_AR)"' -DCRUNCH_DEFAULT_CONF_FILE='"$(CRUNCH_CONF_FILE)"' -DCRUNCH_DEFAULT_ASSEMBLER='"$(CRUNCH_DEFAULT_ASSEMBLER)"' -DCRUNCH_DEFAULT_LINKER='"$(CRUNCH_DEFAULT_LINKER)"' -DCRUNCH_DEFAULT_ARCHIVER='"$(CRUNCH_DEFAULT_ARCHIVER)"' -DCRUNCH_DEFAULT_LIBRARY_PATH='"$(SEARCH_LIB_DIR)"' -DCRUNCH_LIB_DIR='"$(abspath $(BUILD_DIR)/test-native)"' -DCRUNCH_SHARED_LIB_DIR='"$(abspath $(BUILD_DIR))"' $(CFLAGS) $< -o $@

ifeq ($(CRUNCH_NATIVE_ONLY),yes)
test: $(BUILD_DIR)/libcrunch.so $(BUILD_DIR)/test-native/libcrunch.a $(BUILD_DIR)/crunch-test-native $(BUILD_DIR)/test-lexer $(BUILD_DIR)/crunch $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-lexer $(BUILD_DIR)/crunch-parser $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm $(BUILD_DIR)/crunch-as $(BUILD_DIR)/crunch-ld $(BUILD_DIR)/crunch-ar $(BUILD_DIR)/test-sema
	CRUNCH_TEST_NATIVE_ONLY=1 CRUNCH_TEST_TOOLCHAIN=crunch $(PYTHON) ../tests/run-tests.py "$(BUILD_DIR)" "$(PYTHON)" "$(SHELL_TOOL)"
else
ifeq ($(CRUNCH_TEST_TOOLCHAIN),crunch)
test: $(BUILD_DIR)/test-native/libcrunch.a $(BUILD_DIR)/crunch-test-native
endif
test: $(BUILD_DIR)/libcrunch.so $(BUILD_DIR)/test-gnu/libcrunch.a $(BUILD_DIR)/test-native/libcrunch.a $(BUILD_DIR)/test-lexer $(BUILD_DIR)/crunch-pp $(BUILD_DIR)/crunch-lexer $(BUILD_DIR)/crunch-parser $(BUILD_DIR)/crunch-ir $(BUILD_DIR)/crunch-opt $(BUILD_DIR)/crunch-asm $(BUILD_DIR)/crunch-as $(BUILD_DIR)/crunch-ld $(BUILD_DIR)/crunch-ar $(BUILD_DIR)/test-sema
	$(PYTHON) ../tests/run-tests.py "$(BUILD_DIR)" "$(PYTHON)" "$(SHELL_TOOL)"
endif

# The installed preprocessor has the installation paths compiled in, unlike the
# build-tree one, which points at the source tree.
INSTALL_BUILD_DIR ?= $(BUILD_DIR)/install
.PHONY: install-pp
install-pp:
	mkdir -p "$(INSTALL_BUILD_DIR)"
	$(CC) $(CPPFLAGS) -DCRUNCH_AS_PATH='"$(AS)"' -DCRUNCH_LD_PATH='"$(LD)"' -DCRUNCH_AR_PATH='"$(CRUNCH_AR)"' -DCRUNCH_DEFAULT_CONF_FILE='"$(INSTALL_CONF_FILE)"' -DCRUNCH_DEFAULT_ASSEMBLER='"$(CRUNCH_DEFAULT_ASSEMBLER)"' -DCRUNCH_DEFAULT_LINKER='"$(CRUNCH_DEFAULT_LINKER)"' -DCRUNCH_DEFAULT_ARCHIVER='"$(CRUNCH_DEFAULT_ARCHIVER)"' -DCRUNCH_LIB_DIR='"$(INSTALL_LIB_DIR)"' -DCRUNCH_DEFAULT_LIBRARY_PATH='"$(SEARCH_LIB_DIR)"' $(CFLAGS) ../src/driver/crunch_main.c -o "$(INSTALL_BUILD_DIR)/crunch"
	$(CC) $(CPPFLAGS) -DCRUNCH_DEFAULT_INCLUDE_DIR='"$(INSTALL_INCLUDE_DIR)"' -DCRUNCH_DEFAULT_CONF_FILE='"$(INSTALL_CONF_FILE)"' $(CFLAGS) ../src/driver/pp_main.c -o "$(INSTALL_BUILD_DIR)/crunch-pp"
	$(CC) $(CPPFLAGS) -iquote "$(dir $(LINKER_CONFIG))" -DCRUNCH_DEFAULT_CONF_FILE='"$(INSTALL_CONF_FILE)"' -DCRUNCH_DEFAULT_RUNTIME_DIR='"$(INSTALL_LIB_DIR)"' $(CFLAGS) ../src/linker/crunch-ld.c -o "$(INSTALL_BUILD_DIR)/crunch-ld"

clean:
	rm -rf $(BUILD_DIR)
