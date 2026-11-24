#!/bin/sh
set -eu

mode=gnu
case "${1-}" in
    --native) mode=native; shift ;;
    --glibc) mode=glibc; shift ;;
esac
if [ "$#" -ne 1 ]; then
    echo "usage: $0 [--native|--glibc] NEW_OUTPUT_DIRECTORY" >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${CRUNCH_BOOTSTRAP_COMPILER:-"$root/build/out/crunch"}
runtime=${CRUNCH_BOOTSTRAP_RUNTIME:-"$root/build/out/libcrunch.a"}
support="$(dirname -- "$compiler")/libcrunch-support.a"
if [ ! -x "$compiler" ]; then
    echo "bootstrap: build the Crunch compiler first" >&2
    exit 1
fi
if [ "$mode" = glibc ]; then
    if [ ! -f "$support" ]; then
        echo "bootstrap: compiler support archive not found: $support" >&2
        exit 1
    fi
elif [ ! -f "$runtime" ]; then
    echo "bootstrap: build Crunch and its static runtime first" >&2
    exit 1
fi

mkdir -- "$1"
output=$(CDPATH= cd -- "$1" && pwd)
mkdir -- "$output/objects"
if [ "$mode" = native ]; then
    printf 'assembler = crunch\nlinker = crunch\n' > "$output/bootstrap.conf"
elif [ "$mode" = glibc ]; then
    printf 'assembler = crunch\nlinker = crunch\n' > "$output/bootstrap.conf"
    headers="$output/project-headers"
    mkdir -- "$headers"
    for part in assembler codegen common ir lexer optimizer parser sema; do
        cp -R -- "$root/include/$part" "$headers/$part"
    done
    cp -- "$root/include/version.h" "$root/build/crunch-linker-config.h" "$headers/"
    unset CRUNCH_INCLUDE_PATH CPATH C_INCLUDE_PATH
    SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH-0}
    export SOURCE_DATE_EPOCH
else
    printf 'assembler = gnu\nlinker = gnu\n' > "$output/bootstrap.conf"
fi
CRUNCH_CONF_FILE="$output/bootstrap.conf"
export CRUNCH_CONF_FILE

for source in src/driver/crunch_main.c src/driver/pp_main.c src/driver/ir_main.c \
              src/driver/opt_main.c src/optimizer/optimizer.c \
              src/ir/ir.c src/common/source.c src/common/diagnostic.c \
              src/common/constant_expr.c src/lexer/lexer.c src/lexer/token.c \
              src/parser/parser.c src/parser/ast.c src/parser/ast_json.c \
              src/sema/sema.c src/sema/symbol.c src/driver/asm_main.c \
              src/codegen/x86_64.c; do
    object=$(printf '%s' "$source" | tr / _)
    printf 'bootstrap: %s\n' "$source" >&2
    if [ "$mode" = glibc ]; then
        "$compiler" --glibc "-DCRUNCH_DEFAULT_INCLUDE_DIR=\"$root/include\"" \
            '-DCRUNCH_DEFAULT_ASSEMBLER="crunch"' '-DCRUNCH_DEFAULT_LINKER="crunch"' \
            -I "$headers" -c "$root/$source" -o "$output/objects/$object.o"
    else
        "$compiler" -D_Thread_local= '-D__DATE__="bootstrap"' '-D__TIME__="bootstrap"' \
            "-DCRUNCH_DEFAULT_INCLUDE_DIR=\"$root/include\"" \
            "-DCRUNCH_DEFAULT_ASSEMBLER=\"$([ "$mode" = native ] && printf crunch || printf gnu)\"" \
            "-DCRUNCH_DEFAULT_LINKER=\"$([ "$mode" = native ] && printf crunch || printf gnu)\"" \
            -I "$root/include" -I "$root/build" \
            -c "$root/$source" -o "$output/objects/$object.o"
    fi
done

objects="$output/objects"
link_tool() {
    destination=$1
    shift
    if [ "$mode" = glibc ]; then
        "$compiler" --glibc "$@" -lm -o "$destination"
    else
        "$compiler" --static "$@" -o "$destination"
    fi
}
link_tool "$output/crunch" "$objects/src_driver_crunch_main.c.o"
link_tool "$output/crunch-pp" "$objects/src_driver_pp_main.c.o"
link_tool "$output/crunch-opt" "$objects/src_driver_opt_main.c.o" "$objects/src_optimizer_optimizer.c.o"
link_tool "$output/crunch-asm" "$objects/src_driver_asm_main.c.o" "$objects/src_common_diagnostic.c.o" \
    "$objects/src_codegen_x86_64.c.o"
link_tool "$output/crunch-ir" "$objects/src_driver_ir_main.c.o" "$objects/src_ir_ir.c.o" \
    "$objects/src_common_source.c.o" "$objects/src_common_diagnostic.c.o" \
    "$objects/src_common_constant_expr.c.o" "$objects/src_lexer_lexer.c.o" \
    "$objects/src_lexer_token.c.o" "$objects/src_parser_parser.c.o" \
    "$objects/src_parser_ast.c.o" "$objects/src_parser_ast_json.c.o" \
    "$objects/src_sema_sema.c.o" "$objects/src_sema_symbol.c.o"
if [ "$mode" = native ] || [ "$mode" = glibc ]; then
    for source in src/assembler/assembler.c src/driver/crunch_as_main.c \
                  src/linker/crunch-ld.c src/archiver/crunch-ar.c; do
        object=$(printf '%s' "$source" | tr / _)
        printf 'bootstrap: %s\n' "$source" >&2
        if [ "$mode" = glibc ]; then
            "$compiler" --glibc -I "$headers" -c "$root/$source" -o "$objects/$object.o"
        else
            "$compiler" -D_Thread_local= -I "$root/include" -I "$root/build" \
                -c "$root/$source" -o "$objects/$object.o"
        fi
    done
    link_tool "$output/crunch-as" "$objects/src_assembler_assembler.c.o" \
        "$objects/src_driver_crunch_as_main.c.o"
    link_tool "$output/crunch-ld" "$objects/src_linker_crunch-ld.c.o"
    link_tool "$output/crunch-ar" "$objects/src_archiver_crunch-ar.c.o"
fi
if [ "$mode" = native ]; then
    make -C "$root/build" CONFIG_MK="$root/build/config.mk" BUILD_DIR="$output" \
        CRUNCH_RUNTIME_ASSEMBLER=crunch CRUNCH_DEFAULT_ARCHIVER=crunch CC=false \
        -o "$output/crunch-pp" -o "$output/crunch-ir" -o "$output/crunch-opt" -o "$output/crunch-asm" \
        -o "$output/crunch-as" -o "$output/crunch-ld" -o "$output/crunch-ar" \
        "$output/libcrunch.a" "$output/libcrunch-fallback.a" "$output/libcrunch-support.a" \
        "$output/libcrunch.so"
elif [ "$mode" = gnu ]; then
    cp -- "$runtime" "$output/libcrunch.a"
    cp -- "$support" "$output/libcrunch-support.a"
else
    cp -- "$support" "$output/libcrunch-support.a"
fi
