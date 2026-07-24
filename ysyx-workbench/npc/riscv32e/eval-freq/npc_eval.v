// Synthesis-only top module for the B1 frequency evaluation -- the actual
// minirv.v datapath (every instruction/CSR/branch decision unchanged),
// minus the DPI-C npc_trap() hook and dbg_* debug ports (neither
// synthesizable nor relevant to a frequency measurement), wired to the
// Eval* memory/regfile modules above instead of the DPI-C-backed ones.
// Module name matches `DESIGN=npc_eval` for yosys-sta's `synth -top`.
module npc_eval
(
  input  clk,
  input  rst,
  output [31:0] pc
);

  wire [31:0] next_pc, imm_i, imm_u, imm_s, imm_b, imm_j, rs1_val, rs2_val, alu_result, inst, lsu_rdata;
  wire [6:0]  opcode;
  wire [2:0]  funct3;
  wire [6:0]  funct7;
  wire [3:0]  rd, rs1, rs2;

  wire [31:0] pc_plus4    = pc + 32'd4;
  wire        is_jalr     = (opcode == 7'b1100111);
  wire        is_jal      = (opcode == 7'b1101111);
  wire        is_rtype    = (opcode == 7'b0110011);
  wire        is_itype    = (opcode == 7'b0010011);
  wire        is_lui      = (opcode == 7'b0110111);
  wire        is_auipc    = (opcode == 7'b0010111);
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

  wire        is_ecall = (inst == 32'h00000073);
  wire        is_mret  = (inst == 32'h30200073);
  wire        is_csrrw = (opcode == 7'b1110011) && (funct3 == 3'b001);
  wire        is_csrrs = (opcode == 7'b1110011) && (funct3 == 3'b010);
  wire [11:0] csr_addr = inst[31:20];

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

  wire        is_addi  = is_itype && (funct3 == 3'b000);
  wire        is_slli  = is_itype && (funct3 == 3'b001);
  wire        is_slti  = is_itype && (funct3 == 3'b010);
  wire        is_sltiu = is_itype && (funct3 == 3'b011);
  wire        is_xori  = is_itype && (funct3 == 3'b100);
  wire        is_srli  = is_itype && (funct3 == 3'b101) && (funct7 == 7'b0000000);
  wire        is_srai  = is_itype && (funct3 == 3'b101) && (funct7 == 7'b0100000);
  wire        is_ori   = is_itype && (funct3 == 3'b110);
  wire        is_andi  = is_itype && (funct3 == 3'b111);

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
                     : imm_i;

  wire [3:0] alu_op = (is_sub)                ? 4'd1
                    : (is_and  || is_andi)    ? 4'd2
                    : (is_or   || is_ori)     ? 4'd3
                    : (is_xor  || is_xori)    ? 4'd4
                    : (is_sll  || is_slli)    ? 4'd5
                    : (is_srl  || is_srli)    ? 4'd6
                    : (is_sra  || is_srai)    ? 4'd7
                    : (is_slt  || is_slti)    ? 4'd8
                    : (is_sltu || is_sltiu)   ? 4'd9
                    : 4'd0;

  wire [31:0] mstatus_q, mtvec_q, mepc_q, mcause_q;
  wire [31:0] csr_rdata = (csr_addr == 12'h300) ? mstatus_q
                         : (csr_addr == 12'h305) ? mtvec_q
                         : (csr_addr == 12'h341) ? mepc_q
                         : (csr_addr == 12'h342) ? mcause_q
                         : 32'b0;
  wire [31:0] csr_wdata = is_csrrw ? rs1_val : (csr_rdata | rs1_val);

  wire wen_mstatus = (is_csrrw || is_csrrs) && (csr_addr == 12'h300);
  wire wen_mtvec   = (is_csrrw || is_csrrs) && (csr_addr == 12'h305);
  wire wen_mepc    = is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h341));
  wire wen_mcause  = is_ecall || ((is_csrrw || is_csrrs) && (csr_addr == 12'h342));

  wire [31:0] mepc_din   = is_ecall ? pc     : csr_wdata;
  wire [31:0] mcause_din = is_ecall ? 32'd11 : csr_wdata;

  Reg #(32, 32'h00001800) u_mstatus (clk, rst, csr_wdata,  mstatus_q, wen_mstatus);
  Reg #(32, 32'h00000000) u_mtvec   (clk, rst, csr_wdata,  mtvec_q,   wen_mtvec);
  Reg #(32, 32'h00000000) u_mepc    (clk, rst, mepc_din,   mepc_q,    wen_mepc);
  Reg #(32, 32'h00000000) u_mcause  (clk, rst, mcause_din, mcause_q,  wen_mcause);

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
                     : (is_csrrw || is_csrrs) ? csr_rdata
                     : alu_result;

  EvalIFU u_ifu (clk, rst, next_pc, pc, inst);
  decoder u_dec (inst, opcode, funct3, funct7, rd, rs1, rs2, imm_i, imm_u, imm_s, imm_b, imm_j);

  EvalRegisterFile #(4, 32) u_reg
  (
    clk, wdata, rd, reg_wen,
    rs1, rs2,
    rs1_val, rs2_val
  );

  alu u_alu (alu_a, alu_b, alu_op, alu_result);

  EvalLSU u_lsu (clk, alu_result, rs2_val, lsu_wen, lsu_ren, funct3, lsu_rdata);

endmodule
