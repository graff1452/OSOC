# OSOC

Personal coursework repository for the "一生一芯" (ysyx / One Student One Chip) program.

## Structure

```
OSOC/
├── f/                  F-phase: minirv processor built in Logisim Evolution
├── e/                  E-phase: C, HDL prep, Linux, compilation toolchain, simulators
└── ysyx-workbench/     The official ysyx course framework (see below)
```

## `f/` — F6: Mini RISC-V Processor (Logisim)

A fully functional 32-bit `minirv` processor built from scratch in Logisim Evolution,
implementing 8 RISC-V instructions: `add`, `addi`, `lui`, `lw`, `sw`, `lbu`, `sb`, `jalr`.

- 32-bit PC, 32×32-bit GPRs (x0 hardwired to 0), byte-addressable RAM with byte-enable support
- Memory-mapped RGB Video display, 256×256, wired over `[0x20000000, 0x20040000)`
- Verified against course-provided test programs: `mem.hex`, `sum.hex`, `vga.hex`

To open: load the `.circ` file in [Logisim Evolution](https://github.com/logisim-evolution/logisim-evolution).

## `e/` — E-phase: C, Toolchain, and Simulators

| Folder | Contents |
|---|---|
| `e1/` | Introductory C exercises (pointers, byte layout, endianness) |
| `e4/compilation/` | GCC/Clang preprocessing, lexing, AST, IR, optimization levels, target codegen |
| `e4/sisa/` | `sEMU` — a from-scratch instruction set simulator for the toy 8-bit sISA |
| `e4/minirv/` | `minirvEMU` — a from-scratch instruction set simulator for the real `minirv` ISA (all 8 instructions, `ebreak`-based termination, file-loading from `.hex`/`.bin`) |
| `e4/seq/` | C standard behavior: sequence points, unspecified/undefined behavior |
| `e4/perf/` | Compiler optimization benchmarks (`-O0` vs `-O1`/`-O2`) |

Most files are standalone and can be built directly:
```bash
gcc -Wall -Wextra <file>.c -o <output>
```

## `ysyx-workbench/` — Official Course Framework

Cloned from [OSCPU/ysyx-workbench](https://github.com/OSCPU/ysyx-workbench). This is the
scaffold the course builds up incrementally via `init.sh`.

### Setup on a new machine

```bash
cd ysyx-workbench
bash init.sh abstract-machine   # pulls in AM + sets AM_HOME
source ~/.bashrc
```

### Related repos (not included here — see below)

`init.sh` clones some subprojects as **independent git repositories**, gitignored by
`ysyx-workbench` itself. These live in my own separate repos:

- **[am-kernels](https://github.com/graff1452/am-kernels)** — AM test programs + my own
  screensaver and GUI-enabled `minirvEMU`
- **[fceux-am](https://github.com/graff1452/fceux-am)** — NES emulator ported to AM

To restore them on a fresh clone:
```bash
git clone git@github.com:graff1452/am-kernels.git
git clone git@github.com:graff1452/fceux-am.git
```
(Place both inside `ysyx-workbench/`, matching the original layout, so `AM_HOME`-relative
paths in their Makefiles resolve correctly.)

### `npc/` — RTL Reimplementation (D4, in progress)

Skeleton for reimplementing the `minirv` processor from `f/` in real Verilog/RTL, using
Verilator. Not yet started.

## Notes for future me

- `ysyx-workbench`'s Makefile has a `STUNAME`/`STUID` field and a hidden git-tracer
  mechanism (`tracer-ysyx` branch) — don't touch the tracer machinery.
- The E4 minirvEMU's memory-mapped video decode (`[0x20000000, 0x20040000)`, X at bits
  `[9:2]`, Y at bits `[17:10]`, 24-bit RGB) matches the F6 Logisim hardware exactly —
  verified by running the same `vga.hex` on both and getting a pixel-identical image.
