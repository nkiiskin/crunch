#!/bin/sh
# Compatibility wrapper retained for Makefiles and direct shell invocation.
set -eu
exec "${PYTHON:-python3}" "$(dirname "$0")/run-tests.py" "${1:-out}" "${2:-python3}" "${3:-sh}"
