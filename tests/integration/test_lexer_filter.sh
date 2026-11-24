#!/bin/sh
set -eu
lexer=$1
tmp=${TMPDIR:-/tmp}/crunch-lexer-test-$$
trap 'rm -f "$tmp" "$tmp.err"' EXIT HUP INT TERM
printf 'int main() { return 7; }\n' | "$lexer" >"$tmp"
grep -q '^CRUNCH-TOKENS 1$' "$tmp"
grep -q '^INT[[:space:]]' "$tmp"
grep -q '^RETURN[[:space:]]' "$tmp"
grep -q '^EOF[[:space:]]' "$tmp"
if printf '@' | "$lexer" > /dev/null 2>"$tmp.err"; then
    echo 'expected lexer failure for invalid character' >&2
    exit 1
fi
grep -q 'invalid token' "$tmp.err"
echo 'lexer filter integration tests: OK'
