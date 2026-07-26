module minirv
(
  input  clk,
  input  rst,
  output [31:0] pc,
  output [3:0]  dbg_rd,
  output [31:0] dbg_wdata,
  output [31:0] dbg_a0,
  output [31:0] dbg_mem_addr,
  output        dbg_mem_wen,
  output        dbg_mem_ren,
  output [31:0] dbg_inst,
  output [31:0] dbg_mem_wdata,
  output [31:0] dbg_mstatus,
  output [31:0] dbg_mtvec,
  output [31:0] dbg_mepc,
  output [31:0] dbg_mcause,
  output        dbg_if_valid,
  output        dbg_retire
);

  import "DPI-C" function void npc_trap();

  wire [31:0] next_pc, imm_i, imm_u, imm_s, imm_b, imm_j, rs1_val, rs2_val, alu_result, inst, lsu_rdata;
  wire [6:0]  opcode;
  wire [2:0]  funct3;
  wire [6:0]  funct7;
  wire [3:0]  rd, rs1, rs2;

  // --- Step 2: real 1-cycle-latency IFU timing --------------------------------
  // ifu now runs a 2-state FETCH/EXEC FSM instead of instant DPI-C access.
  // if_valid is high during the EXEC cycle, when inst/pc are stable and every
  // other module in this file is allowed to act on them; low during FETCH,
  // when inst is stale-but-unchanged and nothing downstream should fire.
  // NOTE: after this change, DiffTest will report spurious mismatches until
  // step 3 fixes the C++ harness to only check on if_valid cycles -- expected,
  // not a regression.
  wire if_valid;

  // --- Step 4: real 1-cycle-latency LSU timing --------------------------------
  // exec_done tells ifu.v when the current instruction's EXEC phase can
  // actually end: immediately (every cycle) for anything that isn't a
  // load/store, or only once lsu_valid says the memory response is ready.
  // Declared here (before lsu_wen/lsu_ren/lsu_valid exist below) since Verilog
  // wires don't care about declaration order for combinational reads -- kept
  // up here so it sits next to if_valid, the signal it's paired with.
  //
  // retiring is 1 only on the FINAL cycle of EXEC -- 1 cycle for everything
  // except loads/stores (2 cycles: request + wait-for-LSU). Every "fires once
  // per instruction" gate that used to key off if_valid alone (reg_wen, the
  // CSR write-enables, ebreak's npc_trap() call) now uses retiring instead,
  // because if_valid itself stays high for BOTH cycles of a stalled
  // load/store -- without this, those would each fire twice per load/store.
  // lsu_wen/lsu_ren themselves deliberately stay gated on plain if_valid, not
  // retiring: the LSU needs to see the request asserted across both cycles;
  // its own internal busy/new_req logic (lsu.v) is what prevents a second,
  // spurious transaction from starting on the response cycle.
  wire lsu_valid;
  wire exec_done = !(lsu_wen || lsu_ren) || lsu_valid;
  wire retiring  = if_valid && exec_done;

  wire [31:0] pc_plus4    = pc + 32'd4;
  wire        is_jalr     = (opcode == 7'b1100111);
  wire        is_jal      = (opcode == 7'b1101111);
  wire        is_rtype    = (opcode == 7'b0110011);
  wire        is_itype    = (opcode == 7'b0010011);   // ALU-immediate family
  wire        is_lui      = (opcode == 7'b0110111);
  wire        is_auipc    = (opcode == 7'b0010111);
  wire        is_ebreak   = (inst == 32'h00100073);
  wire        is_load     = (opcode == 7'b0000011);
  wire        is_lb       = is_load && (funct3 == 3'b000);
  wire        is_lh       = is_load && (funct3 == 3'b001);
  wire        is_lw       = is_load && (funct3 == 3'b010);
  wire        is_lbu      = is_load && (funct3 == 3'b100);
  wire        is_lhu      = is_load && (funct3 == 3'b101);
  wire        is_store    = (opcode == 7'b0100011);
  wire        is_sb       = is_store && (funct3 == 3'b000);
  wire        is_sh       = is_store && (funct3 == 3'b001);
  wire        is_sw       = is_store && (funct3 == 3'b010);
  wire        is_load_any = is_lb || is_lh || is_lw || is_lbu || is_lhu;
  wire [31:0] jalr_target = (rs1_val + imm_i) & ~32'd1;

  // --- C5: trap/exception instructions and CSR access -----------------------
  // ecall/mret are fixed 32-bit patterns (all operand fields are zero/reserved
  // for these two), matched the exact same way is_ebreak already is above --
  // not a new pattern, just reusing the existing style.
  wire        is_ecall = (inst == 32'h00000073);
  wire        is_mret  = (inst == 32'h30200073);
  // csrrw/csrrs share the SYSTEM opcode with ecall/mret; funct3 tells them apart.
  wire        is_csrrw = (opcode == 7'b1110011) && (funct3 == 3'b001);
  wire        is_csrrs = (opcode == 7'b1110011) && (funct3 == 3'b010);
  // CSR address is a raw unsigned 12-bit field at the same bit position imm_i
  // reads from, just without imm_i's sign-extension -- read directly off inst.
  wire [11:0] csr_addr = inst[31:20];

  // R-type (register-register): funct3 selects the op, funct7 additionally
  // distinguishes add/sub and srl/sra (identical funct3, opposite funct7 bit 5)
  wire        is_add  = is_rtype && (funct3 == 3'b000) && (funct7 == 7'b0000000);
  wire        is_sub  = is_rtype && (funct3 == 3'b000) && (funct7 == 7'b0100000);
  wire        is_sll  = is_rtype && (funct3 == 3'b001);
  wire        is_slt  = is_rtype && (funct3 == 3'b010);
  wire        is_sltu = is_rtype && (funct3 == 3'b011);
  wire        is_xor  = is_rtype && (funct3 == 3'b100);
  wire        is_srl  = is_rtype && (funct3 == 3'b101) && (funct7 == 7'b0000000);
  wire        is_sra  = is_rtype && (funct3 == 3'b101) && (funct7 == 7'b0100000);
  wire        is_or   = is_rtype && (funct3 == 3'b110);
  wire        is_and  = is_rtype && (funct3 == 3'b111);
  wire        is_rtype_alu = is_add || is_sub || is_sll || is_slt || is_sltu
                            || is_xor || is_srl || is_sra || is_or || is_and;

  // I-type ALU-immediate: same funct3 grouping as R-type, immediate instead of rs2.
  // Shift-immediate reuses funct7 (inst[31:25]) the same way R-type shifts do.
  wire        is_addi  = is_itype && (funct3 == 3'b000);
  wire        is_slli  = is_itype && (funct3 == 3'b001);
  wire        is_slti  = is_itype && (funct3 == 3'b010);
  wire        is_sltiu = is_itype && (funct3 == 3'b011);
  wire        is_xori  = is_itype && (funct3 == 3'b100);
  wire        is_srli  = is_itype && (funct3 == 3'b101) && (funct7 == 7'b0000000);
  wire        is_srai  = is_itype && (funct3 == 3'b101) && (funct7 == 7'b0100000);
  wire        is_ori   = is_itype && (funct3 == 3'b110);
  wire        is_andi  = is_itype && (funct3 == 3'b111);

  // Branches: opcode alone says "this is some branch", funct3 says which comparison
  wire        is_branch   = (opcode == 7'b1100011);
  wire        is_beq      = is_branch && (funct3 == 3'b000);
  wire        is_bne      = is_branch && (funct3 == 3'b001);
  wire        is_blt      = is_branch && (funct3 == 3'b100);
  wire        is_bge      = is_branch && (funct3 == 3'b101);
  wire        is_bltu     = is_branch && (funct3 == 3'b110);
  wire        is_bgeu     = is_branch && (funct3 == 3'b111);
  wire        branch_taken = (is_beq  &&  (rs1_val == rs2_val))
                            | (is_bne  &&  (rs1_val != rs2_val))
                            | (is_blt  &&  ($signed(rs1_val) <  $signed(rs2_val)))
                            | (is_bge  &&  ($signed(rs1_val) >= $signed(rs2_val)))
                            | (is_bltu &&  (rs1_val < rs2_val))
                            | (is_bgeu &&  (rs1_val >= rs2_val));

  // Allow-list, not deny-list: only instructions we actually recognize write a
  // register. Anything else (branches, and-yet-unimplemented instructions) is a
  // safe no-op instead of silently corrupting whatever register its garbage-decoded
  // "rd" field happens to land on -- decoder.v extracts rd/imm_i at the same fixed
  // bit positions for every instruction, which is only actually correct for the
  // instruction types below.
  // Step 2: gated so a register write fires only once per instruction, not
  // once per FETCH cycle too. Step 4: gate is now `retiring`, not raw
  // if_valid -- if_valid alone stays high for BOTH cycles of a stalled load,
  // and is_load_any is literally in this allow-list, so without this change
  // a load would now write its register TWICE (once too early, with stale
  // lsu_rdata, then again correctly).
  wire        reg_wen     = (is_addi || is_lui || is_auipc || is_jalr || is_jal || is_load_any
                           || is_rtype_alu
                           || is_slli || is_slti || is_sltiu || is_xori || is_srli || is_srai || is_ori || is_andi
                           || is_lb || is_lh || is_lhu
                           || is_csrrw || is_csrrs) && retiring;

  wire [31:0] alu_a = is_lui   ? 32'b0
                     : is_auipc ? pc
                     : rs1_val;
  wire [31:0] alu_b = is_rtype_alu ? rs2_val
                     : (is_lui || is_auipc) ? imm_u
                     : is_store ? imm_s
                     : imm_i;   // covers addi, jalr, lw, lbu, and every I-type ALU-immediate op

  // Which operation the ALU should perform -- R-type and its matching I-type
  // immediate op always share the same code, since they do the same math, just
  // with a different second operand (chosen by alu_b above).
  wire [3:0] alu_op = (is_sub)                ? 4'd1
                    : (is_and  || is_andi)    ? 4'd2
                    : (is_or   || is_ori)     ? 4'd3
                    : (is_xor  || is_xori)    ? 4'd4
                    : (is_sll  || is_slli)    ? 4'd5
                    : (is_srl  || is_srli)    ? 4'd6
                    : (is_sra  || is_srai)    ? 4'd7
                    : (is_slt  || is_slti)    ? 4'd8
                    : (is_sltu || is_sltiu)   ? 4'd9
                    : 4'd0;   // ADD -- covers add, addi, lui, auipc, jalr target, load/store address

  // --- C5: CSR storage --------------------------------------------------------
  // Same Reg template already used for pc -- one flip-flop bank per CSR, no new
  // primitive needed. mstatus resets to 0x1800 (matching NEMU's own boot-time
  // init in isa/riscv32/init.c) purely so DiffTest agrees from instruction zero;
  // nothing in this design actually reads mstatus's individual bits yet, per the
  // handout's own simplification. mtvec/mepc/mcause reset to 0 and are only ever
  // meaningful once software (cte_init/isa_raise_intr-equivalent traffic) sets
  // them for real.
  wire [31:0] mstatus_q, mtvec_q, mepc_q, mcause_q;
  wire [31:0] csr_rdata = (csr_addr == 12'h300) ? mstatus_q
                         : (csr_addr == 12'h305) ? mtvec_q
                         : (csr_addr == 12'h341) ? mepc_q
                         : (csr_addr == 12'h342) ? mcause_q
                         : 32'b0;
  // csrrw overwrites outright; csrrs ORs rs1_val's set bits into the existing
  // value -- when rs1 is x0 (trap.S's `csrr` pseudo-op), this reduces to a pure
  // read with no side effect, same as NEMU's own csrrs implementation.
  wire [31:0] csr_wdata = is_csrrw ? rs1_val : (csr_rdata | rs1_val);

  // Step 2: all four CSR write-enables gated so they fire once per
  // instruction. Step 4: gate switched from if_valid to retiring -- CSR
  // instructions (csrrw/csrrs/ecall) aren't loads/stores themselves, so
  // exec_done is always 1 for them and retiring==if_valid in their case; the
  // switch is really just for consistency/safety, not a behavior change for
  // these specific instructions.
  wire wen_mstatus = (is_csrrw || is_csrrs) && (csr_addr == 12'h300) && retiring;
  wire wen_mtvec   = (is_csrrw || is_csrrs) && (csr_addr == 12'h305) && retiring;
  // mepc/mcause can also be written by ecall itself (hardware trap response),
  // not just by explicit csrrw/csrrs -- ecall and csrrw/csrrs can never both be
  // true at once (mutually exclusive exact-match/funct3 patterns), so no
  // priority conflict, just two independent ways to reach the same register.
  wire wen_mepc    = (is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h341))) && retiring;
  wire wen_mcause  = (is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h342))) && retiring;

  wire [31:0] mepc_din   = is_ecall ? pc     : csr_wdata;
  // Cause code 11 is RISC-V's spec-fixed value for "Environment call from
  // M-mode" -- the same constant already used on the NEMU side.
  wire [31:0] mcause_din = is_ecall ? 32'd11 : csr_wdata;

  Reg #(32, 32'h00001800) u_mstatus (clk, rst, csr_wdata,  mstatus_q, wen_mstatus);
  Reg #(32, 32'h00000000) u_mtvec   (clk, rst, csr_wdata,  mtvec_q,   wen_mtvec);
  Reg #(32, 32'h00000000) u_mepc    (clk, rst, mepc_din,   mepc_q,    wen_mepc);
  Reg #(32, 32'h00000000) u_mcause  (clk, rst, mcause_din, mcause_q,  wen_mcause);

  // ecall/mret both cause a jump -- reusing the exact same next-address datapath
  // every other control-flow instruction (jal/jalr/branch) already goes through,
  // per the handout's own suggestion, rather than adding a separate mechanism.
  // NOTE: next_pc itself needs NO if_valid gating -- ifu's own internal r_pc
  // (wen=valid) already only latches next_pc once per instruction, at the end
  // of the EXEC cycle. Gating it again here would be redundant.
  assign next_pc = is_ecall                     ? mtvec_q
                  : is_mret                      ? mepc_q
                  : is_jalr                      ? jalr_target
                  : is_jal                       ? pc + imm_j
                  : (is_branch && branch_taken)  ? pc + imm_b
                  : pc_plus4;

  // Step 2: gated by if_valid -- prevents a load/store from being seen by the
  // LSU during the following FETCH cycle, where alu_result/rs2_val are
  // stale-but-unchanged. Step 4: deliberately LEFT as if_valid, not retiring
  // -- these need to stay asserted across BOTH cycles of a stalled access
  // (request cycle + wait-for-LSU cycle) so lsu.v can see the request appear
  // on the first of those cycles. lsu.v's own busy/new_req logic (not this
  // gate) is what stops a second transaction from starting on the second
  // cycle.
  wire        lsu_wen  = (is_sw || is_sh || is_sb) && if_valid;
  wire        lsu_ren  = is_load_any && if_valid;

  wire [31:0] wdata = (is_jalr || is_jal)     ? pc_plus4
                     : is_load_any            ? lsu_rdata
                     : (is_csrrw || is_csrrs) ? csr_rdata   // old value, per csrrw/csrrs semantics
                     : alu_result;

  // --- Step 7: single shared memory, arbitrated between IFU and LSU ------
  // Was two separate SRAM instances (axi_mem_i + axi_mem_d) through step 6;
  // now both masters share ONE memory, matching real hardware (a chip
  // doesn't get two physically separate address spaces for instructions
  // vs. data). ifu.v/lsu.v themselves are completely unaware of this --
  // each still drives its own AR/R (and for lsu, AW/W/B) ports exactly as
  // before, just wired to the arbiter's per-master ports instead of
  // directly to a dedicated slave.
  wire [31:0] i_araddr;  wire i_arvalid;  wire i_arready;
  wire [31:0] i_rdata;   wire [1:0] i_rresp; wire i_rvalid; wire i_rready;

  ifu u_ifu (clk, rst, next_pc, exec_done, pc, inst, if_valid,
             i_araddr, i_arvalid, i_arready, i_rdata, i_rresp, i_rvalid, i_rready);

  decoder u_dec (inst, opcode, funct3, funct7, rd, rs1, rs2, imm_i, imm_u, imm_s, imm_b, imm_j);

  RegisterFile #(4, 32) u_reg 
  (
    clk, wdata, rd, reg_wen,
    rs1, rs2,
    rs1_val, rs2_val, dbg_a0
  );

  alu u_alu (alu_a, alu_b, alu_op, alu_result);

  wire [31:0] d_araddr;  wire d_arvalid;  wire d_arready;
  wire [31:0] d_rdata;   wire [1:0] d_rresp; wire d_rvalid; wire d_rready;
  wire [31:0] d_awaddr;  wire d_awvalid;  wire d_awready;
  wire [31:0] d_wdata;   wire [3:0] d_wstrb; wire d_wvalid; wire d_wready;
  wire [1:0]  d_bresp;   wire d_bvalid;   wire d_bready;

  lsu u_lsu (clk, rst, alu_result, rs2_val, lsu_wen, lsu_ren, funct3, lsu_rdata, lsu_valid,
             d_araddr, d_arvalid, d_arready, d_rdata, d_rresp, d_rvalid, d_rready,
             d_awaddr, d_awvalid, d_awready, d_wdata, d_wstrb, d_wvalid, d_wready,
             d_bresp, d_bvalid, d_bready);

  wire [31:0] s_araddr;  wire s_arvalid;  wire s_arready;
  wire [31:0] s_rdata;   wire [1:0] s_rresp; wire s_rvalid; wire s_rready;
  wire [31:0] s_awaddr;  wire s_awvalid;  wire s_awready;
  wire [31:0] s_wdata;   wire [3:0] s_wstrb; wire s_wvalid; wire s_wready;
  wire [1:0]  s_bresp;   wire s_bvalid;   wire s_bready;

  arbiter u_arb (clk, rst,
             i_araddr, i_arvalid, i_arready, i_rdata, i_rresp, i_rvalid, i_rready,
             d_araddr, d_arvalid, d_arready, d_rdata, d_rresp, d_rvalid, d_rready,
             d_awaddr, d_awvalid, d_awready, d_wdata, d_wstrb, d_wvalid, d_wready,
             d_bresp, d_bvalid, d_bready,
             s_araddr, s_arvalid, s_arready, s_rdata, s_rresp, s_rvalid, s_rready,
             s_awaddr, s_awvalid, s_awready, s_wdata, s_wstrb, s_wvalid, s_wready,
             s_bresp, s_bvalid, s_bready);

  // --- Step 9: crossbar routes the arbiter's single shared bus to one of
  // three real slaves, by address -- replacing the DPI-C UART/CLINT
  // special-casing in main.cpp's pmem_read/pmem_write with actual RTL.
  // Only UART and CLINT move here; axi_mem still handles the real SRAM
  // range plus KBD/VGA/framebuffer, exactly as before, unaffected.
  wire [31:0] mem_araddr;  wire mem_arvalid;  wire mem_arready;
  wire [31:0] mem_rdata;   wire [1:0] mem_rresp; wire mem_rvalid; wire mem_rready;
  wire [31:0] mem_awaddr;  wire mem_awvalid;  wire mem_awready;
  wire [31:0] mem_wdata;   wire [3:0] mem_wstrb; wire mem_wvalid; wire mem_wready;
  wire [1:0]  mem_bresp;   wire mem_bvalid;   wire mem_bready;

  wire [31:0] uart_awaddr; wire uart_awvalid; wire uart_awready;
  wire [31:0] uart_wdata;  wire [3:0] uart_wstrb; wire uart_wvalid; wire uart_wready;
  wire [1:0]  uart_bresp;  wire uart_bvalid;  wire uart_bready;

  wire [31:0] clint_araddr; wire clint_arvalid; wire clint_arready;
  wire [31:0] clint_rdata;  wire [1:0] clint_rresp; wire clint_rvalid; wire clint_rready;

  xbar u_xbar (clk, rst,
             s_araddr, s_arvalid, s_arready, s_rdata, s_rresp, s_rvalid, s_rready,
             s_awaddr, s_awvalid, s_awready, s_wdata, s_wstrb, s_wvalid, s_wready,
             s_bresp, s_bvalid, s_bready,
             mem_araddr, mem_arvalid, mem_arready, mem_rdata, mem_rresp, mem_rvalid, mem_rready,
             mem_awaddr, mem_awvalid, mem_awready, mem_wdata, mem_wstrb, mem_wvalid, mem_wready,
             mem_bresp, mem_bvalid, mem_bready,
             uart_awaddr, uart_awvalid, uart_awready, uart_wdata, uart_wstrb, uart_wvalid, uart_wready,
             uart_bresp, uart_bvalid, uart_bready,
             clint_araddr, clint_arvalid, clint_arready, clint_rdata, clint_rresp, clint_rvalid, clint_rready);

  axi_mem u_mem (clk, rst,
             mem_araddr, mem_arvalid, mem_arready, mem_rdata, mem_rresp, mem_rvalid, mem_rready,
             mem_awaddr, mem_awvalid, mem_awready, mem_wdata, mem_wstrb, mem_wvalid, mem_wready,
             mem_bresp, mem_bvalid, mem_bready);

  uart u_uart (clk, rst,
             uart_awaddr, uart_awvalid, uart_awready, uart_wdata, uart_wstrb, uart_wvalid, uart_wready,
             uart_bresp, uart_bvalid, uart_bready);

  clint u_clint (clk, rst,
             clint_araddr, clint_arvalid, clint_arready, clint_rdata, clint_rresp, clint_rvalid, clint_rready);

  assign dbg_rd    = rd;
  assign dbg_wdata = wdata;
  assign dbg_mem_addr = alu_result;
  // Step 4: also gated by lsu_valid -- lsu_wen/lsu_ren themselves now stay
  // high across BOTH cycles of a stalled access (see comment above), but
  // rdata/the write side effect are only actually correct/complete on the
  // response cycle. Without this, main.cpp's mtrace would log every load and
  // store twice: once on the request cycle with stale rdata, once for real
  // on the response cycle.
  assign dbg_mem_wen  = lsu_wen && lsu_valid;
  assign dbg_mem_ren  = lsu_ren && lsu_valid;
  assign dbg_inst      = inst;
  assign dbg_mem_wdata = rs2_val;
  assign dbg_mstatus = mstatus_q;
  assign dbg_mtvec   = mtvec_q;
  assign dbg_mepc    = mepc_q;
  assign dbg_mcause  = mcause_q;
  assign dbg_if_valid = if_valid;
  // Step 4: new port -- main.cpp should sample THIS, not dbg_if_valid, for
  // "did an instruction retire this cycle" bookkeeping (itrace/ftrace/
  // DiffTest/watchpoints). dbg_if_valid stays high across both cycles of a
  // stalled load/store now; dbg_retire is 1 on exactly the final one.
  assign dbg_retire = retiring;

  always @(posedge clk) 
  begin
    // Step 2: gated so ebreak fires npc_trap() once per instruction. Step 4:
    // switched from if_valid to retiring, same reasoning as reg_wen -- ebreak
    // itself is never a load/store, so this is a no-op behavior change for
    // ebreak specifically, just consistency with the rest of the file.
    if (is_ebreak && retiring) npc_trap();
  end
endmodule
