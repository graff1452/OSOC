# OSOC

Personal coursework repository for the "一生一芯" (ysyx / One Student One Chip) program.

**Jump to:** [What is this?](#what-is-this-project-in-plain-terms) ·
[Build NEMU](#build-nemu-one-time) · [Testing guide — verify everything](#testing-guide--verify-everything) ·
[Structure](#structure) ·
[F6 (Logisim)](#f--f6-mini-risc-v-processor-logisim) ·
[E-phase sims](#e--e-phase-instruction-set-simulators) ·
[NPC (RTL)](#npc--rtl-reimplementation-d4-complete-d5-in-progress) ·
[NEMU / PA1 & PA2 detail log](#nemu--e6pa1--pa2-njus-ics-simple-debugger--rv32im-computer-system) ·
[Related repos](#related-repos-not-included-here) ·
[Setting up a new machine](#setting-up-a-new-machine) ·
[Notes for future me](#notes-for-future-me)

## What is this project, in plain terms?

This repo is a series of exercises in building a **computer, entirely in software,
from the ground up** — no physical chip involved (yet — that comes later in the
course). Each piece below builds on the last:

- **NEMU** — a program that pretends to be a RISC-V processor. Real CPUs are physical
  circuits; NEMU is the same behavior written in C instead, running on a normal
  laptop. You feed it a compiled program (a `.bin` file), and it reads that program's
  instructions one at a time and simulates doing whatever each one says — add these
  numbers, jump to this address, read this memory, etc.
- **The instruction decoder** (`nemu/src/isa/riscv32/inst.c`) — NEMU's "dictionary."
  Every RISC-V instruction is really just a specific pattern of 32 ones and zeros.
  This file is a big list of rules: "if the bits look like *this*, it means *add two
  numbers*"; "if they look like *that*, it means *jump*." Teaching NEMU every rule in
  the RISC-V instruction set (RV32I + RV32M) was PA2.1.
- **AM (Abstract Machine)** — real programs (even something as simple as
  `printf("hello")`) need more than raw instructions; they need a way to print
  characters, know what time it is, read a keyboard, draw to a screen, and so on. AM
  is a small, fixed set of functions (`halt()`, `putch()`, `io_read()`, ...) that any
  program can call to get those things — and it's AM's job, not the program's, to
  know the specific hardware details of *how* to actually talk to a keyboard/screen/
  clock on whatever machine it's running on. Writing the actual code behind those AM
  functions for NEMU specifically (reading the real keyboard, drawing real pixels,
  etc.) was PA2.2.
- **klib** — a small stripped-down copy of the standard C library (`strlen`,
  `memcpy`, `printf`, `malloc`, ...) written completely from scratch, since NEMU is a
  "bare metal" environment with no operating system underneath it to provide these
  for free the way a normal program on your laptop gets them.
- **IOE (I/O Extension)** — the part of AM specifically about talking to devices:
  the clock (timer), keyboard, screen (VGA), and speaker (audio). Each device is
  "wired in" by reading and writing specific memory addresses that NEMU treats
  specially (called MMIO — memory-mapped I/O) rather than as ordinary RAM.
- **`cpu-tests` / `am-tests`** — pre-written test programs (someone else wrote these,
  not me) that get compiled and run *inside* NEMU. If NEMU's instruction decoder or
  AM implementation has a bug, these tests fail in an obvious way (wrong output,
  crash, or NEMU reports `HIT BAD TRAP` instead of `HIT GOOD TRAP`) — they're how I
  know the work above is actually correct, not just "looks right."
- **DiffTest** — running the *same* program on NEMU and on a second,
  independently-written, trusted RISC-V simulator (Spike) side by side, comparing
  their results after every single instruction. Catches subtle bugs that the test
  programs above don't happen to exercise.
- **Trace tools** (`iringbuf`, `mtrace`, `ftrace`) — different ways of recording
  *what actually happened* while a program ran, for debugging: recent instruction
  history, every memory read/write, or a readable function-call tree. None of these
  change NEMU's behavior — they're just windows into what it's already doing.

The rest of this README (F6, E-phase, NPC) covers earlier/parallel stages of the same
course, described in their own sections below.

## Build NEMU (one-time)

Assumes Ubuntu, a fresh clone of this repo, SSH key already set up on GitHub — see
["Setting up a new machine"](#setting-up-a-new-machine) below if any of that isn't
true yet.

```bash
cd ~/Desktop/OSOC/ysyx-workbench

# one-time OS packages -- covers everything below (cpu-tests, IOE devices, malloc/demo)
sudo apt-get install bison flex libreadline-dev python-is-python3 \
  g++-riscv64-linux-gnu binutils-riscv64-linux-gnu \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
sudo sed -i 's|# include <gnu/stubs-ilp32.h>|// # include <gnu/stubs-ilp32.h>|' \
  /usr/riscv64-linux-gnu/include/gnu/stubs.h

# build NEMU
cd nemu
make menuconfig   # Base ISA -> riscv32 (default). Turn ON: Devices.
                  # Leave "Application on Abstract-Machine" UNSELECTED.
make

# pull in the test/demo sources this whole guide uses
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh am-kernels
```

**Two things that WILL bite you if skipped — read before running anything below:**
- `make ARCH=riscv32-nemu run` (and any use of `run`) **hangs** — NEMU's `run` target
  isn't batch-mode by default, so it sits waiting on stdin forever. **Every command in
  this guide runs the compiled `.bin` directly with `-b` instead** — never use `run`.
- Any test that reads `mainargs` (every command below except the plain `cpu-tests`
  suite) needs one **extra explicit step** after building —
  `make ARCH=riscv32-nemu mainargs=<x> insert-arg` — or the *old* `mainargs` value
  silently stays baked into the binary, which looks exactly like "my change did
  nothing." Every command below already includes this step where needed.

## Testing guide — verify everything

Run these in order. Each one tells you exactly what a correct result looks like.

### 1. RV32I + RV32M instruction decoder — 35/35 `cpu-tests`

```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/tests/cpu-tests
for t in $(basename -s .c tests/*.c); do make ARCH=riscv32-nemu ALL=$t; done

cd ~/Desktop/OSOC/ysyx-workbench/nemu
pass=0; fail=0
for f in ../am-kernels/tests/cpu-tests/build/*.bin; do
  if timeout 10 ./build/riscv32-nemu-interpreter -b "$f" 2>&1 | grep -q "HIT GOOD TRAP"; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); echo "FAIL: $(basename "$f" .bin)"
  fi
done
echo "$pass passed, $fail failed"
```
**Expect:** `35 passed, 0 failed`.

### 2. `klib` string/stdio — already covered by test 1 above

`string` and `hello-str` are two of the 35 tests above; no separate step needed.

### 3. Timer (`AM_TIMER_UPTIME`)

```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/tests/am-tests
rm -rf build
make ARCH=riscv32-nemu mainargs=t
make ARCH=riscv32-nemu mainargs=t insert-arg

cd ~/Desktop/OSOC/ysyx-workbench/nemu
timeout 5 ./build/riscv32-nemu-interpreter -b ../am-kernels/tests/am-tests/build/amtest-riscv32-nemu.bin
```
**Expect:** a new line printed roughly once per real second
(`1900-0-0 2d:2d:2d GMT (1 second).`, `(2 seconds).`, ...). The date is a placeholder
(`AM_TIMER_RTC` isn't implemented) — that's expected; what matters is a new line each
second.

### 4. Keyboard (`AM_INPUT_KEYBRD`)

```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/tests/am-tests
rm -rf build
make ARCH=riscv32-nemu mainargs=k
make ARCH=riscv32-nemu mainargs=k insert-arg

cd ~/Desktop/OSOC/ysyx-workbench/nemu
./build/riscv32-nemu-interpreter -b ../am-kernels/tests/am-tests/build/amtest-riscv32-nemu.bin > /tmp/trace.txt
```
A window pops up — **click into it** for keyboard focus, then press some keys, then
`Ctrl+C` to stop. **Expect:** lines like `Got (kbd): UP (73) DOWN` / `... UP` for each
press/release.

### 5. VGA (`AM_GPU_CONFIG` / `AM_GPU_FBDRAW`)

```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/tests/am-tests
rm -rf build
make ARCH=riscv32-nemu mainargs=v
make ARCH=riscv32-nemu mainargs=v insert-arg

cd ~/Desktop/OSOC/ysyx-workbench/nemu
./build/riscv32-nemu-interpreter -b ../am-kernels/tests/am-tests/build/amtest-riscv32-nemu.bin > /tmp/trace.txt
```
**Expect:** the popup window shows a live colored animation (a filled snake-like
pattern moving through a 32x32 grid), not a black screen. Terminal prints
`FPS = 3` (or similar) once per second. `Ctrl+C` to stop.

### 6. Audio (`AM_AUDIO_*`) — optional per the handout, implemented anyway

**Turn your volume down first** — a broken implementation can produce loud white noise.
```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/tests/am-tests
rm -rf build
make ARCH=riscv32-nemu mainargs=a
make ARCH=riscv32-nemu mainargs=a insert-arg

cd ~/Desktop/OSOC/ysyx-workbench/nemu
./build/riscv32-nemu-interpreter -b ../am-kernels/tests/am-tests/build/amtest-riscv32-nemu.bin > /tmp/trace.txt
```
**Expect:** an audible "Twinkle Twinkle Little Star" melody plays; terminal shows
`Already play <N>/56844 bytes of data` counting up to completion.

### 7. `malloc`/`free` — Tower of Hanoi demo

```bash
cd ~/Desktop/OSOC/ysyx-workbench/am-kernels/kernels/demo
rm -rf build
make ARCH=riscv32-nemu mainargs=3
make ARCH=riscv32-nemu mainargs=3 insert-arg

cd ~/Desktop/OSOC/ysyx-workbench/nemu
./build/riscv32-nemu-interpreter -b ../am-kernels/kernels/demo/build/demo-riscv32-nemu.bin > /tmp/trace.txt
```
**Expect:** a window shows the Tower of Hanoi animation (a pyramid of disks moving
between three pegs). Press `Q` in the window to exit cleanly.

### 8. Trace infrastructure (`iringbuf`, `mtrace`, `ftrace`) — sanity check

All three are enabled by default in menuconfig (nested under `Testing and
Debugging`, alongside `ITRACE`) and require no separate setup. Quick way to see
all three in action at once:
```bash
cd ~/Desktop/OSOC/ysyx-workbench/nemu
./build/riscv32-nemu-interpreter -b -e ../am-kernels/tests/cpu-tests/build/recursion-riscv32-nemu.elf \
  ../am-kernels/tests/cpu-tests/build/recursion-riscv32-nemu.bin > /tmp/trace.txt
grep -E "call \[|ret " /tmp/trace.txt | head -20
```
**Expect:** an indented call tree with real function names (`main`, `f0`, `f1`,
`f2`, `f3`) — that's `ftrace`, reading the `.elf`'s symbol table via `-e`.
`iringbuf` shows itself automatically any time an `invalid opcode` crash happens
(prints the last 16 instructions leading up to it, most recent marked `-->`).
`mtrace` writes an `R`/`W` line to stdout for every physical memory access —
grep `/tmp/trace.txt` for `^R |^W ` to see it directly.

### 9. DiffTest against Spike — one-time setup, then verify

This is a heavier one-time setup (builds a full second RISC-V simulator from
source, several minutes):
```bash
sudo apt-get install device-tree-compiler
cd ~/Desktop/OSOC/ysyx-workbench/nemu/tools/spike-diff
make GUEST_ISA=riscv32
```
**Expect:** `build/riscv32-spike-so` gets created (~1.8MB). If it instead comes
out named just `-spike-so` (no `riscv32` prefix), `GUEST_ISA` wasn't picked up —
rerun with it explicit as above.

Then enable it: `make menuconfig` → Testing and Debugging → turn on
`[*] Enable differential testing` → confirm `Reference design` is `Spike` → save.
Rebuild NEMU (`make ISA=riscv32`), then run any test with `-d` pointing at the
`.so`:
```bash
./build/riscv32-nemu-interpreter -b -d tools/spike-diff/build/riscv32-spike-so \
  -e ../am-kernels/tests/cpu-tests/build/add-riscv32-nemu.elf \
  ../am-kernels/tests/cpu-tests/build/add-riscv32-nemu.bin
```
**Expect:** `HIT GOOD TRAP`, same as without DiffTest, just much slower (every
instruction gets independently re-executed by Spike and register-compared —
expected, not a bug). A real mismatch would abort immediately with a `Register
mismatch` message naming the exact register and both values.

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

### `npc/` — RTL Reimplementation (D4 complete, D5 in progress)

Reimplementation of the `minirv` processor from `f/` in real Verilog/RTL, verified with
Verilator. D4 is done: a complete 8-instruction `minirv` core (`addi`, `add`, `lui`,
`jalr`, `lw`, `lbu`, `sw`, `sb`, plus `ebreak`), DPI-C-based memory access and
simulation control, and full AM toolchain integration (`minirv-npc` target) — verified
by running real compiled C programs, 35/35 passing on `cpu-tests`.

D5 (devices and I/O) is in progress: UART output and a real-time clock are implemented
purely by adding address checks to the existing DPI-C `pmem_read`/`pmem_write`
functions — **no RTL changes at all** — plus the matching AM-side platform code
(`putch()`, `__am_timer_uptime()`). Verified with the `hello` kernel (real text output)
and `am-tests`' real-time clock test (ticks once per real second), and further stress-
tested by booting character-mode FCEUX (`fceux-am`, `mario.nes`) to a full, recognizable
ASCII title screen running on the actual RTL core. VGA (graphical Mario) is the
remaining optional piece, not yet started.

See [`npc/README.md`](ysyx-workbench/npc/README.md) for the full breakdown of what's in
each subfolder (`xor-test/`, `nvboard-xor/`, `nvboard-light/`, `scpu-rtl/`, `dpi-test/`,
`minirv-rtl/`) and how to run them.

### `nemu/` — E6/PA1 & PA2: NJU's ICS Simple Debugger + RV32IM Computer System

[NEMU](https://github.com/NJU-ProjectN/nemu) — a from-scratch RISC-V (`riscv32`)
instruction set simulator from Nanjing University's "Computer Systems Fundamentals"
course, integrated into ysyx as its own stage. Conceptually the same idea as
`e4/minirv/minirvemu.c`, just the professional, more complete version with a real ISA
and a built-in interactive debugger.

**For "how do I build and test this," see [Build NEMU](#build-nemu-one-time) and the
[Testing guide](#testing-guide--verify-everything) at the top of this file.**
Everything below is a detailed log of what's implemented, why, and bugs found along
the way — useful for picking a task back up or debugging, not required reading to
get NEMU running.

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

> **Correction (found during PA2.3):** despite the list above, `slti`, `lb`, and `ori`
> were *not* actually present in `inst.c` at the end of PA2.1 — all three passed
> unnoticed because `cpu-tests`' 35/35 never happened to exercise them. Real-world
> programs (`demo/galton`, `demo/donut`, FCEUX) did, and crashed until fixed. See the
> PA2.3 section below for the full story of how each was found and fixed.

**Toolchain setup, in addition to the PA1 steps below:**
```bash
sudo apt-get install g++-riscv64-linux-gnu binutils-riscv64-linux-gnu python-is-python3
# Same missing-multilib-header issue as PA1's stubs-ilp32.h fix, applies here too
# if riscv32 compilation fails with "gnu/stubs-ilp32.h: No such file or directory"
```
`am-kernels/tests/cpu-tests/` requires `bash init.sh am-kernels` (see "Related repos"
below) before any of this can be built or run.

**PA2.2 (complete) — klib, IOE, malloc, infrastructure, DiffTest:**

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

**IOE (I/O Extension), in `abstract-machine/am/src/platform/nemu/ioe/`:**

- `timer.c` — `__am_timer_uptime()` was a stub always returning `0`; now reads NEMU's
  RTC MMIO register as two 32-bit halves (`RTC_ADDR`, `RTC_ADDR+4`) and combines them
  into one `uint64_t` microsecond count (casting the high half to `uint64_t` *before*
  shifting left 32 — shifting a still-32-bit value left by 32 is itself UB). Verified
  with `am-tests` (`mainargs=t`): prints a new line every real second.
- `input.c` — `__am_input_keybrd()` was a stub always reporting "no key"; now reads
  the keyboard MMIO register at `KBD_ADDR`. Bit 15 (`KEYDOWN_MASK`, `0x8000`) is the
  press/release flag; the rest of the bits (`code & ~KEYDOWN_MASK`) are the key code.
  Verified with `am-tests` (`mainargs=k`): real key press/release events come through.
- `gpu.c` — `__am_gpu_config()` was hardcoded to `width=0, height=0`; now reads the
  real screen size from `VGACTL_ADDR` (width in upper 16 bits, height in lower 16,
  per NEMU's `init_vga()`). `__am_gpu_fbdraw()` only handled the `sync` flag and never
  wrote any pixel data; now copies the caller's `w*h` pixel block into the framebuffer
  at `FB_ADDR`, computing each destination row with the *screen's* width (not the
  block's own width), since the framebuffer is one row-major array spanning the
  whole screen.
- `audio.c` (optional per handout) — was fully stubbed (`present=false`, all writes
  no-ops). Implemented `__am_audio_config/ctrl/status/play`; `play` is the interesting
  one — copies PCM data into the stream buffer in chunks, polling `AUDIO_COUNT_ADDR`
  and waiting whenever the buffer is full, tracking its own write position with
  wraparound (clamping each `memcpy` so it never crosses the buffer's physical end
  in one call, since the buffer is a flat region, not something `memcpy` understands
  as circular on its own).

**Also found and fixed two real bugs in NEMU itself, not just AM stubs:**
- `nemu/src/device/vga.c`'s `vga_update_screen()` was an empty `// TODO` — `gpu.c`
  was correctly signaling "frame ready" via the sync register, but nothing on the
  NEMU side ever checked it or pushed the framebuffer to the actual SDL window.
  Symptom was misleading: the AM-side FPS counter incremented normally (logic was
  fine), but the window stayed solid black — looked like an AM-side bug, wasn't.
- `nemu/src/device/audio.c`'s stream-buffer MMIO mapping had `NULL` as its write
  handler — meaning nothing ever tracked how many bytes AM had written into the
  buffer, so `AM_AUDIO_PLAY`'s wait-for-free-space logic had no real data to poll.
  Added a `sbuf_io_handler()` that increments a byte counter on every write.

**Verified**: `am-tests` `mainargs=v` renders live animation in the popup window;
`mainargs=a` plays the built-in "Twinkle Twinkle Little Star" PCM data through to
completion with an audible, recognizable melody.

**Gotcha discovered along the way — `mainargs` for `$ISA-nemu` builds:** unlike Spike
builds (which bake `mainargs` in via `-DMAINARGS`), `$ISA-nemu` builds patch it
*post-link* directly into the compiled `.bin`, via a Python script
(`abstract-machine/tools/insert-arg.py`) invoked by an `insert-arg` Makefile target.
That target only runs automatically as a prerequisite of `make run` — which we avoid
(see the batch-mode-hang note below/in "Notes for future me"). So when testing
anything that needs `mainargs` (keyboard/VGA/audio tests, not just `hello`), run it
explicitly:
```bash
make ARCH=riscv32-nemu mainargs=<char-or-string> insert-arg
```
Also needs `python-is-python3` (same fix as noted elsewhere in this README) — without
it, `insert-arg` fails with `python: not found` and **silently leaves the old
`mainargs` value baked into the binary**, which looks exactly like "my code change
did nothing" and is easy to misdiagnose as a logic bug instead of a missing build step.

**`malloc`/`free`, in `abstract-machine/klib/src/stdlib.c`:**

`malloc()` was a stub (`panic("Not implemented")`); `free()` was already correctly
a no-op. Implemented as a simple bump allocator per the handout's own suggestion: a
static pointer starts at `heap.start`, each call hands out the current position and
advances it by `size` (rounded up to a multiple of 8 for alignment) — no reuse of
freed memory, which is exactly why `free()` staying empty is correct, not lazy.
Verified with `am-kernels/kernels/demo` (`mainargs=3`, Tower of Hanoi): renders and
animates correctly.

**Trace infrastructure, all in `nemu/` (not `abstract-machine/` — these live on the
hardware/simulator side, not the AM/software side):**

- `iringbuf` (`src/cpu/cpu-exec.c`, `src/engine/interpreter/hostcall.c`) — a fixed-size
  circular buffer storing the last `CONFIG_IRINGBUF_SIZE` (default 16) instructions'
  already-formatted `itrace` strings. `display_iringbuf()` prints them oldest-first,
  most recent marked `-->`, automatically called from `invalid_inst()` right before
  it reports an `ABORT` — so every invalid-opcode crash now shows the recent
  instruction trail leading up to it, not just the single crashing instruction.
- `mtrace` (`src/memory/paddr.c`) — logs every physical memory access (`R`/`W`,
  address, length, data) via the same `log_write()` `itrace` already uses. Note it
  also captures instruction *fetches*, not just data loads/stores, since
  `inst_fetch()` itself goes through `paddr_read()`.
- `ftrace` (new file `src/monitor/ftrace.c`, wired into `src/monitor/monitor.c` via a
  new `-e/--elf` flag and into `src/cpu/cpu-exec.c`'s `exec_once()`) — parses an
  ELF's `.symtab`/`.strtab` to map addresses to real function names, then detects
  `jal`(call, if `rd != 0`)/`jalr`(return, if exactly `rd==0 && rs1==1` — the D2
  lecture's `ret` pseudo-instruction encoding) during execution to print an indented
  call tree. Verified against the `recursion` test (matching the PA2 handout's own
  worked example): correctly reproduces the "mismatched call/return" phenomenon
  caused by tail-call optimization that the handout specifically asks about.

**DiffTest against Spike (`nemu/src/isa/riscv32/difftest/dut.c`):**

`isa_difftest_checkregs()` was a stub (`return false`). Implemented: compares
NEMU's live `cpu.gpr[32]`/`cpu.pc` against Spike's reported post-instruction state,
printing a clear mismatch message (register name via `reg_name()`, both values) for
any disagreement. Required real one-time infrastructure setup, not just code:
installed `device-tree-compiler`, built Spike itself from source via
`nemu/tools/spike-diff` (had to pass `GUEST_ISA=riscv32` explicitly — it wasn't
being picked up automatically, and the resulting `.so` built with an empty ISA
prefix in its filename until fixed), enabled `DIFFTEST`/`DIFFTEST_REF_SPIKE` in
menuconfig. Verified against `add`: all 845 instructions independently
re-executed and register-compared against Spike, zero mismatches.

**PA2.2 is now fully complete** — klib, TRM, full IOE (timer/keyboard/VGA/audio),
`malloc`/`free`, all three trace tools, and DiffTest all implemented and verified.

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

**PA2.3 (complete) — demo programs, FCEUX, required questions:**

With malloc/IOE done, ran the full `am-kernels/kernels/demo/` set
(`ant`/`galton`/`hanoi`/`life`/`cmatrix`/`donut`/`bf`, dispatched via a single digit
passed as `mainargs` into `src/main.c`'s `switch`) and both character-mode and
graphical-mode FCEUX (`fceux-am/`, running `mario.nes`). This surfaced **three real
decoder gaps in NEMU itself** that `cpu-tests` never exercised — all found the same
way: a program hangs/crashes, `iringbuf`'s auto-dump on `invalid opcode` shows the
last 16 instructions, `objdump`/`addr2line` on the `.elf` identifies the exact
instruction, compare against what's already implemented in `inst.c`.

- **`slti` (set-less-than-immediate, signed) was never implemented at all** — only its
  unsigned sibling `sltiu` existed. Every `slti` in `cpu-tests` apparently used a small
  enough immediate that signed vs. unsigned comparison gave the same answer, hiding the
  gap. Surfaced by `galton`'s animation-delay counter (`slti a5,s5,1024` — `1024`'s top
  bits don't match `sltiu`'s any-small-positive-immediate coverage). Fixed by adding the
  `funct3=010` pattern, comparison cast to `(sword_t)` (mirrors `slt`'s R-type version,
  which *was* already correct).
- **`lb` (load byte, sign-extended) was never implemented** — `lbu`/`lh`/`lhu` all
  existed, but not plain signed-byte load. Surfaced by `donut`'s shading/lookup code
  reading a signed byte. Fixed by adding the `funct3=000` pattern with an 8-bit `SEXT`,
  mirroring `lh`'s 16-bit version.
- **`ori` (OR-immediate) was never implemented** — `andi`/`xori` (the other two
  bitwise-immediate ops) existed, `ori` didn't. Surfaced by FCEUX's mapper/board
  dispatch-table lookup code. Fixed by adding the `funct3=110` pattern (no sign cast
  needed — OR doesn't care about signedness).

All three follow the same pattern: not *wrong* code, just *absent* code that 35/35
`cpu-tests` passing never required — a good reminder that a passing test suite is a
claim about what it tested, not a claim of completeness. Also hit and fixed a false
alarm along the way: `make ARCH=riscv32-nemu run` in `demo/` crashed with
`Assertion 'ref_so_file != NULL' failed` — not a new bug, just DiffTest still being
enabled in `.config` from PA2.2 testing, expecting a `-d <spike.so>` flag that wasn't
being passed for a plain demo run. Fixed by disabling DiffTest in `make menuconfig`.

**FCEUX-specific notes:**
- Character mode: comment out `HAS_GUI` in `fceux-am/src/config.h`, rebuild, run — no
  VGA dependency, output goes through `putch()`. FCEUX's own startup log
  (`ROM MD5:  0x2x2x2x2x2x2x2x2x2x2x...`) reveals klib's `vsprintf` doesn't support
  `%x`/width modifiers (scoped to `%s`/`%d` only per the PA2.2 handout) — expected
  limitation surfacing in third-party code, not a new bug to fix.
- Graphical mode: re-enable `HAS_GUI`, rebuild — reuses the VGA code from PA2.2
  unchanged. Confirmed working: full-color "SUPER MARIO BROS." title screen renders
  correctly. No keyboard support wired into FCEUX at this stage (per handout), so the
  attract-mode demo is display-only, not playable yet.
- `fceux-am`'s build output lands directly in `build/`, not `nes/build/` as its own
  README's `native` instructions might suggest at a glance — worth double-checking the
  actual path after a build (`+ LD -> build/fceux-riscv32-nemu.elf` in the build log)
  rather than assuming.
- Some of `demo/`'s programs (`donut` specifically) have their own internal `while(1)`
  render loop that never returns to `main()` — so `main()`'s "press Q to exit" handler
  is unreachable for those. Not a bug, just how that file is written; exit via `Ctrl+C`
  instead, same as the raw `am-tests` binaries.

**Required questions answered in the lab report** (`学号.pdf`): state-machine diagram
for the YEMU addition program + RTFSC connection to `exec_once()`; RTFSC of one `add`
instruction's full fetch→decode→execute→PC-update path through `inst.c`; the typing
game's five-layer (physical→NEMU→ISA→AM→program) IOE round-trip; three compile/link
experiments on `inst_fetch()`'s `static`/`inline` qualifiers and a `dummy`-variable
duplication puzzle across `common.h`/`debug.h` (tentative-definition merging is the
key insight — uninitialized `static` redeclarations merge silently, initialized ones
don't); and tracing `hello/`'s `Makefile`/build pipeline via `make -n`.

### Related repos (not included here)

Not everything lives inside this repo. A few pieces grew big enough that I split them
into their own separate GitHub repos, and just cloned those repos into folders here
(alongside `abstract-machine/` and `npc/`, which — unlike these four — live directly
inside this repo's own git history, no separate cloning needed):

- **[am-kernels](https://github.com/graff1452/am-kernels)** — AM test programs + my own
  screensaver and GUI-enabled `minirvEMU`. Also contains `tests/cpu-tests/`, the RV32IM
  instruction-decoder test suite used throughout PA2.1.
- **[fceux-am](https://github.com/graff1452/fceux-am)** — NES emulator ported to AM
- **[nvboard](https://github.com/graff1452/nvboard)** — virtual FPGA board used to test
  RTL interactively (switches, LEDs, VGA, UART, ...)
- **[yosys-sta](https://github.com/graff1452/yosys-sta)** — ASIC synthesis (Yosys) +
  timing/power analysis (iEDA) pipeline, used to turn `npc/` RTL into a real
  standard-cell netlist and get a first PPA (performance/power/area) estimate

Why this matters for setup: cloning *this* repo alone is not enough to get a fully
working checkout — these four also need cloning separately. The next section covers
exactly when and how.

### Setting up a new machine

There are two very different reasons someone might be reading this section — pick
whichever one is actually you, since the right steps are genuinely different.

- **"I'm new to this course and want to try it myself"** → go to
  [Starting the course fresh](#starting-the-course-fresh), below.
- **"This is my repo, and I just want my existing work running on another computer"**
  → go to [Picking up my own work on a new device](#picking-up-my-own-work-on-a-new-device),
  below.

#### Starting the course fresh

If you found this repo while looking into the "一生一芯" (ysyx) course yourself: **don't
clone the four repos listed just above.** Those contain my own finished/in-progress
answers to the course's exercises — copying them would mean copying solutions instead
of doing the work, which is the opposite of what the course (and this whole repo) is
for. Use this repo as a *reference* for how one person organized things, not as a
starting point to clone from.

Instead, get the same **blank, starter** version of the framework that I started from:

```bash
git clone https://github.com/OSCPU/ysyx-workbench.git
cd ysyx-workbench

# each of these downloads a fresh, unmodified copy of that piece —
# not my solved version of it
bash init.sh nemu
bash init.sh am-kernels
bash init.sh abstract-machine
bash init.sh npc
bash init.sh nvboard
```

From there, follow the course's own handouts for each phase (F, E, D, ...) and write
the code yourself. The toolchain-installation commands lower down in this section
(Verilator, the RISC-V cross-compiler, SDL2, etc.) still apply to you exactly as
written — those are just "software this course needs," not part of my personal setup.

#### Picking up my own work on a new device

This is the case where you (meaning: me, on a different computer) already did all the
work, and it's already saved on GitHub. Nothing here needs to be redone or figured out
again — every step below is just "download what already exists" or "install the same
background tool I already installed once before." Follow these in order:

**1. Let this new machine talk to GitHub.** Generate an SSH key here and add its public
half to your GitHub account (Settings → SSH and GPG keys → New SSH key):
```bash
ssh-keygen -t ed25519 -C "your_email@example.com"
cat ~/.ssh/id_ed25519.pub
```

**2. Download the main repo** (this one):
```bash
cd ~/Desktop
git clone git@github.com:graff1452/OSOC.git
```

**3. Download the three other repos that live alongside it.** As explained above,
`am-kernels`, `fceux-am`, and `nvboard` are separate GitHub repos, not part of
`OSOC` itself — cloning `OSOC` alone won't bring them along:
```bash
cd OSOC/ysyx-workbench
git clone git@github.com:graff1452/am-kernels.git
git clone git@github.com:graff1452/fceux-am.git
git clone git@github.com:graff1452/nvboard.git
```

**4. Install NVBoard's dependencies, and tell the shell where to find it:**
```bash
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
echo "export NVBOARD_HOME=$(readlink -f nvboard)" >> ~/.bashrc
```

**5. Point at `abstract-machine/` — no separate download needed here.** Unlike the
three repos above, `abstract-machine/` is tracked directly inside `OSOC` itself, so
step 2 already brought it along:
```bash
echo "export AM_HOME=$(readlink -f abstract-machine)" >> ~/.bashrc
source ~/.bashrc
```

**6. Install Verilator v5.008 from source.** The version Ubuntu's package manager
offers is too old for what `npc/` needs, so this has to be built by hand, outside the
repo (see [the official install guide](https://verilator.org/guide/latest/install.html)
for more detail than fits here):
```bash
sudo apt-get install git make autoconf g++ flex bison help2man
#    (help2man is needed by `sudo make install` -- without it, install fails
#    partway through with "help2man: No such file or directory")
cd ~/Desktop
git clone https://github.com/verilator/verilator
cd verilator && git checkout v5.008
autoconf && ./configure && make -j$(nproc) && sudo make install
verilator --version   # should report "Verilator 5.008"
```

**7. Point at `npc/`:**
```bash
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh npc
source ~/.bashrc
```

**8. Set up `yosys-sta`** (synthesis + timing/power analysis) — only needed if you
plan to redo the PPA/synthesis work, not for RTL simulation itself:
```bash
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
```

**9. Build NEMU from source.** Only the *source code* is saved in git — the compiled
program itself isn't, so it has to be rebuilt on every new machine:
```bash
cd ~/Desktop/OSOC/ysyx-workbench
bash init.sh nemu
source ~/.bashrc
sudo apt-get install bison flex libreadline-dev
sudo apt-get install g++-riscv64-linux-gnu binutils-riscv64-linux-gnu python-is-python3
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
# Ubuntu's riscv64-linux-gnu cross-toolchain is missing 32-bit multilib headers
# (must run AFTER the toolchain is installed above, or stubs.h doesn't exist yet):
sudo sed -i 's|# include <gnu/stubs-ilp32.h>|// # include <gnu/stubs-ilp32.h>|' \
  /usr/riscv64-linux-gnu/include/gnu/stubs.h
cd nemu
make menuconfig   # Base ISA -> riscv32 (default); turn ON Devices; leave
                  # "Application on Abstract-Machine" UNSELECTED, or the build breaks
make
./build/riscv32-nemu-interpreter
```

**10. Pull in the test/demo sources** (skip if step 3 already cloned `am-kernels`),
then see ["Testing guide — verify everything"](#testing-guide--verify-everything)
near the top of this file for the full list of checks (cpu-tests, timer, keyboard,
VGA, audio, malloc/hanoi demo) and what a correct result looks like for each:
```bash
bash init.sh am-kernels
```

**11. (Optional) Add a ROM to run `fceux-am`.** `nes/rom/` is gitignored on purpose
(ROM files shouldn't be committed), so this has to be done by hand on every machine —
place a legally-obtained ROM at `fceux-am/nes/rom/<name>.nes`.

---

For any *other* `init.sh` subproject not covered above (`npc-chisel`, ...), just run
`bash init.sh <name>` from `ysyx-workbench/` — those always come straight from the
course's own upstream source, nothing personal to track down.

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
- New Kconfig `*_COND` string options (like `ITRACE_COND`, `MTRACE_COND`) don't
  become usable C expressions just by adding them to `Kconfig` — they need a
  matching `-D<NAME>_COND=...` line added to `nemu/Makefile`'s `CFLAGS_TRACE`
  block, which strips the quotes and pastes the raw Kconfig string as a compiler
  flag. Easy to add the Kconfig entry, rebuild, and get a confusing "undeclared
  identifier" error while forgetting this second step exists.
- The `minirv-npc` `ARCH` target never defined the `ISA_H` macro (compare
  `scripts/minirv-nemu.mk`, which has `CFLAGS += -DISA_H=\"riscv/riscv.h\"` —
  `scripts/minirv-npc.mk` was missing the equivalent line entirely). This went
  unnoticed through all of D4 because nothing in `trm.c`/`timer.c` needed
  `outb`/`inl` (from `riscv/riscv.h`) until D5's UART/clock work actually called
  them — surfaced as `implicit declaration of function 'outb'`. Fixed by adding
  the same `-DISA_H=\"riscv/riscv.h\"` line to `minirv-npc.mk`, **and** adding a
  new `npc.h` (mirroring `nemu.h`) with `#include ISA_H` in it — the macro being
  defined isn't enough on its own if nothing actually `#include`s it.
- Compiled `.o` files don't get deleted when a compile step fails partway through
  a multi-file build — they're just left stale. Cost real debugging time during D5:
  fixing `trm.c`'s compile error didn't actually get exercised on the next `make
  run`, because `trm.o` from *before* the fix was still sitting there looking
  "up to date" to Make. A full `rm -rf am/build klib/build <program>/build` is the
  reliable fix when a previous build attempt errored out partway through.
- Verilator's own build (`verilator -cc --exe --build ...` inside `minirv-rtl/`)
  and the AM program build (`make ARCH=minirv-npc run` from a kernel/test
  directory) are two entirely separate build systems that happen to share a
  directory — `make ARCH=minirv-npc run` never rebuilds `Vminirv` itself, only
  the `.bin` it hands to whatever `Vminirv` already exists on disk. Editing
  `minirv-rtl/csrc/main.cpp` always needs its own explicit Verilator rebuild.
- `nemu/tools/spike-diff`'s `make` needs `GUEST_ISA=riscv32` passed explicitly —
  it isn't picked up from NEMU's own `.config` automatically the way other
  Makefiles in this project do. Symptom if forgotten: it still builds
  successfully, just names the output file `-spike-so` (empty ISA prefix)
  instead of `riscv32-spike-so`, which then silently fails to match what
  `-d tools/spike-diff/build/riscv32-spike-so` expects.
