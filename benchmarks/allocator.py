#!/usr/bin/env python3
"""Compare Crunch and host allocator costs in isolated processes."""
import pathlib
import statistics
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
LIBRARY = ROOT / "build/out/libcrunch.a"
CASES = ((32768, 64), (512, 4096), (32, 65536))
REPEATS = 3


def main():
    if not LIBRARY.is_file():
        raise SystemExit("build/out/libcrunch.a is missing; run ./configure and make first")
    with tempfile.TemporaryDirectory(prefix="crunch-allocator-benchmark-") as directory:
        executable = pathlib.Path(directory) / "allocator"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                        "-O2", str(ROOT / "benchmarks/allocator.c"), str(LIBRARY),
                        "-o", str(executable)], check=True)
        print(f"Median of {REPEATS} separate processes; memory is change from baseline.")
        print("allocator  blocks  bytes/block  requested KiB  alloc ms  touch ms  "
              "free ms  virtual KiB  resident KiB")
        for count, size in CASES:
            for allocator in ("crunch", "system"):
                samples = []
                for _ in range(REPEATS):
                    output = subprocess.run([str(executable), allocator, str(count), str(size)],
                                            check=True, stdout=subprocess.PIPE, text=True)
                    values = output.stdout.split()
                    if len(values) != 5:
                        raise RuntimeError(f"unexpected benchmark output: {output.stdout!r}")
                    samples.append([float(value) for value in values])
                alloc, touch, release, vm, rss = (
                    statistics.median(values) for values in zip(*samples))
                print(f"{allocator:9} {count:7} {size:12} {count * size // 1024:14}"
                      f" {alloc:9.3f} {touch:9.3f} {release:8.3f}"
                      f" {vm:12.0f} {rss:13.0f}")


if __name__ == "__main__":
    main()
