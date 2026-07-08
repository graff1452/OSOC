# OSOC

Personal coursework repository for the "一生一芯" (ysyx / One Student One Chip) program.

## Structure

```
OSOC/
├── f/                  F-phase: minirv processor built in Logisim Evolution
├── e/                  E-phase: C toolchain, ISA simulators
└── ysyx-workbench/     The official ysyx course framework (see below)
```

## `f/` — F6: Mini RISC-V Processor (Logisim)

A fully functional 32-bit `minirv` processor built from scratch in Logisim Evolution,
implementing 8 RISC-V instructions: `add`, `addi`, `lui`, `lw`, `sw`, `lbu`, `sb`, `jalr`.
See [`f/README.md`](f/README.md) for the full writeup (architecture, testing process,
bugs found and fixed).

- `F6.circ` — the processor itself
- `F6_vga.circ` — with the memory-mapped RGB Video display wired in

To open: load the `.circ` file in [Logisim Evolution](https://github.com/logisim-evolution/logisim-evolution).

## `e/` — E-phase: Instruction Set Simulators

| Folder | Contents |
|---|---|
| `e4/sisa/semu.c` | `sEMU` — a from-scratch instruction set simulator for the toy 8-bit sISA (ADD/LI/BNER0/OUT) |
| `e4/minirv/minirvemu.c` | `minirvEMU` — a from-scratch instruction set simulator for the real `minirv` ISA: all 8 instructions, `ebreak`-based automatic pass/fail detection, loads real compiled test programs (`sum.bin`, `mem.bin` included) |

Build directly with:
```bash
gcc -Wall -Wextra minirvemu.c -o minirvemu
```

*(GUI-enabled versions of `minirvEMU` and a screensaver built on AM live in the separate
[am-kernels](https://github.com/graff1452/am-kernels) repo — see below.)*

## `ysyx-workbench/` — Official Course Framework

Cloned from [OSCPU/ysyx-workbench](https://github.com/OSCPU/ysyx-workbench). This is the
scaffold the course builds up incrementally via `init.sh`.

### `npc/` — RTL Reimplementation (D4/E5, in progress)

Skeleton for reimplementing the `minirv` processor from `f/` in real Verilog/RTL,
verified with Verilator.

- `csrc/main.cpp`, `vsrc/example.v` — untouched framework skeleton
- `xor-test/` — first hands-on Verilator exercise: a two-way switch (`f = a ^ b`),
  driven by a C++ testbench with randomized inputs and `assert()`-checked output
  against a plain-C reference computation. Also generates an FST waveform
  (`--trace-fst`, `VerilatedFstC`) viewable with GTKWave (`sudo apt-get install
  gtkwave`), so signal transitions can be inspected visually rather than just
  read off `printf` output.

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

# 5. Install Verilator v5.008 from source, OUTSIDE this repo
#    (see https://verilator.org/guide/latest/install.html)
sudo apt-get install git make autoconf g++ flex bison help2man
#    (help2man is needed by `sudo make install` -- without it, install fails
#    partway through with "help2man: No such file or directory")
cd ~/Desktop
git clone https://github.com/verilator/verilator
cd verilator && git checkout v5.008
autoconf && ./configure && make -j$(nproc) && sudo make install
verilator --version   # should report "Verilator 5.008"

# 6. Point NPC_HOME at npc/
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh npc
source ~/.bashrc

# 7. (Optional) place a legally-obtained ROM to run fceux-am --
#    nes/rom/ is gitignored, so this has to be done manually every time
#    fceux-am/nes/rom/<name>.nes
```

For any *other* `init.sh` subprojects not yet needed (`nemu`, `nvboard`,
`npc-chisel`, ...), just run `bash init.sh <name>` from `ysyx-workbench/` as usual --
those aren't tracked anywhere and always come straight from upstream.

## Notes for future me

- `ysyx-workbench`'s Makefile has a `STUNAME`/`STUID` field and a hidden git-tracer
  mechanism (`tracer-ysyx` branch) — don't touch the tracer machinery.
- The E4 minirvEMU's memory-mapped video decode (`[0x20000000, 0x20040000)`, X at bits
  `[9:2]`, Y at bits `[17:10]`, 24-bit RGB) matches the F6 Logisim hardware exactly —
  verified by running the same `vga.hex` on both and getting a pixel-identical image.
- `npc/**/obj_dir/` (Verilator build output) and `npc/**/*.fst` (waveform traces) are
  gitignored — regenerate with `make`, don't expect them to be present after a fresh
  clone.
