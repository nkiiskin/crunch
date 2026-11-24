#!/bin/sh
set -eu
runner=$1
root=$(CDPATH= cd -- "$(dirname -- "$0")/../fixtures/sema" && pwd)
"$runner" "$root/scopes_valid.bc" expect-ok
for file in duplicate_local.bc undefined_name.bc conflicting_function.bc duplicate_definition.bc duplicate_struct_field.bc duplicate_parameter.bc duplicate_global.bc duplicate_structure.bc undefined_function.bc; do
    "$runner" "$root/$file" expect-error 2>"${TMPDIR:-/tmp}/crunch-sema-test.err"
    if ! grep -Eq 'duplicate|undefined|conflicting' "${TMPDIR:-/tmp}/crunch-sema-test.err"; then
        echo "semantic analyzer did not explain failure for $file" >&2
        cat "${TMPDIR:-/tmp}/crunch-sema-test.err" >&2
        exit 1
    fi
done
"$runner" "$root/types_valid.bc" expect-ok
"$runner" "$root/casts_valid.bc" expect-ok
"$runner" "$root/numeric_bounds_valid.bc" expect-ok
for file in type_bad_assignment.bc type_bad_call.bc type_bad_argument.bc type_bad_return.bc type_bad_void_return.bc type_bad_operator.bc type_bad_member.bc type_bad_array_init.bc type_bad_byte_real_cast.bc type_bad_real_byte_cast.bc numeric_bad_byte_initializer.bc numeric_bad_assignment.bc numeric_bad_int_initializer.bc numeric_bad_long_literal.bc numeric_bad_byte_cast_constant.bc numeric_bad_int_cast_constant.bc; do
    "$runner" "$root/$file" expect-error 2>"${TMPDIR:-/tmp}/crunch-sema-type-test.err"
    if ! grep -Eq 'incompatible|expects|return|operator|member|initializer|casts between byte and real|out of range|exceeds the range of long' "${TMPDIR:-/tmp}/crunch-sema-type-test.err"; then
        echo "type checker did not explain failure for $file" >&2
        cat "${TMPDIR:-/tmp}/crunch-sema-type-test.err" >&2
        exit 1
    fi
done
printf 'semantic symbol/scope and type-checking tests passed\n' 
