#!/usr/bin/env python3
"""Generate one ordinary-linkage trampoline per fixed-ABI runtime function."""

import pathlib
import re
import sys


def main():
    """Print runtime names or the assembly for one fallback trampoline.

    Returns:
        None; exits with an error for an unknown name or invalid invocation.
    """
    if len(sys.argv) != 3:
        raise SystemExit("usage: runtime-fallbacks.py HEADER --list|NAME")
    header = pathlib.Path(sys.argv[1]).read_text()
    names = re.findall(r'X\("([A-Za-z_][A-Za-z_0-9]*)",\s*[0-9]+,\s*\'.\',\s*"[ip]*"\)', header)
    if not names or len(names) != len(set(names)):
        raise SystemExit("invalid runtime routine list")
    if sys.argv[2] == "--list":
        print(" ".join(names))
        return
    name = sys.argv[2]
    if name not in names:
        raise SystemExit(f"unknown runtime routine: {name}")
    marker = (f"\t.globl __crunch_fallback_{name}\n"
              f"\t.hidden __crunch_fallback_{name}\n"
              f"\t.type __crunch_fallback_{name}, @function\n"
              f"__crunch_fallback_{name}:\n")
    body = f"\tjmp __crunch_{name}\n"
    if name in ("printf", "fprintf", "sprintf", "snprintf", "scanf", "fscanf"):
        fixed = {"printf": 1, "fprintf": 2, "sprintf": 2, "snprintf": 3, "scanf": 1, "fscanf": 2}[name]
        body = "\tsub $216, %rsp\n"
        for i, register in enumerate(("rdi", "rsi", "rdx", "rcx", "r8", "r9")):
            body += f"\tmov %{register}, {32 + 8 * i}(%rsp)\n"
        for i in range(8):
            body += f"\tmovups %xmm{i}, {80 + 16 * i}(%rsp)\n"
        body += (f"\tmovl ${fixed * 8}, (%rsp)\n"
                 "\tmovl $48, 4(%rsp)\n"
                 "\tlea 224(%rsp), %rax\n"
                 "\tmov %rax, 8(%rsp)\n"
                 "\tlea 32(%rsp), %rax\n"
                 "\tmov %rax, 16(%rsp)\n")
        va_register = ("rsi", "rdx", "rcx")[fixed - 1]
        body += (f"\tlea (%rsp), %{va_register}\n"
                 f"\tcall __crunch_v{name}\n"
                 "\tadd $216, %rsp\n\tret\n")
    print(f"\t.text\n\t.weak {name}\n\t.type {name}, @function\n{name}:\n{marker}"
          f"{body}\t.size {name}, .-{name}\n"
          f'\t.section .rodata.__crunch_guard_{name},"a",@progbits\n'
          f"\t.globl __crunch_guard_{name}\n\t.hidden __crunch_guard_{name}\n"
          f"__crunch_guard_{name}:\n\t.byte 0\n"
          '\t.section .note.GNU-stack,"",@progbits')


if __name__ == "__main__":
    main()
