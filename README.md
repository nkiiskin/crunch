# Crunch

![Crunchie, the Crunch mascot](Crunchie.jpg)

Crunch is an experimental C compiler suite for Linux x86-64. It includes a
preprocessor, compiler, assembler, linker, archiver and runtime. It is self-hosting,
e.g. it can build itself from the sources.

Crunch is under development: it is not yet a fully conforming C compiler or a
drop-in replacement for GCC. Its native tools and runtime have limitations;
with the `--glibc` option, crunch use's system headers and libraries (e.g. gcc & glibc)
instead of it's own.

## Build

You need Linux x86-64, a C11 compiler such as GCC, GNU make, GNU assembler (as/gas) and
linker, Python 3 and a POSIX shell. From the project root:

```sh
./configure
make
make check
sudo make install
```

This builds the initial compiler at `build/out/crunch` using the host C
compiler. `make check` runs the test suite. `sudo make install` installs it,
by ddefault to prefix /usr/local. Crunch also insstalls it's own man pages.
You can uninstall it by: `sudo make uninstall`

## OPTIONAL: Build the first bootstrap generation

After the initial build, run:

```sh
sh tests/bootstrap-compiler.sh --native "$PWD/bootstrap-stage1"
```

This uses the initial Crunch compiler to build a first-generation compiler
and native tools in `bootstrap-stage1`. The output directory must not already
exist. To use the resulting compiler with its native runtime:

```sh
CRUNCH_CONF_FILE="$PWD/bootstrap-stage1/bootstrap.conf" \
  "$PWD/bootstrap-stage1/crunch" --static -I "$PWD/include" program.c -o program
```

For two generations and alternative GNU-tool or glibc bootstrap modes, see
[build configuration and bootstrap](docs/configure.md#self-hosting-bootstrap).

## Documentation

- [Language specification and limitations](docs/language-spec.md)
- [Compiler architecture](docs/architecture.md)
- [Build configuration and glibc mode](docs/configure.md)
- [Testing](docs/testing.md)

The GNU GPL version 2 text is in [Copying](Copying).
Bugs, questions etc. Niko Kiiskinen <crunch.compiler@yahoo.com>
Have fun! :)
