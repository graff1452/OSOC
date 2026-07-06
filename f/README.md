# Mini RISC-V Processor (Logisim Evolution)

A fully functional 8-instruction RISC-V processor built from scratch in [Logisim Evolution](https://github.com/logisim-evolution/logisim-evolution), implemented as part of the **F6** module of the [一生一芯 (ysyx) "One Student, One Chip"](https://ysyx.oscc.cc/) course.

The processor was built incrementally, stage by stage, with hardware-level testing after every component — no HDL, no simulation shortcuts, just gate- and component-level design in Logisim's graphical editor.

## Features

- **32-bit architecture**: 32-bit PC, 32 general-purpose 32-bit registers, byte-addressable memory
- **32 GPRs**, with `x0` hardwired to zero (writes to it are silently discarded)
- **8 RV32I instructions**, fully implemented and tested:

  | Instruction | Type | Description |
  |---|---|---|
  | `add`  | R-type | `rd = rs1 + rs2` |
  | `addi` | I-type | `rd = rs1 + sext(imm)` |
  | `lui`  | U-type | `rd = imm << 12` |
  | `lw`   | I-type | Load 32-bit word from memory |
  | `sw`   | S-type | Store 32-bit word to memory |
  | `lbu`  | I-type | Load unsigned byte from memory (zero-extended) |
  | `sb`   | S-type | Store one byte to memory |
  | `jalr` | I-type | `rd = pc + 4; pc = rs1 + imm` |

- **Instruction ROM** (separate from data RAM), loaded from `.hex` files
- **Byte-addressable data RAM** with individual byte-enable (`BE0`–`BE3`) lines for `sb`/`lbu` support
- **Memory-mapped RGB video output** (256×256, 24-bit color), addressed at `0x20000000`–`0x2003FFFF`, with a hardware address decoder routing `sw` writes to either RAM or the display

## Architecture Overview

The processor follows the classic state-machine model taught earlier in the course: state = `{PC, Register File, Memory}`, and every instruction executes the same four-phase cycle — **fetch → decode → execute → update PC**.

### Datapath
- PC register + adder (PC+4) + next-PC mux (overridden by `jalr`)
- Instruction ROM, word-addressed via `PC[N:2]`
- Instruction field splitter (opcode, funct3, rd, rs1, rs2, I-type/S-type/U-type immediates)
- Sign/zero extenders for each immediate format
- 32-register file with two independent read ports and one decoder-gated write port
- ALU (single adder) with a multiplexed second operand (`imm`, `rs2`, or S-type `imm`, depending on instruction)
- Byte-addressable RAM with per-lane byte enables, plus byte-select/replicate logic for `lbu`/`sb`
- 4-way write-back multiplexer (ALU result / PC+4 / LUI immediate / memory read data)
- Address decoder (`isVGA` / `isMem`) routing stores to RAM or the RGB Video peripheral

### Control
- Instruction identification via **opcode comparators** (with an added **funct3 comparator** wherever two instructions share an opcode, e.g. `lw`/`lbu` and `sw`/`sb`)
- No microcode or generic decoder — each instruction's identity is a single clean one-hot signal (`is_addi`, `is_add`, `is_lui`, `is_jalr`, `is_lw`, `is_sw`, `is_lbu`, `is_sb`) driving every downstream mux/enable

## Testing

Every stage was verified in isolation before integration, following a test-driven build process:

1. Individual instructions were hand-encoded and verified against the RISC-V ISA manual (Ch. 36 instruction listings) before being poked into ROM.
2. Each new datapath component (splitters, comparators, muxes, sign extenders, the register file, RAM) was tested standalone with manual input pins before being wired into the full datapath.
3. **Milestone test** (from the course handout): a 6-instruction `addi`/`jalr` program exercising jumps, return addresses, and `x0`'s write-immunity.
4. **Full regression tests** after every new instruction, to confirm previously-working instructions weren't broken by new wiring.
5. **`mem.hex`** — a ~75,000-word compiled test suite exercising every instruction across many edge cases. Passes (`a0 == 0`, halts at the expected address).
6. **`sum.hex`** — computes 1+2+...+100 and self-checks the result. Passes (`a0 == 0`, halts correctly).
7. **`vga.hex`** — a 628,000-cycle program that renders the "一生一芯" logo via the memory-mapped RGB display. Runs to completion and renders correctly.

## Bugs Found & Fixed Along the Way

Debugging real compiled test programs surfaced several issues that simple hand-written tests hadn't caught:

- **Missing `RegWrite` terms**: each new instruction that writes a register (`add`, `lui`, `lw`, `lbu`) needed to be explicitly OR'd into the register file's write-enable signal — easy to forget when adding a new instruction.
- **Reversed byte-enable lane order**: `sb`'s byte-lane wiring was initially flipped, causing stores to land in the wrong byte position.
- **`sb` using the wrong immediate**: the ALU-input mux's immediate-selection logic was only wired to switch to the S-type immediate for `is_sw`, not `is_sb` — meaning `sb` silently computed its target address using the I-type immediate instead. Found via systematic register-by-register tracing after `mem.hex`'s built-in self-check failed on its very first test case.

## Tools

- [Logisim Evolution](https://github.com/logisim-evolution/logisim-evolution)
- Course materials and test programs from the [ysyx](https://ysyx.oscc.cc/) "一生一芯" program

## What's Next

Per the course syllabus, the next stage (**D4**) reimplements this same processor using RTL (Verilog), after foundational modules on C programming, HDL basics, and the Linux/tapeout toolchain (E stage).