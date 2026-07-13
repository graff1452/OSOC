# OSOC

Personal coursework repository for the "一生一芯" (ysyx / One Student One Chip) program.

## Structure

```
OSOC/
├── f/                  F-phase: minirv processor built in Logisim Evolution
├── e/                  E-phase: C toolchain, ISA simulators
└── ysyx-workbench/     The official ysyx course framework, including nemu/ (see below)
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
verified with Verilator and (for interactive testing) NVBoard. See
[`npc/README.md`](ysyx-workbench/npc/README.md) for the full breakdown of what's in
each subfolder (`xor-test/`, `nvboard-xor/`, `nvboard-light/`) and how to run them.

### `nemu/` — E6/PA1: NJU's ICS Simple Debugger (complete)

[NEMU](https://github.com/NJU-ProjectN/nemu) — a from-scratch RISC-V (`riscv32`)
instruction set simulator from Nanjing University's "Computer Systems Fundamentals"
course, integrated into ysyx as its own stage. Conceptually the same idea as
`e4/minirv/minirvemu.c`, just the professional, more complete version with a real ISA
and a built-in interactive debugger.

**All of PA1 implemented and tested:**
- `si [N]` — single-step N instructions (default 1)
- `info r` — print all 32 GPRs + PC (`isa_reg_display()`, was an empty stub)
- `x N EXPR` — scan N words of memory starting at address EXPR
- `p EXPR` — full recursive-descent expression evaluator: `+ - * /` with correct
  precedence and parentheses, `== != &&`, register access (`$pc`, `$a0`, ...). Tested
  against 9,646 randomly-generated expressions (`tools/gen-expr/`), cross-checked
  against real `gcc`-compiled-and-run reference answers — 9,619 passed; the remaining
  27 are a known limitation of the reference-oracle method itself (GCC constant-folds
  fully-literal expressions at compile time and silently resolves division-by-zero
  differently than genuine runtime execution would, rather than crashing)
- `w EXPR` / `d N` / `info w` — full watchpoint system: expression re-evaluated and
  compared every instruction cycle, execution pauses automatically on change

**PA2.1 — Full RV32I + RV32M instruction decoder (complete):**

Built `nemu/src/isa/riscv32/inst.c` up from a 4-instruction skeleton (`auipc`, `lbu`,
`sb`, `ebreak`) to the complete RV32I base ISA plus the RV32M extension, driven entirely
by `cpu-tests` failures (implement only what the next `invalid opcode` error demands).

- All 6 RISC-V instruction formats decoded from raw bit patterns: I, U, S, J, R, B
  (`decode_operand()` + `imm{I,U,S,J,B}()` macros in `inst.c`)
- RV32I: full arithmetic/logic (`add/addi`, `sub`, `and/andi`, `or/ori`, `xor/xori`,
  `slt/slti`, `sltu/sltiu`, `sll/slli`, `srl/srli`, `sra/srai`), all loads/stores at
  every width incl. sign-extension (`lb/lh/lw/lbu/lhu`, `sb/sh/sw`), both jumps
  (`jal`, `jalr`), all six branches (`beq/bne/blt/bge/bltu/bgeu`), `lui`
- RV32M: `mul`, `mulh`, `mulhu` (missing `mulhsu` — unused by any test so far),
  `div`, `divu`, `rem`, `remu` — including the RISC-V-mandated divide-by-zero and
  `INT_MIN`/`-1` overflow special cases (result conventions differ between
  `div`/`divu` and `rem`/`remu` — see comments in `inst.c`)

**Toolchain setup, in addition to the PA1 steps below:**
```bash
sudo apt-get install g++-riscv64-linux-gnu binutils-riscv64-linux-gnu python-is-python3
# Same missing-multilib-header issue as PA1's stubs-ilp32.h fix, applies here too
# if riscv32 compilation fails with "gnu/stubs-ilp32.h: No such file or directory"
```
`am-kernels/tests/cpu-tests/` requires `bash init.sh am-kernels` (see "Related repos"
below) before any of this can be built or run.

**PA2.2 (in progress) — klib, infrastructure, DiffTest:**

`abstract-machine/klib/src/string.c` and `stdio.c` implemented (both were previously
all-stub, every function calling `panic("Not implemented")`):

- `string.c`: `strlen`, `strcpy`, `strncpy`, `strcat`, `strcmp`, `strncmp`, `memset`,
  `memmove` (handles overlapping regions correctly by choosing copy direction based
  on relative pointer position — the one function here that genuinely needs it),
  `memcpy`, `memcmp`
- `stdio.c`: `vsprintf` (`%s`/`%d` only, per handout scope — `%d` implemented by hand
  via repeated `% 10`/`/ 10` digit extraction, no library shortcut), with `sprintf`,
  `printf`, `vsnprintf`, `snprintf` all built as thin wrappers around it

**35/35 `cpu-tests` now passing** — `string`/`hello-str` were the last two, both
blocked purely on the two files above, no decoder-side gap.

**Still to do for PA2.2:** trace infrastructure (`iringbuf`, `mtrace`, `ftrace` —
`itrace` already exists in the framework code), DiffTest wiring against Spike for
instruction-level correctness verification beyond what `cpu-tests` happens to exercise.

**Real bugs found and fixed along the way** (worth remembering):
- `init_regex()` was never called in the standalone test harness → segfault inside
  `regexec()` on the *first* token of the *simplest* possible input
- Two independent token-count limits (the `tokens[]` array size *and* a hardcoded
  `>= 32` check inside `make_token()`) — growing one without the other silently did
  nothing
- `eval()` used unsigned (`word_t`) arithmetic throughout, but the reference C programs
  evaluate as signed `int` until the final assignment — caused wrong answers on any
  expression involving negative intermediate division results

**Setup:** needs `bash init.sh nemu` (sets `$NEMU_HOME`), `bison flex libreadline-dev`,
and (Ubuntu-specific) a one-line comment-out of `gnu/stubs-ilp32.h`'s include in
`/usr/riscv64-linux-gnu/include/gnu/stubs.h` to work around a missing 32-bit multilib
header. Configure via `make menuconfig` (Base ISA → riscv32, leave "Application on
Abstract-Machine" **unselected** — that option is for a different build mode and will
break the normal build), then `make`.

### Related repos (not included here)

`init.sh` clones some subprojects as **independent git repositories**, gitignored by
`ysyx-workbench` itself. These live in my own separate repos, cloned inside
`ysyx-workbench/`, alongside `abstract-machine/` and `npc/`:

- **[am-kernels](https://github.com/graff1452/am-kernels)** — AM test programs + my own
  screensaver and GUI-enabled `minirvEMU`. Also contains `tests/cpu-tests/`, the RV32IM
  instruction-decoder test suite used throughout PA2.1.
- **[fceux-am](https://github.com/graff1452/fceux-am)** — NES emulator ported to AM
- **[nvboard](https://github.com/graff1452/nvboard)** — virtual FPGA board used to test
  RTL interactively (switches, LEDs, VGA, UART, ...)
- **[yosys-sta](https://github.com/graff1452/yosys-sta)** — ASIC synthesis (Yosys) +
  timing/power analysis (iEDA) pipeline, used to turn `npc/` RTL into a real
  standard-cell netlist and get a first PPA (performance/power/area) estimate

### Full setup on a brand-new device

```bash
# 1. Generate an SSH key on this machine and add its public key to GitHub
#    (Settings -> SSH and GPG keys -> New SSH key)
ssh-keygen -t ed25519 -C "your_email@example.com"
cat ~/.ssh/id_ed25519.pub

# 2. Clone this repo
cd ~/Desktop
git clone git@github.com:graff1452/OSOC.git

# 3. Clone the subprojects into ysyx-workbench/
cd OSOC/ysyx-workbench
git clone git@github.com:graff1452/am-kernels.git
git clone git@github.com:graff1452/fceux-am.git
git clone git@github.com:graff1452/nvboard.git
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
echo "export NVBOARD_HOME=$(readlink -f nvboard)" >> ~/.bashrc

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

# 7. Clone yosys-sta (synthesis + timing/power analysis) and install its toolchain
cd ~/Desktop
git clone git@github.com:graff1452/yosys-sta.git
# download oss-cad-suite for your architecture (`uname -m`) from
# https://github.com/YosysHQ/oss-cad-suite-build/releases, then:
tar -xzf oss-cad-suite-linux-*.tgz -C ~/Desktop
echo 'export PATH=$PATH:'"$HOME"'/Desktop/oss-cad-suite/bin' >> ~/.bashrc
source ~/.bashrc
yosys --version   # should report >= 0.48
cd yosys-sta
sudo apt-get install libunwind-dev liblzma-dev
make init
echo exit | ./bin/iEDA -v

# 8. Set up NEMU (E6/PA1)
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh nemu
source ~/.bashrc
sudo apt-get install bison flex libreadline-dev
sudo apt-get install g++-riscv64-linux-gnu binutils-riscv64-linux-gnu python-is-python3
# Ubuntu's riscv64-linux-gnu cross-toolchain is missing 32-bit multilib headers
# (must run AFTER the toolchain is installed above, or stubs.h doesn't exist yet):
sudo sed -i 's|# include <gnu/stubs-ilp32.h>|// # include <gnu/stubs-ilp32.h>|' \
  /usr/riscv64-linux-gnu/include/gnu/stubs.h
cd nemu
make menuconfig   # Base ISA -> riscv32 (default); leave "Application on
                  # Abstract-Machine" UNSELECTED, or the build breaks
make
./build/riscv32-nemu-interpreter

# 9. Set up PA2.1 (RV32IM cpu-tests)
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh am-kernels   # if not already cloned in step 3
cd am-kernels/tests/cpu-tests
# Build every test binary (compile only, doesn't run/hang):
for t in $(basename -s .c tests/*.c); do make ARCH=riscv32-nemu ALL=$t; done
# Run them all against NEMU in batch mode (-b) and report pass/fail.
# NOTE: plain `make ARCH=riscv32-nemu run` will HANG here -- NEMU's run target
# doesn't pass -b by default, so it drops into an interactive prompt waiting on
# stdin. Implementing default batch mode is a PA2 "required question" I haven't
# done yet (see "Notes for future me" below) -- until then, use this loop instead:
cd ~/Desktop/OSOC/ysyx-workbench/nemu
pass=0; fail=0
for f in ../am-kernels/tests/cpu-tests/build/*.bin; do
  name=$(basename "$f" .bin)
  if timeout 10 ./build/riscv32-nemu-interpreter -b "$f" 2>&1 | grep -q "HIT GOOD TRAP"; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); echo "FAIL: $name"
  fi
done
echo "$pass passed, $fail failed"
# Expected: 35 passed, 0 failed (klib's string.c/stdio.c are implemented and
# committed too, so string/hello-str pass along with everything else)

# 10. (Optional) place a legally-obtained ROM to run fceux-am --
#     nes/rom/ is gitignored, so this has to be done manually every time
#     fceux-am/nes/rom/<name>.nes
```

For any *other* `init.sh` subprojects not yet needed (`npc-chisel`, ...), just
run `bash init.sh <name>` from `ysyx-workbench/` as usual -- those aren't tracked
anywhere and always come straight from upstream.

## Notes for future me

- `ysyx-workbench`'s Makefile has a `STUNAME`/`STUID` field and a hidden git-tracer
  mechanism (`tracer-ysyx` branch) — don't touch the tracer machinery.
- The E4 minirvEMU's memory-mapped video decode (`[0x20000000, 0x20040000)`, X at bits
  `[9:2]`, Y at bits `[17:10]`, 24-bit RGB) matches the F6 Logisim hardware exactly —
  verified by running the same `vga.hex` on both and getting a pixel-identical image.
- `npc/**/obj_dir/` (Verilator build output) and `npc/**/*.fst` (waveform traces) are
  gitignored — regenerate with `make`, don't expect them to be present after a fresh
  clone.
- NEMU's `make run` doesn't pass `-b` (batch mode) by default, so it drops into an
  interactive `(nemu)` prompt and will hang if run non-interactively. Run
  `./build/riscv32-nemu-interpreter -b <image>.bin` directly instead, or fix the
  Makefile to default to batch mode (a PA2 "required question" I haven't done yet).
