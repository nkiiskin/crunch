#!/usr/bin/env python3
"""Inventory headers visible to a GCC C compiler and probe standalone inclusion."""

import argparse
import collections
import json
import os
import pathlib
import shutil
import signal
import subprocess
import sys


PROFILES = {
    "default": [],
    "posix": ["-D_POSIX_C_SOURCE=200809L"],
    "gnu": ["-D_GNU_SOURCE=1"],
}
BASE_FLAGS = ["-std=gnu11", "-funsigned-char"]


def compiler_output(compiler, args, env, input_text=""):
    """Run a GCC discovery command, returning its captured output.

    Args:
        compiler: Path to the compiler driver.
        args: Compiler options.
        env: Controlled subprocess environment.
        input_text: Text supplied on standard input.
    Returns:
        Completed process; raises on failure or timeout.
    """
    return subprocess.run([compiler, *args], input=input_text, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          env=env, timeout=30, check=True, errors="replace")


def discover_directories(compiler, env):
    """Read GCC's ordered angle-include search directories.

    Args:
        compiler: Path to the compiler driver.
        env: Controlled subprocess environment.
    Returns:
        Ordered list of existing directory paths; raises on unparseable output.
    """
    result = compiler_output(compiler, [*BASE_FLAGS, "-E", "-x", "c", "-", "-v"], env)
    lines = result.stderr.splitlines()
    start = "#include <...> search starts here:"
    end = "End of search list."
    try:
        first = lines.index(start) + 1
        last = lines.index(end, first)
    except ValueError as exc:
        raise ValueError("could not parse GCC include search list") from exc
    directories = [line.strip() for line in lines[first:last]]
    if not directories or any(not pathlib.Path(path).is_dir() for path in directories):
        raise ValueError("GCC returned an empty or invalid include search list")
    return directories


def collect_headers(directories):
    """Find visible .h files and shadowed candidates in search order.

    Args:
        directories: Ordered GCC angle-include directories.
    Returns:
        Header records ordered by include name.
    """
    headers = {}

    def scan_error(exc):
        """Propagate directory traversal errors rather than silently omitting files.

        Args:
            exc: Error reported by os.walk.
        Returns:
            Nothing; always raises the supplied error.
        """
        raise exc

    for root in directories:
        root_path = pathlib.Path(root)
        for current, dirs, files in os.walk(root, followlinks=True, onerror=scan_error):
            current_path = pathlib.Path(current)
            ancestors = {path.resolve() for path in [current_path, *current_path.parents]
                         if path == root_path or root_path in path.parents}
            dirs[:] = sorted(name for name in dirs
                             if (current_path / name).resolve() not in ancestors)
            for name in sorted(files):
                path = current_path / name
                if not name.endswith(".h") or not path.is_file():
                    continue
                relative = path.relative_to(root_path).as_posix()
                if relative not in headers:
                    parts = pathlib.PurePosixPath(relative).parts
                    internal = (any(part in {"bits", "sysdeps", "internal"} for part in parts)
                                or name.startswith("__") or relative.startswith("gnu/stubs"))
                    headers[relative] = {
                        "name": relative,
                        "selected": str(path),
                        "shadowed": [],
                        "classification": "internal-candidate" if internal else "public-candidate",
                    }
                else:
                    headers[relative]["shadowed"].append(str(path))
    return [headers[name] for name in sorted(headers)]


def probe_header(compiler, header, flags, env, timeout):
    """Test whether GCC can preprocess a header by itself in one profile.

    Args:
        compiler: Path to the compiler driver.
        header: Header include name.
        flags: Profile-specific compiler options.
        env: Controlled subprocess environment.
        timeout: Per-header timeout in seconds.
    Returns:
        Status and bounded diagnostic text.
    """
    if any(char in header for char in '>\n\r\\'):
        return {"status": "unprobeable", "diagnostic": "unsafe include spelling"}
    process = subprocess.Popen([compiler, *BASE_FLAGS, *flags, "-E", "-x", "c", "-",
                                "-o", os.devnull],
                               stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, text=True, env=env,
                               start_new_session=True, errors="replace")
    try:
        _, stderr = process.communicate(input=f"#include <{header}>\n", timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
        return {"status": "timeout", "diagnostic": f"exceeded {timeout} seconds"}
    if process.returncode < 0:
        return {"status": "crash", "diagnostic": f"signal {-process.returncode}"}
    if process.returncode:
        return {"status": "error", "diagnostic": stderr[:2000]}
    return {"status": "ok", "diagnostic": stderr[:2000]}


def build_inventory(compiler, timeout):
    """Discover headers and classify their standalone GCC preprocessing results.

    Args:
        compiler: GCC driver name or path.
        timeout: Per-header, per-profile timeout in seconds.
    Returns:
        JSON-serializable inventory and provenance.
    """
    path = shutil.which(compiler)
    if not path:
        raise ValueError(f"compiler not found: {compiler}")
    path = os.path.abspath(path)
    env = dict(os.environ, LC_ALL="C")
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH"):
        env.pop(name, None)
    directories = discover_directories(path, env)
    headers = collect_headers(directories)
    for index, header in enumerate(headers, 1):
        if index == 1 or index % 500 == 0:
            print(f"probing header {index}/{len(headers)}", file=sys.stderr)
        results = {name: probe_header(path, header["name"], flags, env, timeout)
                   for name, flags in PROFILES.items()}
        header["profiles"] = results
        if results["default"]["status"] == "ok":
            header["standalone"] = "default"
        elif any(results[name]["status"] == "ok" for name in ("posix", "gnu")):
            header["standalone"] = "feature-profile-only"
        else:
            header["standalone"] = "not-established"
    libc = os.confstr("CS_GNU_LIBC_VERSION") if "CS_GNU_LIBC_VERSION" in os.confstr_names else None
    return {
        "format": 1,
        "compiler": path,
        "compiler_version": compiler_output(path, ["-dumpfullversion", "-dumpversion"], env).stdout.strip(),
        "target": compiler_output(path, ["-dumpmachine"], env).stdout.strip(),
        "host_libc": libc,
        "base_flags": BASE_FLAGS,
        "profiles": PROFILES,
        "include_directories": directories,
        "summary": {
            "header_count": len(headers),
            "shadowed_candidates": sum(len(header["shadowed"]) for header in headers),
            "classification": dict(sorted(collections.Counter(
                header["classification"] for header in headers).items())),
            "standalone": dict(sorted(collections.Counter(
                header["standalone"] for header in headers).items())),
            "profiles": {name: dict(sorted(collections.Counter(
                header["profiles"][name]["status"] for header in headers).items()))
                         for name in PROFILES},
        },
        "headers": headers,
    }


def main():
    """Parse options and write the inventory as JSON.

    Returns:
        Zero on success, or nonzero on discovery and I/O errors.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc", help="GCC driver (default: gcc)")
    parser.add_argument("--timeout", type=float, default=5.0,
                        help="per-header and profile timeout in seconds (default: 5)")
    args = parser.parse_args()
    if not 0 < args.timeout < float("inf"):
        parser.error("--timeout must be a positive finite number")
    try:
        json.dump(build_inventory(args.cc, args.timeout), sys.stdout, indent=2)
        sys.stdout.write("\n")
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f"gnu_header_inventory: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
