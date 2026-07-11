# npc — RTL Reimplementation of minirv

This is where the `minirv` processor (originally built in Logisim, see `../../f/`) gets
reimplemented as real Verilog RTL, verified with Verilator. Currently in the E5 stage:
having built up the Verilator/NVBoard toolchain on small throwaway circuits, the first
real processor build — **sCPU** (the sISA processor, not yet `minirv`) — is now working.

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

## Running things

Each subfolder is self-contained:
```bash
cd xor-test && make          # plain Verilator sim, Ctrl+C to stop
cd nvboard-xor && make run   # opens a virtual board window
cd nvboard-light && make run # same, for the running-lights circuit
cd scpu-rtl && <see build command above>
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
