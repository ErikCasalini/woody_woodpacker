# Woody

An x86-64 ELF packer with a freestanding loader embedded in the generated file.

## Build and checks

```sh
make
make test
./packer path/to/executable
./woody
```

The build uses Clang, GNU ld and objcopy. Integration checks also use Python 3.
Tests compile temporary PIE, ET_EXEC and static fixtures, then check execution,
arguments, environment, constructors, BSS, payload metadata and error handling.
Generated fixtures and packed files stay in temporary directories.

To build with the output layout printed by the packer:

```sh
make re PACKER_CFLAGS='-Wall -Werror -Wextra -g -MMD -DPACKER_DEBUG'
```

## Source layout

| Directory | Responsibility |
| --- | --- |
| `src/packer/` | Read and validate the target, extract the embedded stub, calculate the output layout and write `woody`. |
| `src/stub/` | Locate the payload, load its ELF segments and interpreter, update auxv and transfer execution. |
| `src/common/` | Shared payload format, page rounding and ELF mapping range calculations. |
| `src/ft/` | Memory and string primitives usable by both components. |
| `tests/` | Integration checks and existing executable fixtures. |
| `src/elf_parser_tests/` | Separate experimental ELF loader, kept outside the main build. |

The packer's `main.c` and the stub's `execute_packed_elf.c` coordinate their
respective modules. Headers expose the functions used across modules; internal
helpers remain `static`. Types live in the module that owns them: `mapped_file_t`
in `packer/file.h`, `elf_layout_t` in `packer/layout.h`, and `elf_info_t` in
`stub/elf_info.h`. Error codes and messages belong to each component.

Common C sources are compiled independently into `packer_bin/common/` and
`stub_bin/common/`, with the flags of each component. The stub's shared code uses
no libc calls. Page/range helpers operate on accessible ELF headers whose bounds,
ordering and overflow conditions have already been checked by the caller.

The stub linker script places `stub_entry` first and gathers all code and
read-only data into `.stub`. The packer copies that section into the output ELF;
the linked stub must remain executable after copying to a different address.
