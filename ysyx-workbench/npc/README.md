# npc — RTL Reimplementation of minirv

This is where the `minirv` processor (originally built in Logisim, see `../../f/`) gets
reimplemented as real Verilog RTL, verified with Verilator. D4 is now complete: after
building up the Verilator/NVBoard toolchain on small throwaway circuits and a first real
processor build (**sCPU**, the sISA processor), a full 8-instruction `minirv` core now
exists in RTL — DPI-C-based memory access, self-terminating via `ebreak`, and verified
both by hand-crafted per-instruction tests and by running real AM-compiled C programs
(`cpu-tests`, 35/35 passing).

## Directory guide

`vsrc/top.v` and `csrc/main.cpp` at this level are currently a leftover copy of the
`xor-test/` circuit (from early testing of the framework's `sim` Makefile target,
before `xor-test/` existed as its own separate folder) — **not yet the real `minirv`
NPC build**. Worth replacing with the actual processor once that work starts, rather
than treating this as reserved/empty placeholder space. Everything else:

| Folder | What it is |
|---|---|
| `xor-test/` | First Verilator exercise: a two-way switch (`f = a^b`), driven by a C++ testbench with randomized inputs, `assert()`-checked against a plain-C reference, plus FST waveform tracing (view with `gtkwave xor-test/wave.fst`) |
| `nvboard-xor/` | The same two-way switch, wired to a virtual FPGA board (NVBoard) instead of a C++ testbench — `SW0`/`SW1` drive `a`/`b`, `LD0` shows `f`, interactively |
| `nvboard-light/` | A sequential (clocked) circuit — 16 LEDs shifting in sequence — on NVBoard. First circuit here with real `clk`/`rst` handling |
| `scpu-rtl/` | **A complete, working sISA processor in real synthesizable RTL** — computes 1+2+...+10, verified with Verilator, synthesized with `yosys-sta` |
| `dpi-test/` | Minimal standalone DPI-C experiment (`notify_hit()` called from Verilog on every `posedge clk`) — proves the RTL→C++ call mechanism works before trusting it for `ebreak`/memory access |
| `minirv-rtl/` | **The real D4 deliverable: a complete 8-instruction `minirv` RISC-V core**, DPI-C memory access, `ebreak`-based termination, HIT GOOD/BAD TRAP, runs real AM-compiled C programs |

`xor-test/` and `nvboard-xor/` implement the *same* circuit two different ways
deliberately — one batch-tested via C++ assertions, one interactively driven via a
virtual board — to learn both verification approaches before they're needed together on
real processor RTL.

## `scpu-rtl/` — sCPU (sISA processor)

Built following the course's structural-modeling discipline: **no `always` blocks**
anywhere in the design (aside from inside the provided `Reg`/`MuxKey` templates
themselves) — every module is pure instantiation + wiring, to avoid the data-race and
synthesis-mismatch pitfalls of behavioral modeling covered in E5.

```
scpu-rtl/vsrc/
├── templates.v   Reg (parameterized flip-flop) and MuxKey (key-value mux) —
│                  course-provided templates, used throughout instead of always blocks
├── regfile.v      4×8-bit register file, 2 general read ports + a dedicated R0
│                  output (needed unconditionally every cycle for the branch check)
├── imem.v         16-entry × 8-bit instruction ROM (MuxKey-based lookup table),
│                  hardcoded with the 1+2+...+10 summation program
├── decoder.v       Pure combinational bit-slicing: opcode/rd/rs1/rs2/imm/baddr
├── alu.v            One line: assign result = a + b (sISA only has ADD)
├── pc_reg.v         The PC itself: one Reg instance, always-enabled
└── scpu.v            Top-level: wires all of the above together, plus the two
                       genuine decisions that need multiple modules' signals at once —
                       branch-taken/next-PC selection, and register write-back data/enable
```

Each module was built and unit-tested in isolation (own `csrc/*_test.cpp` testbench,
own `assert()`-based checks) before being wired into `scpu.v` — same incremental
discipline as everything else in this repo.

**Run the full test:**
```bash
cd scpu-rtl
verilator -cc --exe --build -Mdir build --top-module scpu \
  vsrc/templates.v vsrc/regfile.v vsrc/imem.v vsrc/decoder.v vsrc/alu.v vsrc/pc_reg.v vsrc/scpu.v \
  csrc/scpu_test.cpp
./build/Vscpu
```
Expected: `OUT fired ... value = 55`.

**Synthesis result** (`icsprout55`, 500MHz, via `yosys-sta`): 0 warnings, **36 flip-flops**
(exactly matching 4×8-bit registers + 4-bit PC = 36 bits of declared state — a clean
sanity check that synthesis preserved the design faithfully), 658.84 total area units,
33.66% of which is sequential. Noticeably more, denser combinational gate variety than
the running-light circuit — a direct cost of `MuxKey`'s comparator-heavy internal
structure versus simpler hand-written logic.

**sEMU vs sCPU**: same sISA, two fundamentally different implementations. sEMU is one
sequential C function executed by a real (x86) CPU, fetch/decode/execute happening one
after another in time, with no real notion of physical cost. sCPU is five separate
hardware modules evaluating *simultaneously* every cycle, with a genuine, measurable
area/power/timing cost — and its program is baked directly into synthesized hardware
(`imem`'s lookup table) rather than loaded at runtime, unlike sEMU's `M[]` array.

## `minirv-rtl/` — minirv (8-instruction RTL RISC-V core, D4)

Modular by instruction-stage, same structural-RTL discipline as `scpu-rtl/` (no `always`
blocks outside the provided templates and the two DPI-C-calling units, which require
procedural context):

```
minirv-rtl/vsrc/
├── templates.v   Reg/MuxKey, reused as-is from scpu-rtl/
├── ifu.v          PC register + instruction fetch via DPI-C pmem_read() — replaced an
│                    earlier "C++ hands `inst` to the top level" version once real
│                    memory writes (sw/sb) made that approach unworkable
├── decoder.v       Pure combinational bit-slicing: opcode/funct3/funct7/rd/rs1/rs2,
│                    plus three separate immediate formats (I-type, U-type, S-type —
│                    each with a different assembly rule, so each gets its own output)
├── regfile.v        Built around the course-provided RegisterFile skeleton (write-only
│                    always block, left untouched) — read ports, x0-is-always-zero
│                    (forced on the read side, not the write side), and a third
│                    dedicated debug-only read port hardwired to x10/a0, added around it
├── alu.v             Pure addition — every arithmetic instruction (addi/add/lui/jalr's
│                      target/load-store address calc) reduces to "add two 32-bit
│                      numbers," with the top-level wiring choosing the two inputs
├── lsu.v              Byte/word memory access via DPI-C pmem_read()/pmem_write(),
│                      including byte-lane extraction/shifting for lbu/sb at
│                      non-word-aligned addresses
└── minirv.v            Top-level wiring: all the "which signal wins" decisions live
                         here — ALU input selection, write-back data selection,
                         next-PC selection, load/store control signal generation
```

**Instructions implemented:** `addi`, `add`, `lui`, `jalr`, `lw`, `lbu`, `sw`, `sb`,
`ebreak` (all 8 from the D4 spec, plus the trap instruction) — each added incrementally,
verified with its own hand-encoded test program before moving to the next, per the
handout's staged breakdown (`addi`+`jalr` first, then `ebreak`, then the rest).

**DPI-C is used for three things**, each proven with a minimal standalone test
(`dpi-test/`) before being trusted for the real thing:
- Instruction fetch (`pmem_read`, called from `ifu.v`)
- Load/store memory access (`pmem_read`/`pmem_write`, called from `lsu.v`)
- Simulation termination on `ebreak` (`npc_trap`, called from `minirv.v`)

**Real bugs caught along the way** (not just "it worked first try"):
- `WIDTHTRUNC`/`WIDTHEXPAND` Verilator errors from sloppy bit-slice widths — fixed by
  slicing explicit 4-bit register-address ranges (RV32E only ever uses 16 registers)
  instead of relying on truncation, and matching DPI-C's `byte` argument width exactly
- Debug output sampled *after* a clock edge showed next-cycle values, not the cycle just
  executed — combinational signals derived from a register resettle the instant the
  edge fires, in the same `eval()` call; fixed by sampling before triggering the edge
- Write-enable hardwired to `1'b1` (correct once addi/jalr were the only instructions)
  silently corrupted a register once `sw`/`sb` existed, because S-type's immediate
  encoding reuses the `rd` bit position — fixed with real `wen` logic gating out stores
- `pmem_read` firing during reset (before the `0x80000000` PC reset value had actually
  been latched by a clock edge) read from address `0`, which translates to a wildly
  out-of-bounds array index once `PMEM_BASE`-relative addressing was in place — fixed
  with a bounds check in the C++ memory model

**Run a hand-encoded instruction test:**
```bash
cd minirv-rtl
verilator -cc --exe --build -Mdir build --top-module minirv --trace-fst \
  vsrc/templates.v vsrc/ifu.v vsrc/decoder.v vsrc/regfile.v vsrc/alu.v vsrc/lsu.v vsrc/minirv.v \
  csrc/main.cpp
./build/Vminirv
```

**Run a real AM-compiled program** (see AM setup below):
```bash
cd ../../am-kernels/tests/cpu-tests
make ARCH=minirv-npc ALL=dummy run
```

**AM integration:** `minirv-npc` boots at `0x80000000` (not `0`, unlike the hand-encoded
tests above), `halt()` emits a real `ebreak` via inline assembly
(`abstract-machine/am/src/riscv/npc/trm.c`), and `abstract-machine/scripts/platform/npc.mk`'s
`run:` target invokes `minirv-rtl/build/Vminirv` directly on the compiled `.bin`.

**cpu-tests result: 35/35 PASS**, including programs (`matrix-mul`, `crc32`,
`mul-longlong`) that use instructions `minirv` doesn't implement in RTL at all — made
possible by `minirv-gcc`'s macro-based instruction emulation
(`abstract-machine/tools/minirv/inst-replace.h`): every instruction outside the base 8
(branches, shifts, logic ops, comparisons) compiles into a sequence of load/store
instructions against precomputed lookup tables in memory, rather than a real opcode.
`riscv-tests`/`riscv-arch-test` were deliberately not run — those are hand-written
per-instruction assembly suites with no compiler in the loop to apply that emulation
trick, so the large majority of their test files exercise instructions genuinely absent
from this RTL by design; `cpu-tests`'s 35/35 was judged sufficient evidence for D4's
actual scope.

## D5 — Devices and I/O (complete, including optional VGA)

MMIO for NPC needed **zero RTL changes** — `ifu.v` and `lsu.v` already funnel every
instruction fetch and every load/store through `pmem_read()`/`pmem_write()` in
`minirv-rtl/csrc/main.cpp`, so "implementing a device" just means teaching those two
C++ functions to recognize specific addresses and redirect them, instead of treating
them as real `pmem[]` memory. Addresses deliberately reuse NEMU's own numbers
(`SERIAL_PORT` = `0xa00003f8`, `RTC_ADDR` = `0xa0000048`) — different backend, same
constants, defined once in a new `abstract-machine/am/src/riscv/npc/include/npc.h`
(mirroring `nemu.h`'s pattern).

**UART (`pmem_write`):** a write to `UART_ADDR` is intercepted before the normal
`PMEM_BASE`-relative address math runs (that math would otherwise silently swallow it —
`UART_ADDR` is a device address, not real memory, so treating it as a `pmem[]` offset
would come out nonsensically large and just no-op via the existing bounds check). The
byte itself comes through `wdata & 0xFF`; goes straight to `putchar()` + `fflush()`.
AM-side: `trm.c`'s `putch()` now calls `outb(UART_ADDR, ch)` — the exact same `outb()`
NEMU's `putch()` already used, since it's defined once in `riscv/riscv.h`, shared by
every RISC-V platform.

**Clock (`pmem_read`):** same interception pattern, but the source of "time" is real —
`main.cpp` calls `clock_gettime(CLOCK_MONOTONIC, ...)`, records the first read as a
boot timestamp, and reports elapsed microseconds since then. Since `pmem_read` only
returns one 32-bit `int` per call, the 64-bit microsecond count is split across two
addresses — `RTC_ADDR` (low 32 bits), `RTC_ADDR + 4` (high 32 bits) — same two-halves
approach NEMU's own `RTC_ADDR` register already uses. AM-side: `timer.c`'s
`__am_timer_uptime()` reads both halves via `inl()` and reassembles them, casting the
high half to `uint64_t` *before* shifting left 32 (shifting a still-32-bit value left by
32 is undefined behavior — same subtlety already called out in this repo's NEMU
`timer.c` notes above). `__am_timer_rtc()` (the actual calendar date, not uptime) stays
a placeholder — matches NEMU's own known limitation, not a new gap.

**Real bugs found and fixed along the way:**
- `ISA_H` was never defined for the `minirv-npc` `ARCH` target at all (present in
  `minirv-nemu.mk`, missing from `minirv-npc.mk`) — invisible through all of D4 since
  nothing needed `outb`/`inl` until now. Fixed by adding the matching
  `-DISA_H=\"riscv/riscv.h\"` line, **and** making sure something actually
  `#include`s it (`npc.h` does, mirroring `nemu.h`) — defining the macro alone
  doesn't pull in the header on its own.
- `main.cpp`'s original `MAX_CYCLES = 1000000` was sized for `cpu-tests`-style
  programs that terminate via `ebreak`. Both the real-time clock test and FCEUX
  run indefinitely by design (no `ebreak` ever) — the cap was hit and reported as
  a timeout before a single second of simulated time had even elapsed. Raised to
  2,000,000,000 (`long long`, not `int`, to avoid overflow) with a heartbeat print
  every 10M cycles, so genuine progress is visible instead of a silent wait.
- Two separate build systems share `minirv-rtl/`'s directory without either
  triggering the other: the Verilator build (`verilator -cc --exe --build ...`)
  produces `Vminirv`; the AM program build (`make ARCH=minirv-npc run` from a
  kernel/test directory) only rebuilds the `.bin` and runs whatever `Vminirv`
  already exists on disk. Editing `main.cpp` silently has no effect until
  `Vminirv` is rebuilt explicitly — easy to mistake for "my code change did
  nothing," same shape of bug as the `mainargs` gotcha already documented for
  NEMU builds.

**Verified:**
- `am-kernels/kernels/hello` (`make ARCH=minirv-npc run`): prints
  `Hello, AbstractMachine!` for real, through `outb()` → `sb` → `lsu.v` →
  `pmem_write()` → `putchar()`.
- `am-kernels/tests/am-tests` (`make ARCH=minirv-npc mainargs=t run`): prints one
  line per real elapsed second, confirming `__am_timer_uptime()`'s microsecond count
  tracks actual wall-clock time, not a fake counter.
- `fceux-am` character mode (`HAS_GUI` commented out in `src/config.h`,
  `make ARCH=minirv-npc run`): boots to a full, recognizable ASCII-art Super Mario
  title screen, rendered entirely through the UART path above — no VGA needed for
  this part. FCEUX's own internal FPS/timing readout (`(System time: ...s)`) is a
  second, independent confirmation the clock works, on top of the RTC test.
  Runs indefinitely by design (continuous redraw loop, never calls `ebreak`) — stop
  with `Ctrl+C` once the image is visibly stable, same as the `donut` demo from
  PA2.3.

**VGA:** a real framebuffer region (`FB_ADDR = 0xa1000000`, one `uint32_t` per pixel
across a `400×300` screen, `480,000` bytes total) plus a small VGA control register
(`VGACTL_ADDR = 0xa0000100`: word 0 = `(width<<16)|height`, read-only; word 0+4 = a
sync-trigger the program writes to after finishing a frame). Structurally different from
UART/clock — this is an address *range* to recognize in `pmem_write`, not a single fixed
address, and unlike NEMU (which decouples "flag written" from "redraw" via a separate
periodic polling function), NPC's `pmem_write` has no equivalent background loop — it
only ever runs synchronously, in direct response to a store instruction — so the sync
write triggers the redraw immediately, right there in the same function call.

AM-side: a new `abstract-machine/am/src/riscv/npc/gpu.c`, mirroring NEMU's own `gpu.c`
field-for-field (`__am_gpu_config`, `__am_gpu_fbdraw`, `__am_gpu_status`, same
`AM_GPU_CONFIG_T`/`AM_GPU_FBDRAW_T`/`AM_GPU_STATUS_T` fields from `amdev.h`), registered
into `ioe.c`'s `lut[]` dispatch table, and added to `AM_SRCS` in
`scripts/platform/npc.mk` (unlike `trm.c`/`timer.c`/`ioe.c`, new files aren't
auto-discovered — that list is explicit, and a new file sitting on disk but missing from
it just never gets compiled, silently). `main.cpp`-side: a `vmem[]` pixel array plus a
real SDL window (`init_vga_if_needed()` lazy-inits it — so running `hello`/`cpu-tests`
never pops up a window — `vga_redraw()` does the same `SDL_UpdateTexture` →
`SDL_RenderClear` → `SDL_RenderCopy` → `SDL_RenderPresent` sequence NEMU's own
`update_screen()` uses). Needed one addition to the Verilator build command that UART/
clock never did — SDL flags, since nothing linked against it before:
```
-CFLAGS "$(sdl2-config --cflags)" -LDFLAGS "$(sdl2-config --libs)"
```

**Real bug hit again here:** the exact "access nonexist register" panic from `ioe.c`
showed up a second time, later, for a different reason than the `fceux-am` repo-boundary
issue below — attempting `HAS_GUI` on before `gpu.c`/`ioe.c`/`AM_SRCS` were actually
written and wired up. Confirms the general shape of the earlier UART/clock bugs: turning
a feature "on" (`HAS_GUI`) only controls whether a program *attempts* to use a device —
whether that attempt succeeds depends entirely on whether an implementation exists to
answer it. `ARCH=riscv32-nemu`/`ARCH=native` never hit this because their `gpu.c`
(`am/src/platform/nemu/ioe/gpu.c`, `am/src/native/ioe/gpu.c`) already shipped as part of
the framework — NPC's didn't exist until this work wrote it.

Also worth noting: the long-running waveform trace (`tfp->dump()` on every clock edge,
unconditionally, since D4) is fine for short unit tests but risks unbounded disk growth
on long device-driven runs like FCEUX (150M+ cycles). Capped to the first
`TRACE_CYCLES = 200000` cycles only — long runs still simulate correctly past that
point, just without dumping every single edge to `wave.fst`.

**Verified:**
- `am-kernels/kernels/hello` (`make ARCH=minirv-npc run`): prints
  `Hello, AbstractMachine!` for real, through `outb()` → `sb` → `lsu.v` →
  `pmem_write()` → `putchar()`.
- `am-kernels/tests/am-tests` (`make ARCH=minirv-npc mainargs=t run`): prints one
  line per real elapsed second, confirming `__am_timer_uptime()`'s microsecond count
  tracks actual wall-clock time, not a fake counter.
- `am-kernels/tests/am-tests` (`make ARCH=minirv-npc mainargs=v run`): a real SDL
  window shows the expected animated colored-square pattern, confirming the whole
  `AM_GPU_FBDRAW` → `gpu.c` → `pmem_write`'s `FB_ADDR` check → `vmem[]` →
  `vga_redraw()` chain end to end.
- `fceux-am` character mode (`HAS_GUI` commented out in `src/config.h`,
  `make ARCH=minirv-npc run`): boots to a full, recognizable ASCII-art Super Mario
  title screen, rendered entirely through the UART path above — no VGA needed for
  this part. FCEUX's own internal FPS/timing readout (`(System time: ...s)`) is a
  second, independent confirmation the clock works, on top of the RTC test.
  Runs indefinitely by design (continuous redraw loop, never calls `ebreak`) — stop
  with `Ctrl+C` once the image is visibly stable, same as the `donut` demo from
  PA2.3.
- `fceux-am` graphical mode (`HAS_GUI` on, `make ARCH=minirv-npc run`): boots to a
  correct, fully colored Super Mario title screen in a real SDL window — the actual
  goal of D5's optional section. Same as character mode, runs indefinitely by
  design; a run was left going 2 billion cycles (`HIT BAD TRAP (timeout)`) with the
  image stable and correct throughout, rather than stopped early.

**`fceux-am` gotcha (repo-boundary, not npc-specific):** `fceux-am` is cloned as its
*own* separate git repo, independent of `OSOC`. Local edits to it don't get carried
along by anything in `OSOC` itself — they need their own `git commit`/`push` inside
`fceux-am/`, or a fresh clone reverts them. Cost real debugging time once: after
commenting out `#define HAS_GUI` for character mode without committing it in
`fceux-am`'s own repo, a fresh clone on a second machine silently had `HAS_GUI` back on.
Since `gpu.c`/`ioe.c` didn't exist yet at that point, `sdl-video.cpp`'s GPU calls
(`io_read(AM_GPU_CONFIG)`, `io_write(AM_GPU_FBDRAW)`, only compiled in `#ifdef HAS_GUI`)
ran fine for a long while — the ASCII path this replaces only becomes unreachable once
the emulated NES produces an actual first frame, which took ~150M cycles to reach —
before panicking with `access nonexist register`. Now that `gpu.c` is written and
`HAS_GUI` genuinely works, it's been committed for real in `fceux-am`'s own repo (not
`OSOC`) as the final state — this isn't a live footgun anymore, just a reminder that
edits to `fceux-am`/`am-kernels`/`nvboard` (all separate repos from `OSOC`) need their
own commits.

## Running things

Each subfolder is self-contained:
```bash
cd xor-test && make          # plain Verilator sim, Ctrl+C to stop
cd nvboard-xor && make run   # opens a virtual board window
cd nvboard-light && make run # same, for the running-lights circuit
cd scpu-rtl && <see build command above>
cd dpi-test && verilator -cc --exe --build -Mdir build --top-module dpi_top \
  vsrc/dpi_top.v csrc/main.cpp && ./build/Vdpi_top
cd minirv-rtl && <see build command above>
```

The top-level `Makefile` here (not in a subfolder) has the framework-provided `sim`
target, which includes a git-tracking call (`$(call git_commit, ...)`) — **do not
remove that line**, it's the course's originality-tracking mechanism. Run with:
```bash
make sim
```
Note: this currently references `$NEMU_HOME`, which isn't set yet (that's set up during
the PA phase, not yet reached) — the tracer commands fail silently rather than blocking
the build.

## Setup

Needs `$NPC_HOME` (set via `bash init.sh npc` from `ysyx-workbench/`), Verilator v5.008
built from source (see the top-level `OSOC/README.md` for exact steps — apt's version is
too old), and for the `nvboard-*` folders, `$NVBOARD_HOME` (via `bash init.sh nvboard`)
plus `libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev`.
