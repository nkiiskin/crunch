#!/usr/bin/env python3
"""Check function documentation in maintained C, header, Python, and assembly files."""
from __future__ import annotations

import ast
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def mask_c(text):
    """Hide comments and literals while preserving offsets and line breaks.

    Args:
        text: C source text to scan.
    Returns:
        Source text with comments and literals replaced by whitespace.
    """
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda match: re.sub(r"[^\n]", " ", match.group()),
                  text, flags=re.S)


def c_functions(text, declarations=False):
    """Find top-level C function signatures, including multiline declarations.

    Args:
        text: C source text.
        declarations: Whether to include prototypes, as required for headers.
    Returns:
        List of (offset, name, parameter names, return type) tuples.
    """
    masked = mask_c(text)
    masked = re.sub(r"(?m)^[ \t]*\#(?:[^\n]*\\\n)*[^\n]*",
                    lambda match: re.sub(r"[^\n]", " ", match.group()), masked)
    pattern = re.compile(
        r"(?m)^[ \t]*(?P<result>(?:[A-Za-z_]\w*\s+)*[A-Za-z_]\w*[\s*]+)"
        r"(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}]*?)\)\s*(?P<end>[{;])")
    functions = []
    depth = cursor = 0
    for match in pattern.finditer(masked):
        prefix = masked[cursor:match.start()]
        depth += prefix.count("{") - prefix.count("}")
        cursor = match.start()
        if depth or "typedef" in match["result"].split() or (match["end"] == ";" and not declarations):
            continue
        parameters = []
        for argument in match["args"].split(","):
            argument = argument.strip()
            if not argument or argument in {"void", "..."}:
                continue
            argument = re.sub(r"\[[^\]]*\]", "", argument)
            names = re.findall(r"[A-Za-z_]\w*", argument)
            if names:
                parameters.append(names[-1])
        functions.append((match.start(), match["name"], parameters, match["result"]))
    return functions


def check_c(text, declarations=False):
    """Validate C function comments and their parameter names.

    Args:
        text: C source text to check.
        declarations: Whether public header prototypes should be checked too.
    Returns:
        List of (line number, diagnostic) tuples.
    """
    errors = []
    for offset, name, parameters, _ in c_functions(text, declarations):
        line = text.count("\n", 0, offset) + 1
        comment = re.search(r"/\*\*((?:(?!\*/).)*)\*/\s*$",
                            text[:offset], re.S)
        if not comment:
            errors.append((line, f"{name}: missing function documentation"))
            continue
        doc = comment.group(1)
        if not re.search(r"@brief\s+\S", doc):
            errors.append((line, f"{name}: missing @brief"))
        documented = re.findall(r"@param(?:\[[^\]]*\])?\s+(\w+)", doc)
        if documented != parameters:
            errors.append((line, f"{name}: @param names {documented} != {parameters}"))
        if not re.search(r"@return\s+\S", doc):
            # Existing no-argument unit tests describe their output in @note.
            if not (not parameters and "@note" in doc and
                    re.search(r"\breturns?\b", doc, re.I)):
                errors.append((line, f"{name}: missing @return"))
    return errors


def check_python(text):
    """Validate docstrings for every Python function, including nested helpers.

    Args:
        text: Python source text to parse.
    Returns:
        List of (line number, diagnostic) tuples.
    """
    errors = []
    for node in ast.walk(ast.parse(text)):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        doc = ast.get_docstring(node)
        if not doc:
            errors.append((node.lineno, f"{node.name}: missing docstring"))
            continue
        args = node.args
        names = [arg.arg for arg in args.posonlyargs + args.args + args.kwonlyargs]
        names += [arg.arg for arg in (args.vararg, args.kwarg) if arg]
        documented = re.findall(r"(?m)^    (\w+):", doc)
        if documented != names:
            errors.append((node.lineno,
                           f"{node.name}: documented arguments {documented} != {names}"))
        if not re.search(r"(?m)^Returns:\n    \S", doc):
            errors.append((node.lineno, f"{node.name}: missing Returns contract"))
    return errors


def check_assembly(text):
    """Check that each exported assembly entry point has an ABI contract.

    Args:
        text: GNU assembly source text to check.
    Returns:
        List of (line number, diagnostic) tuples.
    """
    errors = []
    for match in re.finditer(r"(?m)^\s*\.globl\s+(\w+)", text):
        if re.search(r"(?m)^\s*\.type\s+" + re.escape(match.group(1)) +
                     r"\s*,\s*@object\b", text):
            continue
        lines = text[:match.start()].splitlines()[-6:]
        doc = "\n".join(lines)
        if not re.search(r"#.*Inputs?:", doc) or not re.search(r"#.*Output:", doc):
            errors.append((text.count("\n", 0, match.start()) + 1,
                           f"{match.group(1)}: missing assembly input/output contract"))
    return errors


def main():
    """Check all maintained function-bearing source files.

    Returns:
        Zero when all documentation contracts pass, otherwise one.
    """
    errors = []
    for directory, pattern, check in (
            ("src", "*.c", check_c), ("tests", "*.c", check_c),
            ("include", "*.h", lambda text: check_c(text, declarations=True)),
            ("tests", "*.py", check_python), ("lib", "*.s", check_assembly)):
        for path in sorted((ROOT / directory).rglob(pattern)):
            # Imported experimental tools predate the function-doc contract.
            if path in (ROOT / "src/assembler/assembler.c", ROOT / "src/linker/crunch-ld.c",
                        ROOT / "src/archiver/crunch-ar.c"):
                continue
            if directory == "include" and path.parent == ROOT / "include/test":
                continue
            if directory == "tests" and ROOT / "tests/fixtures" in path.parents:
                continue
            for line, message in check(path.read_text()):
                errors.append(f"{path.relative_to(ROOT)}:{line}: {message}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Function documentation contracts: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
