#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/out"}
[ -f "$ROOT/configure.ac" ] || { echo 'configure.ac is missing' >&2; exit 1; }
[ -f "$ROOT/Makefile.in" ] || { echo 'Automake-generated Makefile.in is missing' >&2; exit 1; }
TMP_BASE=${TMPDIR:-/tmp}/crunch-configure-test-$$
TMP_DIR="$TMP_BASE.dir"
TMP_SOURCE="$TMP_BASE.source"
TMP_OUT="$TMP_BASE.out"
TMP_ERR="$TMP_BASE.err"
trap 'rm -rf "$TMP_DIR" "$TMP_SOURCE" "$TMP_OUT" "$TMP_ERR"' EXIT HUP INT TERM
mkdir -p "$TMP_DIR" "$TMP_SOURCE/build" "$TMP_SOURCE/build-aux" "$TMP_SOURCE/src" "$TMP_SOURCE/include"
cp "$ROOT/configure" "$ROOT/Makefile.in" "$TMP_SOURCE/"
cp "$ROOT/build/config.mk.in" "$ROOT/build/crunch-linker-config.h.in" "$TMP_SOURCE/build/"
cp "$ROOT/src/main.c" "$TMP_SOURCE/src/"
cp "$ROOT/build-aux/config.guess" "$ROOT/build-aux/config.sub" "$ROOT/build-aux/install-sh" "$ROOT/build-aux/missing" "$ROOT/build-aux/compile" "$TMP_SOURCE/build-aux/"

if ! "$ROOT/configure" --help >"$TMP_OUT" 2>"$TMP_ERR"; then
    echo 'configure --help unexpectedly failed' >&2
    exit 1
fi
grep -q '^Usage:' "$TMP_OUT"
grep -q -- '--with-default-assembler=gnu|crunch' "$TMP_OUT"
grep -q -- '--with-default-linker=gnu|crunch' "$TMP_OUT"
grep -q -- '--with-runtime-assembler=gnu|crunch' "$TMP_OUT"
grep -q -- '--with-default-archiver=gnu|crunch' "$TMP_OUT"

if (unset MAKEFLAGS MFLAGS MAKELEVEL; cd "$TMP_DIR" && "$TMP_SOURCE/configure" --not-a-real-option >"$TMP_OUT" 2>"$TMP_ERR"); then
    echo 'configure accepted an unknown option' >&2
    exit 1
fi
grep -q 'unrecognized option' "$TMP_ERR"

if ! (unset MAKEFLAGS MFLAGS MAKELEVEL; cd "$TMP_DIR" && "$TMP_SOURCE/configure" --srcdir="$TMP_SOURCE" >"$TMP_OUT" 2>"$TMP_ERR"); then
    cat "$TMP_OUT" "$TMP_ERR" >&2
    echo 'configure failed in the current test environment' >&2
    exit 1
fi
[ -f "$TMP_DIR/build/config.mk" ] || { echo 'configure did not write build/config.mk' >&2; exit 1; }
grep -q '^CC := ' "$TMP_DIR/build/config.mk"
grep -q '^PYTHON := ' "$TMP_DIR/build/config.mk"
grep -q '^AS := ' "$TMP_DIR/build/config.mk"
grep -q '^LD := ' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_DEFAULT_LINKER := gnu$' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_DEFAULT_ASSEMBLER := gnu$' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_DEFAULT_ARCHIVER := gnu$' "$TMP_DIR/build/config.mk"
grep -q 'Tool selection (configuration file may override driver defaults)' "$TMP_OUT"
grep -q "archiver:  detected $(command -v ar) (GNU); driver and runtime gnu" "$TMP_OUT"
grep -q "^CRUNCH_INCLUDE_DIR := $TMP_SOURCE/include$" "$TMP_DIR/build/config.mk"

if grep -q 'LLVM' "$TMP_DIR/build/config.mk"; then
    echo 'configure still writes LLVM configuration variables' >&2
    exit 1
fi

# Default detection must agree with the first as/ld found in PATH.
[ "$(sed -n 's/^AS := //p' "$TMP_DIR/build/config.mk")" = "$(command -v as)" ] ||
    { echo 'configure did not detect the system as' >&2; exit 1; }
[ "$(sed -n 's/^LD := //p' "$TMP_DIR/build/config.mk")" = "$(command -v ld)" ] ||
    { echo 'configure did not detect the system ld' >&2; exit 1; }

TOOLS="$TMP_BASE.tools"
mkdir -p "$TOOLS"
trap 'rm -rf "$TMP_DIR" "$TMP_SOURCE" "$TMP_OUT" "$TMP_ERR" "$TOOLS"' EXIT HUP INT TERM
ln -s "$(command -v as)" "$TOOLS/custom-as"
ln -s "$(command -v ld)" "$TOOLS/custom-ld"
printf '#!/bin/sh\necho fake\n' >"$TOOLS/fake"
chmod +x "$TOOLS/fake"

configure_with() {
    rm -rf "$TMP_DIR" && mkdir -p "$TMP_DIR"
    (unset MAKEFLAGS MFLAGS MAKELEVEL; cd "$TMP_DIR" &&
        "$TMP_SOURCE/configure" --srcdir="$TMP_SOURCE" "$@" >"$TMP_OUT" 2>"$TMP_ERR")
}

configure_with --with-gnu-as="$TOOLS/custom-as" --with-crunch-ld="$TOOLS/custom-ld" ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected valid --with-crunch-as/ld' >&2; exit 1; }
grep -q "^AS := $TOOLS/custom-as$" "$TMP_DIR/build/config.mk"
grep -q "^LD := $TOOLS/custom-ld$" "$TMP_DIR/build/config.mk"

configure_with --with-gnu-as="$TOOLS/custom-as" ||
    { echo 'configure rejected --with-gnu-as alone' >&2; exit 1; }
grep -q "^AS := $TOOLS/custom-as$" "$TMP_DIR/build/config.mk"
grep -q "^LD := $(command -v ld)$" "$TMP_DIR/build/config.mk"

configure_with --with-crunch-as="$TOOLS/custom-as" ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected the legacy assembler path option' >&2; exit 1; }
grep -q "^AS := $TOOLS/custom-as$" "$TMP_DIR/build/config.mk"
grep -q 'deprecated; use --with-gnu-as=FILE' "$TMP_ERR"

configure_with --with-default-assembler=crunch ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected the Crunch assembler default' >&2; exit 1; }
grep -q '^CRUNCH_DEFAULT_ASSEMBLER := crunch$' "$TMP_DIR/build/config.mk"
mkdir -p "$TMP_DIR/assembler-tools"
(unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
    CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/assembler-tools" "$TMP_DIR/assembler-tools/crunch") ||
    { echo 'failed to build driver with native assembler default' >&2; exit 1; }
for tool in crunch-pp crunch-ir crunch-opt crunch-asm crunch-as crunch-ld; do
    ln -s "$BUILD_DIR/$tool" "$TMP_DIR/assembler-tools/$tool"
done
CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/assembler-tools/crunch" -v -c \
    "$ROOT/tests/fixtures/ir/main42.bc" -o "$TMP_DIR/program.o" >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_ERR" >&2; echo 'configured native assembler failed' >&2; exit 1; }
grep -q "$TMP_DIR/assembler-tools/crunch-as -o" "$TMP_ERR" ||
    { echo 'configured driver did not select crunch-as' >&2; exit 1; }

configure_with --with-default-linker=crunch ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected the native linker default' >&2; exit 1; }
grep -q '^CRUNCH_DEFAULT_LINKER := crunch$' "$TMP_DIR/build/config.mk"
mkdir -p "$TMP_DIR/out"
(unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
    CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/out" "$TMP_DIR/out/crunch") ||
    { echo 'failed to build driver with native linker default' >&2; exit 1; }
for tool in crunch-pp crunch-ir crunch-opt crunch-asm crunch-as crunch-ld libcrunch.a libcrunch-fallback.a libcrunch.so libcrunch.so.1; do
    ln -s "$BUILD_DIR/$tool" "$TMP_DIR/out/$tool"
done
CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/out/crunch" -v \
    "$ROOT/tests/fixtures/ir/main42.bc" -o "$TMP_DIR/program" >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_ERR" >&2; echo 'configured native linker failed' >&2; exit 1; }
grep -q "$TMP_DIR/out/crunch-ld -nostdlib" "$TMP_ERR" ||
    { echo 'configured driver did not select crunch-ld' >&2; exit 1; }
printf 'linker = gnu\n' >"$TMP_DIR/tool.conf"
CRUNCH_CONF_FILE="$TMP_DIR/tool.conf" "$TMP_DIR/out/crunch" -v -c \
    "$ROOT/tests/fixtures/ir/main42.bc" -o "$TMP_DIR/program.o" >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_ERR" >&2; echo 'configuration did not override native linker default' >&2; exit 1; }

expect_configure_failure() {
    message=$1; shift
    if configure_with "$@"; then
        echo "configure accepted $*" >&2
        exit 1
    fi
    grep -q "$message" "$TMP_ERR" || { cat "$TMP_ERR" >&2; echo "missing diagnostic: $message" >&2; exit 1; }
}
expect_configure_failure 'not an executable file' --with-gnu-as=/nonexistent/as
expect_configure_failure 'requires an absolute file name' --with-crunch-ld=relative/ld
expect_configure_failure 'requires a file name' --with-crunch-ld
expect_configure_failure 'GNU assembler is required' --with-gnu-as="$TOOLS/fake"
expect_configure_failure 'GNU ld is required' --with-crunch-ld="$TOOLS/fake"
expect_configure_failure 'must be gnu or crunch' --with-default-linker=invalid
expect_configure_failure 'must be gnu or crunch' --with-default-assembler=invalid
expect_configure_failure 'must be gnu or crunch' --with-default-assembler=auto
expect_configure_failure 'must be gnu or crunch' --with-default-archiver=auto
expect_configure_failure 'cannot be combined' --with-crunch-as="$TOOLS/custom-as" --with-gnu-as="$TOOLS/custom-as"
expect_configure_failure 'requires an absolute path' --with-dynamic-linker=relative/loader
expect_configure_failure 'requires an absolute path' '--with-dynamic-linker=/bad$loader'

for missing in as ld ar all; do
    poison="$TOOLS/unavailable-$missing"
    mkdir -p "$poison"
    for name in as ld ar; do
        if [ "$missing" = "$name" ] || [ "$missing" = all ]; then
            real_tool=$(command -v "$name")
            printf '#!/bin/sh\nif [ "$1" = --version ]; then exit 1; fi\nexec "%s" "$@"\n' \
                "$real_tool" >"$poison/$name"
            chmod +x "$poison/$name"
        fi
    done
    PATH="$poison:$PATH" configure_with ||
        { cat "$TMP_ERR" >&2; echo "configure failed with unavailable $missing" >&2; exit 1; }
    for pair in 'as AS ASSEMBLER' 'ld LD LINKER' 'ar CRUNCH_AR ARCHIVER'; do
        set -- $pair
        name=$1 variable=$2 choice=$3
        if [ "$missing" = "$name" ] || [ "$missing" = all ]; then
            grep -q "^$variable := $" "$TMP_DIR/build/config.mk"
            grep -q "^CRUNCH_DEFAULT_$choice := crunch$" "$TMP_DIR/build/config.mk"
        else
            grep -q "^$variable := $(command -v "$name")$" "$TMP_DIR/build/config.mk"
            grep -q "^CRUNCH_DEFAULT_$choice := gnu$" "$TMP_DIR/build/config.mk"
        fi
    done
    grep -q "Tool selection (configuration file may override driver defaults)" "$TMP_OUT"
    for option in assembler linker archiver; do
        grep -q "$option:.*detected.*driver" "$TMP_OUT"
    done
    mkdir -p "$TMP_DIR/selected"
    (unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
        CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/selected" \
        "$TMP_DIR/selected/crunch") ||
        { echo "failed to build driver with unavailable $missing" >&2; exit 1; }
    for tool in crunch-as crunch-ld crunch-ar; do
        ln -s "$BUILD_DIR/$tool" "$TMP_DIR/selected/$tool"
    done
    printf '.text\n.globl answer\nanswer: mov $42,%%eax\nret\n' >"$TMP_DIR/answer.s"
    CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/selected/crunch" -v -c \
        "$TMP_DIR/answer.s" -o "$TMP_DIR/answer.o" >"$TMP_OUT" 2>"$TMP_ERR" ||
        { cat "$TMP_ERR" >&2; echo "mixed assembler/linker failed for $missing" >&2; exit 1; }
    if [ "$missing" = as ] || [ "$missing" = all ]; then
        grep -q "$TMP_DIR/selected/crunch-as -o" "$TMP_ERR"
    else
        grep -q "$(command -v as) -o" "$TMP_ERR"
    fi
    if [ "$missing" = ld ] || [ "$missing" = all ]; then
        grep -q "$TMP_DIR/selected/crunch-ld -nostdlib -r" "$TMP_ERR"
    else
        grep -q "$(command -v ld) -r" "$TMP_ERR"
    fi
    CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/selected/crunch" -v --archive \
        -o "$TMP_DIR/answer.a" "$TMP_DIR/answer.o" >"$TMP_OUT" 2>"$TMP_ERR" ||
        { cat "$TMP_ERR" >&2; echo "selected archiver failed for $missing" >&2; exit 1; }
    if [ "$missing" = ar ] || [ "$missing" = all ]; then
        grep -q "$TMP_DIR/selected/crunch-ar rcs" "$TMP_ERR"
    else
        grep -q "$(command -v ar) rcs" "$TMP_ERR"
    fi
    if [ "$missing" = all ]; then
        (unset MAKEFLAGS MFLAGS MAKELEVEL; PATH="$poison:$PATH" make -s -C "$ROOT/build" \
            CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/fallback" all \
            >"$TMP_OUT" 2>"$TMP_ERR") ||
            { cat "$TMP_OUT" "$TMP_ERR" >&2; echo 'automatic fallback build failed' >&2; exit 1; }
    fi
done
PATH="$TOOLS/unavailable-all:$PATH" expect_configure_failure 'GNU assembler is required' \
    --with-default-assembler=gnu
PATH="$TOOLS/unavailable-all:$PATH" expect_configure_failure 'GNU ld is required' \
    --with-default-linker=gnu
PATH="$TOOLS/unavailable-all:$PATH" expect_configure_failure 'GNU ar is required' \
    --with-default-archiver=gnu
PATH="$TOOLS/unavailable-all:$PATH" expect_configure_failure 'GNU assembler is required' \
    --with-runtime-assembler=gnu

configure_with --enable-native-only --with-dynamic-linker=/custom/ld-linux.so ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected an explicit interpreter' >&2; exit 1; }
grep -q '^#define CRUNCH_DYNAMIC_LINKER "/custom/ld-linux.so"$' "$TMP_DIR/build/crunch-linker-config.h"
(unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
    CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/custom" "$TMP_DIR/custom/crunch-ld") ||
    { echo 'failed to build linker with configured interpreter' >&2; exit 1; }
printf '.text\n.globl _start\n_start: mov $60,%%eax\nxor %%edi,%%edi\nsyscall\n' |
    "$BUILD_DIR/crunch-as" -o "$TMP_DIR/start.o"
CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/custom/crunch-ld" -nostdlib -dynamic \
    -o "$TMP_DIR/program" "$TMP_DIR/start.o"
grep -aq '/custom/ld-linux.so' "$TMP_DIR/program" ||
    { echo 'linker did not use configured interpreter' >&2; exit 1; }
CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/custom/crunch-ld" -nostdlib \
    --dynamic-linker=/override/ld-linux.so -o "$TMP_DIR/program" "$TMP_DIR/start.o"
grep -aq '/override/ld-linux.so' "$TMP_DIR/program" ||
    { echo 'linker did not honor interpreter override' >&2; exit 1; }

configure_with --enable-native-only ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected native-only mode' >&2; exit 1; }
grep -q '^CRUNCH_NATIVE_ONLY := yes$' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_DEFAULT_ASSEMBLER := crunch$' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_DEFAULT_LINKER := crunch$' "$TMP_DIR/build/config.mk"
grep -q '^AS := $' "$TMP_DIR/build/config.mk"
grep -q '^LD := $' "$TMP_DIR/build/config.mk"
grep -q '^CRUNCH_AR := $' "$TMP_DIR/build/config.mk"
configure_with --enable-native-only --with-default-assembler=crunch ||
    { cat "$TMP_ERR" >&2; echo 'configure rejected native-only Crunch assembler selection' >&2; exit 1; }
mkdir -p "$TOOLS/poison"
for name in as ld ar; do
    real_tool=$(command -v "$name")
    printf '#!/bin/sh\nif [ "$1" = --version ]; then echo "%s" >> "$CRUNCH_PROBE_LOG"; exit 99; fi\nexec "%s" "$@"\n' \
        "$name" "$real_tool" >"$TOOLS/poison/$name"
    chmod +x "$TOOLS/poison/$name"
done
CRUNCH_PROBE_LOG="$TOOLS/probes" PATH="$TOOLS/poison:$PATH" configure_with --enable-native-only ||
    { cat "$TMP_ERR" >&2; echo 'native-only configure required GNU binutils' >&2; exit 1; }
[ ! -s "$TOOLS/probes" ] ||
    { cat "$TOOLS/probes" >&2; echo 'native-only configure probed GNU binutils' >&2; exit 1; }
# Build into a fresh output directory so existing GNU objects cannot mask
# native-only dependencies.
(unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
    CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/fresh" all >"$TMP_OUT" 2>"$TMP_ERR") ||
    { cat "$TMP_OUT" "$TMP_ERR" >&2; echo 'fresh native-only build failed' >&2; exit 1; }
[ -f "$TMP_DIR/fresh/lib-native/pow.o" ] && [ ! -e "$TMP_DIR/fresh/lib/pow.o" ] ||
    { echo 'native-only IR build used the wrong power helper' >&2; exit 1; }
CRUNCH_CONF_FILE="$TMP_DIR/no.conf" "$TMP_DIR/fresh/crunch" \
    "$ROOT/tests/fixtures/units/expressions/power_runtime.bc" -o "$TMP_DIR/power-program" \
    >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_OUT" "$TMP_ERR" >&2; echo 'fresh native-only power compilation failed' >&2; exit 1; }
status=0
"$TMP_DIR/power-program" || status=$?
[ "$status" -eq 42 ] || { echo "native-only power program returned $status" >&2; exit 1; }
mkdir -p "$TMP_DIR/out/test-native"
(unset MAKEFLAGS MFLAGS MAKELEVEL; make -s -C "$ROOT/build" \
    CONFIG_MK="$TMP_DIR/build/config.mk" BUILD_DIR="$TMP_DIR/out" "$TMP_DIR/out/crunch") ||
    { echo 'failed to build native-only driver' >&2; exit 1; }
for tool in crunch-pp crunch-lexer crunch-parser crunch-ir crunch-opt crunch-asm crunch-as crunch-ld crunch-ar test-lexer test-sema libcrunch.a libcrunch-fallback.a libcrunch-support.a libcrunch.so.1; do
    ln -s "$BUILD_DIR/$tool" "$TMP_DIR/out/$tool"
done
ln -s libcrunch.so.1 "$TMP_DIR/out/libcrunch.so"
ln -s crunch "$TMP_DIR/out/crunch-test-native"
ln -s "$BUILD_DIR/libcrunch.a" "$TMP_DIR/out/test-native/libcrunch.a"
CRUNCH_TEST_NATIVE_ONLY=1 CRUNCH_TEST_TOOLCHAIN=crunch CRUNCH_TEST_LINKER=crunch \
    python3 "$ROOT/tests/run-tests.py" "$TMP_DIR/out" python3 /bin/sh >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_OUT" "$TMP_ERR" >&2; echo 'native-only test suite failed' >&2; exit 1; }
printf 'assembler = gnu\nassembler_path = %s\nlinker = gnu\nlinker_path = %s\n' \
    "$TOOLS/custom-as" "$TOOLS/custom-ld" >"$TMP_DIR/tool.conf"
CRUNCH_CONF_FILE="$TMP_DIR/tool.conf" "$TMP_DIR/out/crunch" --static -v \
    "$ROOT/tests/fixtures/ir/main42.bc" -o "$TMP_DIR/program" >"$TMP_OUT" 2>"$TMP_ERR" ||
    { cat "$TMP_ERR" >&2; echo 'native-only driver rejected explicit GNU tool paths' >&2; exit 1; }
grep -q "$TOOLS/custom-as" "$TMP_ERR"
grep -q "$TOOLS/custom-ld" "$TMP_ERR"
status=0
"$TMP_DIR/program" || status=$?
[ "$status" -eq 42 ] || { echo "GNU override executable returned $status" >&2; exit 1; }
expect_configure_failure 'cannot be combined' --enable-native-only --with-default-linker=gnu
expect_configure_failure 'cannot be combined' --enable-native-only --with-default-assembler=gnu
expect_configure_failure 'cannot be combined' --enable-native-only --with-runtime-assembler=gnu
expect_configure_failure 'cannot be combined' --enable-native-only --with-default-archiver=gnu
expect_configure_failure 'cannot be combined' --enable-native-only --with-gnu-as="$TOOLS/custom-as"
expect_configure_failure 'cannot be combined' --enable-native-only --with-crunch-ld="$TOOLS/custom-ld"
expect_configure_failure 'cannot be combined' --enable-native-only --with-crunch-ar="$TOOLS/fake"
expect_configure_failure 'accepts yes or no' --enable-native-only=maybe

echo 'configure integration tests: OK'
