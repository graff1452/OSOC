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
  output [31:0] dbg_mcause
);

  import "DPI-C" function void npc_trap();
  wire [31:0] next_pc, imm_i, imm_u, imm_s, imm_b, imm_j, rs1_val, rs2_val, alu_result, inst, lsu_rdata;
  wire [6:0]  opcode; 
  wire [2:0]  funct3;
  wire [6:0]  funct7;
  wire [3:0]  rd, rs1, rs2;
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
  wire        reg_wen     = is_addi || is_lui || is_auipc || is_jalr || is_jal || is_load_any
                           || is_rtype_alu
                           || is_slli || is_slti || is_sltiu || is_xori || is_srli || is_srai || is_ori || is_andi
                           || is_lb || is_lh || is_lhu
                           || is_csrrw || is_csrrs;

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

  wire wen_mstatus = (is_csrrw || is_csrrs) && (csr_addr == 12'h300);
  wire wen_mtvec   = (is_csrrw || is_csrrs) && (csr_addr == 12'h305);
  // mepc/mcause can also be written by ecall itself (hardware trap response),
  // not just by explicit csrrw/csrrs -- ecall and csrrw/csrrs can never both be
  // true at once (mutually exclusive exact-match/funct3 patterns), so no
  // priority conflict, just two independent ways to reach the same register.
  wire wen_mepc    = is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h341));
  wire wen_mcause  = is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h342));

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
  assign next_pc = is_ecall                     ? mtvec_q
                  : is_mret                      ? mepc_q
                  : is_jalr                      ? jalr_target
                  : is_jal                       ? pc + imm_j
                  : (is_branch && branch_taken)  ? pc + imm_b
                  : pc_plus4;

  wire        lsu_wen  = is_sw || is_sh || is_sb;
  wire        lsu_ren  = is_load_any;

  wire [31:0] wdata = (is_jalr || is_jal)     ? pc_plus4
                     : is_load_any            ? lsu_rdata
                     : (is_csrrw || is_csrrs) ? csr_rdata   // old value, per csrrw/csrrs semantics
                     : alu_result;

  ifu u_ifu (clk, rst, next_pc, pc, inst);
  decoder u_dec (inst, opcode, funct3, funct7, rd, rs1, rs2, imm_i, imm_u, imm_s, imm_b, imm_j);

  RegisterFile #(4, 32) u_reg 
  (
    clk, wdata, rd, reg_wen,
    rs1, rs2,
    rs1_val, rs2_val, dbg_a0
  );

  alu u_alu (alu_a, alu_b, alu_op, alu_result);

  lsu u_lsu (alu_result, rs2_val, lsu_wen, lsu_ren, funct3, lsu_rdata);

  assign dbg_rd    = rd;
  assign dbg_wdata = wdata;
  assign dbg_mem_addr = alu_result;
  assign dbg_mem_wen  = lsu_wen;
  assign dbg_mem_ren  = lsu_ren;
  assign dbg_inst      = inst;
  assign dbg_mem_wdata = rs2_val;
  assign dbg_mstatus = mstatus_q;
  assign dbg_mtvec   = mtvec_q;
  assign dbg_mepc    = mepc_q;
  assign dbg_mcause  = mcause_q;

  always @(posedge clk) 
  begin
    if (is_ebreak) npc_trap();
  end
endmodule
