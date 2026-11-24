#!/bin/sh
set -eu
bin=${1:-out}
tmp=${TMPDIR:-/tmp}/crunch-ir-$$
trap 'rm -f "$tmp" "$tmp.s" "$tmp.o" "$tmp.exe" "$tmp-hello" "$tmp-hello.s" "$tmp-hello.o" "$tmp-hello.exe" "$tmp-control_flow" "$tmp-control_flow.s" "$tmp-control_flow.o" "$tmp-control_flow.exe" "$tmp-comparisons" "$tmp-comparisons.s" "$tmp-comparisons.o" "$tmp-comparisons.exe" "$tmp-loop_condition" "$tmp-loop_condition.s" "$tmp-loop_condition.o" "$tmp-loop_condition.exe" "$tmp-numeric_types" "$tmp-numeric_types.s" "$tmp-numeric_types.o" "$tmp-numeric_types.exe"' EXIT HUP INT TERM

# Arithmetic path: return 42.
"$bin/crunch-ir" ../tests/fixtures/ir/main42.bc > "$tmp"
grep -q '^CRUNCH-IR 2$' "$tmp"
grep -q '^add int$' "$tmp"
"$bin/crunch-asm" "$tmp" > "$tmp.s"
grep -q '^_start:' "$tmp.s"
as -o "$tmp.o" "$tmp.s"
ld -o "$tmp.exe" "$tmp.o"
set +e
"$tmp.exe"
status=$?
set -e
if [ "$status" -ne 42 ]; then echo "Crunch assembly test: expected exit 42, got $status" >&2; exit 1; fi

# A single explicit return must not be followed by an implicit return.
"$bin/crunch-ir" ../tests/fixtures/parser/hello.bc > "$tmp-hello"
ret_count=$(grep -c '^ret int$' "$tmp-hello" || true)
if [ "$ret_count" -ne 1 ]; then
    echo "Crunch IR test: expected one ret for hello.bc, got $ret_count" >&2
    cat "$tmp-hello" >&2
    exit 1
fi
"$bin/crunch-asm" "$tmp-hello" > "$tmp-hello.s"
if [ "$(grep -c 'mov \$60, %rax' "$tmp-hello.s")" -ne 1 ]; then
    echo "Crunch assembly test: expected exactly one exit syscall sequence" >&2
    cat "$tmp-hello.s" >&2
    exit 1
fi
as -o "$tmp-hello.o" "$tmp-hello.s"
ld -o "$tmp-hello.exe" "$tmp-hello.o"
set +e
"$tmp-hello.exe"
status=$?
set -e
if [ "$status" -ne 0 ]; then echo "Crunch assembly test: expected hello.bc exit 0, got $status" >&2; exit 1; fi

# Control flow, loop break, increment/compound assignment, comparisons and
# short-circuit evaluation (the division by zero must not execute).
for fixture in control_flow comparisons; do
    "$bin/crunch-ir" "../tests/fixtures/ir/$fixture.bc" > "$tmp-$fixture"
    "$bin/crunch-asm" "$tmp-$fixture" > "$tmp-$fixture.s"
    as -o "$tmp-$fixture.o" "$tmp-$fixture.s"
    ld -o "$tmp-$fixture.exe" "$tmp-$fixture.o"
    set +e
    "$tmp-$fixture.exe"
    status=$?
    set -e
    if [ "$status" -ne 42 ]; then
        echo "Crunch control-flow test ($fixture): expected exit 42, got $status" >&2
        cat "$tmp-$fixture" >&2
        exit 1
    fi
done

# Regression: while(condition) must keep its condition on the evaluation stack,
# while redundant adjacent push %rax / pop %rax pairs are eliminated.
"$bin/crunch-ir" ../tests/fixtures/ir/loop_condition.bc > "$tmp-loop_condition"
if grep -q '^dup$' "$tmp-loop_condition"; then
    echo "Crunch IR test: discarded postfix decrement should not duplicate its value" >&2
    cat "$tmp-loop_condition" >&2
    exit 1
fi
"$bin/crunch-asm" "$tmp-loop_condition" > "$tmp-loop_condition.s"
if awk '
    { current = $0; sub(/^[ \t]+/, "", current); sub(/[ \t]+$/, "", current) }
    previous == "push %rax" && current == "pop %rax" { found = 1 }
    { previous = current }
    END { exit found ? 0 : 1 }
' "$tmp-loop_condition.s"; then
    echo "Crunch assembly test: redundant adjacent push/pop pair remains" >&2
    cat "$tmp-loop_condition.s" >&2
    exit 1
fi
as -o "$tmp-loop_condition.o" "$tmp-loop_condition.s"
ld -o "$tmp-loop_condition.exe" "$tmp-loop_condition.o"
set +e
"$tmp-loop_condition.exe"
status=$?
set -e
if [ "$status" -ne 2 ]; then
    echo "Crunch while(condition) test: expected exit 2, got $status" >&2
    cat "$tmp-loop_condition.s" >&2
    exit 1
fi

# Multiple functions, nested calls, register arguments, and stack-passed arguments.
"$bin/crunch-ir" ../tests/fixtures/ir/function_calls.bc > "$tmp-function_calls"
grep -q '^func add 2 int$' "$tmp-function_calls"
grep -q '^call combine 7$' "$tmp-function_calls"
"$bin/crunch-asm" "$tmp-function_calls" > "$tmp-function_calls.s"
as -o "$tmp-function_calls.o" "$tmp-function_calls.s"
ld -o "$tmp-function_calls.exe" "$tmp-function_calls.o"
set +e
"$tmp-function_calls.exe"
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "Crunch function-call test: expected exit 42, got $status" >&2
    cat "$tmp-function_calls" >&2
    cat "$tmp-function_calls.s" >&2
    exit 1
fi

# Numeric widths, casts, IEEE-754 arithmetic/comparisons, real truth values, and mixed ABI classes.
"$bin/crunch-ir" "../tests/fixtures/ir/numeric_types.bc" > "$tmp-numeric_types"
grep -q '^pushr 0x3ff8000000000000$' "$tmp-numeric_types"
grep -q '^cast real int$' "$tmp-numeric_types"
"$bin/crunch-asm" "$tmp-numeric_types" > "$tmp-numeric_types.s"
as -o "$tmp-numeric_types.o" "$tmp-numeric_types.s"
ld -o "$tmp-numeric_types.exe" "$tmp-numeric_types.o"
set +e
"$tmp-numeric_types.exe"
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "Crunch numeric test: expected exit 42, got $status" >&2
    cat "$tmp-numeric_types" >&2
    cat "$tmp-numeric_types.s" >&2
    exit 1
fi

# Pointer address/dereference, typed indirect stores, indexing, pointer arithmetic,
# pointer parameters, and pointer comparisons.
"$bin/crunch-ir" "../tests/fixtures/ir/pointers.bc" > "$tmp-pointers"
grep -q '^addr 0$' "$tmp-pointers"
grep -q '^loadind int$' "$tmp-pointers"
grep -q '^ptradd int$' "$tmp-pointers"
"$bin/crunch-asm" "$tmp-pointers" > "$tmp-pointers.s"
as -o "$tmp-pointers.o" "$tmp-pointers.s"
ld -o "$tmp-pointers.exe" "$tmp-pointers.o"
set +e
"$tmp-pointers.exe"
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "Crunch pointer test: expected exit 42, got $status" >&2
    cat "$tmp-pointers" >&2
    cat "$tmp-pointers.s" >&2
    exit 1
fi

printf '%s\n' 'Crunch IR and x86-64 assembly integration tests: OK' 
