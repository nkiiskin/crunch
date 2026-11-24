#!/usr/bin/env python3
"""Compare GCC and Crunch preprocessing of independently includable C headers."""

import argparse
import collections
import hashlib
import json
import os
import pathlib
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile

from gnu_header_inventory import BASE_FLAGS, PROFILES, compiler_output, discover_directories


REPRESENTATIVE = ("stddef.h", "stdint.h", "stdio.h", "stdlib.h", "string.h",
                  "errno.h", "fcntl.h", "unistd.h", "sys/types.h", "sys/stat.h",
                  "time.h", "math.h", "limits.h")
MACRO_PROBES = {
    "stddef.h": ("NULL",),
    "stdint.h": ("SIZE_MAX", "INT32_MAX", "UINT32_MAX"),
    "stdio.h": ("EOF", "BUFSIZ", "FOPEN_MAX"),
    "stdlib.h": ("EXIT_SUCCESS", "EXIT_FAILURE", "RAND_MAX"),
    "errno.h": ("EINVAL", "ENOMEM"),
    "fcntl.h": ("O_RDONLY", "O_CLOEXEC"),
    "unistd.h": ("_POSIX_VERSION",),
    "sys/stat.h": ("S_IFMT", "S_IFREG"),
    "time.h": ("CLOCKS_PER_SEC", "CLOCK_MONOTONIC"),
    "math.h": ("HUGE_VAL", "M_PI"),
    "limits.h": ("CHAR_BIT", "PATH_MAX"),
}
INTEGER_EXPECTATIONS = {
    "SIZE_MAX": "18446744073709551615UL",
    "INT32_MAX": "2147483647",
    "UINT32_MAX": "4294967295U",
    "EOF": "(-1)",
    "EXIT_SUCCESS": "0",
    "EXIT_FAILURE": "1",
    "O_RDONLY": "0",
    "CHAR_BIT": "8",
}
OUTPUT_LIMIT = 16 * 1024 * 1024
TOKEN_LIMIT = 500000
TOKENS = re.compile(
    r"(?P<space>[ \t\n\r\v\f]+)|(?P<comment>/\*.*?\*/|//[^\n]*)|"
    r"""(?P<literal>(?:u8|u|U|L)?(?:"(?:\\[^\n]|[^"\\\n])*"|'(?:\\[^\n]|[^'\\\n])*'))|"""
    r"(?P<number>(?:[0-9]|\.[0-9])(?:[eEpP][+-]|[A-Za-z0-9_.]|\\u[0-9a-fA-F]{4}|\\U[0-9a-fA-F]{8})*)|"
    r"(?P<identifier>(?:[A-Za-z_$\u0080-\U0010ffff]|\\u[0-9a-fA-F]{4}|\\U[0-9a-fA-F]{8})"
    r"(?:[A-Za-z0-9_$\u0080-\U0010ffff]|\\u[0-9a-fA-F]{4}|\\U[0-9a-fA-F]{8})*)|"
    r"(?P<punct>%:%:|>>=|<<=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||"
    r"\*=|/=|%=|\+=|-=|&=|\^=|\|=|##|<:|:>|<%|%>|%:)|(?P<other>.)",
    re.DOTALL,
)


def tokenize(text):
    """Split preprocessed C into spelling-preserving preprocessing tokens.

    Args:
        text: Preprocessor output including optional comments.
    Returns:
        Token spellings; raises ValueError for unterminated comments or literals.
    """
    tokens = []
    offset = 0
    while offset < len(text):
        match = TOKENS.match(text, offset)
        if not match:
            raise ValueError(f"unrecognized input at offset {offset}")
        if match.lastgroup == "other" and (
                text.startswith("/*", offset) or text[offset] in "\"'"):
            raise ValueError(f"unterminated comment or literal at offset {offset}")
        if match.lastgroup not in ("space", "comment"):
            tokens.append(match.group())
            if len(tokens) > TOKEN_LIMIT:
                raise ValueError(f"exceeded {TOKEN_LIMIT} preprocessing tokens")
        offset = match.end()
    return tokens


def run_preprocessor(args, source, env, timeout):
    """Run one preprocessor with bounded output and process-tree timeout.

    Args:
        args: Executable and command arguments.
        source: Source bytes supplied on standard input.
        env: Controlled subprocess environment.
        timeout: Maximum run time in seconds.
    Returns:
        Status, captured output bytes, and bounded diagnostics.
    """
    def limit_output():
        """Limit each child output file to prevent unbounded disk usage.

        Returns:
            None; sets the process file-size limit.
        """
        resource.setrlimit(resource.RLIMIT_FSIZE, (OUTPUT_LIMIT, OUTPUT_LIMIT))

    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=output, stderr=errors,
                                   env=env, start_new_session=True, preexec_fn=limit_output)
        try:
            process.communicate(input=source, timeout=timeout)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.communicate()
            return {"status": "timeout", "diagnostic": f"exceeded {timeout} seconds"}, b""
        output_size = output.tell()
        error_size = errors.tell()
        errors.seek(0)
        diagnostic = errors.read(2000).decode("utf-8", "replace")
        if output_size >= OUTPUT_LIMIT or error_size >= OUTPUT_LIMIT:
            return {"status": "output-limit", "diagnostic": diagnostic}, b""
        if process.returncode < 0:
            return {"status": "crash", "diagnostic": f"signal {-process.returncode}: {diagnostic}"}, b""
        if process.returncode:
            return {"status": "error", "diagnostic": diagnostic}, b""
        output.seek(0)
        return {"status": "ok", "diagnostic": diagnostic}, output.read()


def probe_source(macros):
    """Construct standalone definedness and expansion probes for object-like macros.

    Args:
        macros: Macro names to probe.
    Returns:
        Source bytes appending uniquely marked results to the header output.
    """
    expansions = "".join(
        f"#ifdef {name}\n"
        f"__CRUNCH_PROBE_BEGIN_{name} {name} __CRUNCH_PROBE_END_{name}\n"
        "#else\n"
        f"__CRUNCH_PROBE_BEGIN_{name} __CRUNCH_PROBE_UNDEFINED __CRUNCH_PROBE_END_{name}\n"
        "#endif\n" for name in macros)
    values = "".join(
        f"#ifdef {name}\n#if ({name}) == ({INTEGER_EXPECTATIONS[name]})\n"
        f"__CRUNCH_VALUE_BEGIN_{name} 1 __CRUNCH_VALUE_END_{name}\n"
        "#else\n"
        f"__CRUNCH_VALUE_BEGIN_{name} 0 __CRUNCH_VALUE_END_{name}\n"
        "#endif\n#else\n"
        f"__CRUNCH_VALUE_BEGIN_{name} __CRUNCH_PROBE_UNDEFINED __CRUNCH_VALUE_END_{name}\n"
        "#endif\n" for name in macros if name in INTEGER_EXPECTATIONS)
    return (expansions + values).encode("ascii")


def split_probes(tokens, macros):
    """Separate appended macro probes from a header's token stream.

    Args:
        tokens: Tokenized preprocessor output.
        macros: Macro names in probe order.
    Returns:
        Header tokens and a mapping of macro names to definedness and values.
    """
    if not macros:
        return tokens, {}
    marker = f"__CRUNCH_PROBE_BEGIN_{macros[0]}"
    try:
        offset = tokens.index(marker)
        header = tokens[:offset]
        probes = {}
        for name in macros:
            begin = f"__CRUNCH_PROBE_BEGIN_{name}"
            end = f"__CRUNCH_PROBE_END_{name}"
            if tokens[offset] != begin:
                raise ValueError(f"missing macro probe {name}")
            finish = tokens.index(end, offset + 1)
            value = tokens[offset + 1:finish]
            defined = value != ["__CRUNCH_PROBE_UNDEFINED"]
            probes[name] = {"defined": defined, "value": value if defined else []}
            offset = finish + 1
        for name in macros:
            if name not in INTEGER_EXPECTATIONS:
                continue
            begin = f"__CRUNCH_VALUE_BEGIN_{name}"
            end = f"__CRUNCH_VALUE_END_{name}"
            if tokens[offset] != begin:
                raise ValueError(f"missing numeric probe {name}")
            finish = tokens.index(end, offset + 1)
            value = tokens[offset + 1:finish]
            if value not in (["0"], ["1"], ["__CRUNCH_PROBE_UNDEFINED"]):
                raise ValueError(f"invalid numeric probe {name}")
            probes[name]["matches_expected"] = None if value[0] == "__CRUNCH_PROBE_UNDEFINED" else value[0] == "1"
            offset = finish + 1
        if offset != len(tokens):
            raise ValueError("unexpected tokens following macro probes")
        return header, probes
    except (ValueError, IndexError) as exc:
        raise ValueError(f"malformed macro probe output: {exc}") from exc


def compare_tokens(reference, actual, macros=()):
    """Compare header token spellings and selected macro expansions.

    Args:
        reference: GCC output bytes.
        actual: Crunch output bytes.
        macros: Names of appended macro probes.
    Returns:
        Header equality, first-difference context and macro comparison.
    """
    expected, expected_macros = split_probes(
        tokenize(reference.decode("utf-8", "surrogateescape")), macros)
    observed, observed_macros = split_probes(
        tokenize(actual.decode("utf-8", "surrogateescape")), macros)
    result = {
        "status": "equal" if expected == observed else "different",
        "gcc_tokens": len(expected),
        "crunch_tokens": len(observed),
        "gcc_sha256": hashlib.sha256(b"\0".join(
            token.encode("utf-8", "surrogateescape") for token in expected)).hexdigest(),
        "crunch_sha256": hashlib.sha256(b"\0".join(
            token.encode("utf-8", "surrogateescape") for token in observed)).hexdigest(),
    }
    if expected != observed:
        index = next((i for i, (a, b) in enumerate(zip(expected, observed)) if a != b),
                     min(len(expected), len(observed)))
        def display(token):
            """Limit the size of one token in a difference report.

            Args:
                token: Preprocessing token spelling.
            Returns:
                Token spelling truncated after 160 characters if necessary.
            """
            return token if len(token) <= 160 else token[:160] + "..."

        result["first_difference"] = {
            "index": index,
            "gcc_token": display(expected[index]) if index < len(expected) else None,
            "crunch_token": display(observed[index]) if index < len(observed) else None,
            "gcc": [display(token) for token in expected[max(0, index - 3):index + 4]],
            "crunch": [display(token) for token in observed[max(0, index - 3):index + 4]],
        }
    if macros:
        result["macros"] = {
            name: {
                "status": "equal" if
                          (expected_macros[name]["defined"], expected_macros[name]["value"]) ==
                          (observed_macros[name]["defined"], observed_macros[name]["value"])
                          else "different",
                "gcc": {"defined": expected_macros[name]["defined"],
                        "tokens": [token[:160] for token in expected_macros[name]["value"][:64]],
                        "token_count": len(expected_macros[name]["value"])},
                "crunch": {"defined": observed_macros[name]["defined"],
                           "tokens": [token[:160] for token in observed_macros[name]["value"][:64]],
                           "token_count": len(observed_macros[name]["value"])},
            } for name in macros
        }
        for name in macros:
            if name in INTEGER_EXPECTATIONS:
                gcc_value = expected_macros[name]["matches_expected"]
                crunch_value = observed_macros[name]["matches_expected"]
                if gcc_value is None and crunch_value is None:
                    check_status = "both-undefined"
                elif gcc_value is True and crunch_value is True:
                    check_status = "equal"
                elif gcc_value is False and crunch_value is False:
                    check_status = "both-not-expected"
                else:
                    check_status = "different"
                result["macros"][name]["integer_check"] = {
                    "expected": INTEGER_EXPECTATIONS[name],
                    "gcc_matches": gcc_value,
                    "crunch_matches": crunch_value,
                    "status": check_status,
                }
    return result


def compare_headers(compiler, crunch, names, inventory, timeout):
    """Run selected header/profile comparisons with recorded provenance.

    Args:
        compiler: GCC driver name or path.
        crunch: Crunch driver name or path.
        names: Header names to compare.
        inventory: Optional previously generated header inventory.
        timeout: Per-process timeout in seconds.
    Returns:
        JSON-serializable comparison report.
    """
    compiler = shutil.which(compiler)
    crunch = shutil.which(crunch)
    if not compiler or not crunch:
        raise ValueError("GCC or Crunch driver not found")
    compiler = os.path.abspath(compiler)
    crunch = os.path.abspath(crunch)
    env = dict(os.environ, LC_ALL="C", CRUNCH_CONF_FILE=os.devnull,
               CRUNCH_GLIBC_CC=compiler)
    for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                "CRUNCH_INCLUDE_PATH"):
        env.pop(key, None)
    directories = discover_directories(compiler, env)
    version = compiler_output(compiler, ["-dumpfullversion", "-dumpversion"], env).stdout.strip()
    target = compiler_output(compiler, ["-dumpmachine"], env).stdout.strip()
    if not target.startswith("x86_64-") or "linux" not in target:
        raise ValueError(f"GCC target is not Linux x86-64: {target}")
    entries = {}
    if inventory is not None:
        if not isinstance(inventory, dict) or not isinstance(inventory.get("headers"), list):
            raise ValueError("invalid header inventory")
        if (inventory.get("format") != 1 or inventory.get("compiler") != compiler or
                inventory.get("compiler_version") != version or inventory.get("target") != target or
                inventory.get("include_directories") != directories or
                inventory.get("base_flags") != BASE_FLAGS or inventory.get("profiles") != PROFILES):
            raise ValueError("inventory compiler, flags or search paths changed; regenerate it")
        entries = {entry["name"]: entry for entry in inventory["headers"]}
        if len(entries) != len(inventory["headers"]):
            raise ValueError("duplicate header names in inventory")
    results = []
    for number, name in enumerate(names, 1):
        if number == 1 or number % 100 == 0:
            print(f"comparing header {number}/{len(names)}", file=sys.stderr)
        path = pathlib.PurePosixPath(name)
        unsafe = (path.is_absolute() or any(part in (".", "..") for part in name.split("/")) or
                  any(char in name for char in '>\n\r\\') or not name)
        if unsafe and inventory is not None and name in entries and all(
                entries[name]["profiles"][profile]["status"] == "unprobeable" for profile in PROFILES):
            results.extend({"header": name, "selected": entries[name]["selected"],
                            "profile": profile, "status": "inventory-ineligible",
                            "inventory_status": "unprobeable"} for profile in PROFILES)
            continue
        if unsafe:
            raise ValueError(f"unsafe header name: {name!r}")
        if inventory is not None and name not in entries:
            raise ValueError(f"header absent from inventory: {name}")
        selected = next((str(pathlib.Path(directory) / name) for directory in directories
                         if (pathlib.Path(directory) / name).is_file()), None)
        if inventory is not None and selected != entries[name]["selected"]:
            raise ValueError(f"header search changed for {name}; regenerate inventory")
        macros = MACRO_PROBES.get(name, ())
        source = b"#include <" + os.fsencode(name) + b">\n" + probe_source(macros)
        for profile, flags in PROFILES.items():
            record = {"header": name, "selected": selected, "profile": profile}
            if inventory is not None:
                record["inventory_status"] = entries[name]["profiles"][profile]["status"]
            if inventory is not None and record["inventory_status"] == "unprobeable":
                record["status"] = "inventory-ineligible"
                results.append(record)
                continue
            gcc_args = [compiler, *BASE_FLAGS, *flags, "-E", "-P", "-x", "c", "-"]
            crunch_args = [crunch, "--glibc", "-E", *flags, "-"]
            gcc, gcc_output = run_preprocessor(gcc_args, source, env, timeout)
            record["gcc"] = gcc
            if gcc["status"] != "ok":
                record["status"] = "gcc-" + gcc["status"]
                results.append(record)
                continue
            actual, crunch_output = run_preprocessor(crunch_args, source, env, timeout)
            record["crunch"] = actual
            if actual["status"] != "ok":
                record["status"] = "crunch-" + actual["status"]
            else:
                try:
                    record["comparison"] = compare_tokens(gcc_output, crunch_output, macros)
                    record["status"] = record["comparison"]["status"]
                except ValueError as exc:
                    record["status"] = "tokenization-error"
                    record["diagnostic"] = str(exc)
            results.append(record)
    return {
        "format": 1,
        "compiler": compiler,
        "compiler_version": version,
        "target": target,
        "crunch": crunch,
        "base_flags": BASE_FLAGS,
        "profiles": PROFILES,
        "include_directories": directories,
        "summary": dict(sorted(collections.Counter(item["status"] for item in results).items())),
        "macro_summary": dict(sorted(collections.Counter(
            macro["status"] for item in results
            for macro in item.get("comparison", {}).get("macros", {}).values()).items())),
        "integer_check_summary": dict(sorted(collections.Counter(
            macro["integer_check"]["status"] for item in results
            for macro in item.get("comparison", {}).get("macros", {}).values()
            if "integer_check" in macro).items())),
        "results": results,
    }


def main():
    """Parse options and write the comparison report as JSON.

    Returns:
        Zero after a completed scan; nonzero for invalid input or infrastructure errors.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", help="GCC driver (default: inventory compiler or gcc)")
    parser.add_argument("--crunch", default="build/out/crunch", help="Crunch driver")
    parser.add_argument("--inventory", help="header inventory JSON")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--all", action="store_true", help="scan the entire inventory")
    selection.add_argument("--header", action="append", help="compare a specific header (repeatable)")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-process timeout in seconds")
    args = parser.parse_args()
    if not 0 < args.timeout < float("inf"):
        parser.error("--timeout must be a positive finite number")
    if args.all and not args.inventory:
        parser.error("--all requires --inventory")
    try:
        inventory = json.loads(pathlib.Path(args.inventory).read_text()) if args.inventory else None
        if inventory is not None and (not isinstance(inventory, dict) or
                                      not isinstance(inventory.get("headers"), list)):
            raise ValueError("invalid header inventory")
        names = ([entry["name"] for entry in inventory["headers"]] if args.all
                 else args.header if args.header else list(REPRESENTATIVE))
        compiler = args.cc or (inventory["compiler"] if inventory else "gcc")
        json.dump(compare_headers(compiler, args.crunch, names, inventory, args.timeout),
                  sys.stdout, indent=2)
        sys.stdout.write("\n")
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        print(f"gnu_header_compare: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
