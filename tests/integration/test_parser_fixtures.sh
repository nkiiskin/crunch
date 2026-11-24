#!/bin/sh
set -eu
lexer=$1
parser=$2
root=$(CDPATH= cd -- "$(dirname -- "$0")/../fixtures/parser" && pwd)
for file in hello.bc arithmetic.bc loops.bc arrays.bc structures.bc argv.bc; do
    "$lexer" "$root/$file" | "$parser" >/dev/null
done
# The summary counts local declarations inside function bodies, not only globals.
loops_output=$("$lexer" "$root/loops.bc" | "$parser")
printf '%s\n' "$loops_output" | grep -q '"format":"CRUNCH-AST","version":3' || {
    echo "parser should emit the versioned AST document" >&2
    printf '%s\n' "$loops_output" >&2
    exit 1
}
printf '%s\n' "$loops_output" | grep -q '"name":"i"' || {
    echo "AST should include local variable i" >&2
    exit 1
}
printf '%s\n' "$loops_output" | grep -q '"name":"total"' || {
    echo "AST should include local variable total" >&2
    exit 1
}
printf '%s\n' "$loops_output" | grep -q '"kind":"for"' || {
    echo "AST should include the for statement" >&2
    exit 1
}
if "$lexer" "$root/invalid_missing_semicolon.bc" | "$parser" >/dev/null 2>&1; then
    echo "parser unexpectedly accepted invalid fixture" >&2
    exit 1
fi
printf 'parser fixture tests passed\n'
