#!/bin/sh
set -eu

mode=
if [ "$#" -eq 1 ]; then
    case "$1" in
        --native|--glibc) mode=$1 ;;
        *) echo "usage: $0 [--native|--glibc]" >&2; exit 2 ;;
    esac
elif [ "$#" -ne 0 ]; then
    echo "usage: $0 [--native|--glibc]" >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
first="$root/bootstrap-stage1"
second="$root/bootstrap-stage2"

for directory in "$first" "$second"; do
    if [ -e "$directory" ] || [ -L "$directory" ]; then
        printf 'Output path already exists: %s\n' "$directory" >&2
        exit 1
    fi
done

cd "$root"
printf 'Configuring and building the initial compiler...\n'
./configure
make

printf 'Building the first-generation compiler...\n'
if [ "$mode" = --glibc ]; then
    CRUNCH_BOOTSTRAP_COMPILER="$root/build/out/crunch" \
        sh "$root/tests/bootstrap-compiler.sh" "$mode" "$first"
else
    CRUNCH_BOOTSTRAP_COMPILER="$root/build/out/crunch" \
    CRUNCH_BOOTSTRAP_RUNTIME="$root/build/out/libcrunch.a" \
        sh "$root/tests/bootstrap-compiler.sh" ${mode:+"$mode"} "$first"
fi

printf 'Building the second-generation compiler...\n'
if [ "$mode" = --glibc ]; then
    CRUNCH_BOOTSTRAP_COMPILER="$first/crunch" \
        sh "$root/tests/bootstrap-compiler.sh" "$mode" "$second"
else
    CRUNCH_BOOTSTRAP_COMPILER="$first/crunch" \
    CRUNCH_BOOTSTRAP_RUNTIME="$first/libcrunch.a" \
        sh "$root/tests/bootstrap-compiler.sh" ${mode:+"$mode"} "$second"
fi

printf '\nSecond-generation binaries:\n'
for tool in crunch crunch-pp crunch-ir crunch-opt crunch-asm; do
    printf '  %s/%s\n' "$second" "$tool"
done
if [ -n "$mode" ]; then
    for tool in crunch-as crunch-ld crunch-ar; do
        printf '  %s/%s\n' "$second" "$tool"
    done
fi
printf 'Configuration: %s/bootstrap.conf\n' "$second"
