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

### Related repos (not included here)

`init.sh` clones some subprojects as **independent git repositories**, gitignored by
`ysyx-workbench` itself. These live in my own separate repos, cloned inside
`ysyx-workbench/`, alongside `abstract-machine/` and `npc/`:

- **[am-kernels](https://github.com/graff1452/am-kernels)** — AM test programs + my own
  screensaver and GUI-enabled `minirvEMU`
- **[fceux-am](https://github.com/graff1452/fceux-am)** — NES emulator ported to AM

### Full setup on a brand-new device

```bash
# 1. Generate an SSH key on this machine and add its public key to GitHub
#    (Settings -> SSH and GPG keys -> New SSH key)
ssh-keygen -t ed25519 -C "your_email@example.com"
cat ~/.ssh/id_ed25519.pub

# 2. Clone this repo
cd ~/Desktop
git clone git@github.com:graff1452/OSOC.git

# 3. Clone the two subprojects into ysyx-workbench/
cd OSOC/ysyx-workbench
git clone git@github.com:graff1452/am-kernels.git
git clone git@github.com:graff1452/fceux-am.git

# 4. Point AM_HOME at the abstract-machine that came with this repo
#    (already present -- no need to re-run init.sh for it)
echo "export AM_HOME=$(readlink -f abstract-machine)" >> ~/.bashrc
source ~/.bashrc

# 5. (Optional) place a legally-obtained ROM to run fceux-am --
#    nes/rom/ is gitignored, so this has to be done manually every time
#    fceux-am/nes/rom/<name>.nes
```

For any *other* `init.sh` subprojects not yet needed (`nemu`, `nvboard`,
`npc-chisel`, ...), just run `bash init.sh <name>` from `ysyx-workbench/` as usual --
those aren't tracked anywhere and always come straight from upstream.

### `npc/` — RTL Reimplementation (D4, in progress)

Skeleton for reimplementing the `minirv` processor from `f/` in real Verilog/RTL, using
Verilator. Not yet started.

## Notes for future me

- `ysyx-workbench`'s Makefile has a `STUNAME`/`STUID` field and a hidden git-tracer
  mechanism (`tracer-ysyx` branch) — don't touch the tracer machinery.
- The E4 minirvEMU's memory-mapped video decode (`[0x20000000, 0x20040000)`, X at bits
  `[9:2]`, Y at bits `[17:10]`, 24-bit RGB) matches the F6 Logisim hardware exactly —
  verified by running the same `vga.hex` on both and getting a pixel-identical image.
