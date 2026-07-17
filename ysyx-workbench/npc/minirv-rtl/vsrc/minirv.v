module minirv
(
  input  clk,
  input  rst,
  output [31:0] pc,
  output [3:0]  dbg_rd,
  output [31:0] dbg_wdata,
  output [31:0] dbg_a0
);

  import "DPI-C" function void npc_trap();

  wire [31:0] next_pc, imm_i, imm_u, imm_s, rs1_val, rs2_val, alu_result, inst, lsu_rdata;
  wire [6:0]  opcode;
  wire [2:0]  funct3;
  wire [6:0]  funct7;
  wire [3:0]  rd, rs1, rs2;

  wire [31:0] pc_plus4    = pc + 32'd4;
  wire        is_jalr     = (opcode == 7'b1100111);
  wire        is_add      = (opcode == 7'b0110011) && (funct3 == 3'b000);
  wire        is_lui      = (opcode == 7'b0110111);
  wire        is_ebreak   = (inst == 32'h00100073);
  wire        is_load     = (opcode == 7'b0000011);
  wire        is_lw       = is_load && (funct3 == 3'b010);
  wire        is_lbu      = is_load && (funct3 == 3'b100);
  wire        is_store    = (opcode == 7'b0100011);
  wire        is_sw       = is_store && (funct3 == 3'b010);
  wire        is_sb       = is_store && (funct3 == 3'b000);
  wire [31:0] jalr_target = (rs1_val + imm_i) & ~32'd1;
  wire        reg_wen     = !(is_sw || is_sb);

  wire [31:0] alu_a = is_lui ? 32'b0 : rs1_val;
  wire [31:0] alu_b = is_add ? rs2_val
                    : is_lui ? imm_u
                    : is_store ? imm_s
                    : imm_i;   // covers addi, jalr, lw, lbu

  assign next_pc = is_jalr ? jalr_target : pc_plus4;

  wire        lsu_wen  = is_sw || is_sb;
  wire        lsu_ren  = is_lw || is_lbu;
  wire [1:0]  lsu_size = (is_sb || is_lbu) ? 2'd0 : 2'd2;

  wire [31:0] wdata = is_jalr           ? pc_plus4
                     : (is_lw || is_lbu) ? lsu_rdata
                     : alu_result;

  ifu u_ifu (clk, rst, next_pc, pc, inst);
  decoder u_dec (inst, opcode, funct3, funct7, rd, rs1, rs2, imm_i, imm_u, imm_s);

  RegisterFile #(4, 32) u_reg 
  (
    clk, wdata, rd, reg_wen,
    rs1, rs2,
    rs1_val, rs2_val, dbg_a0
  );

  alu u_alu (alu_a, alu_b, alu_result);

  lsu u_lsu (alu_result, rs2_val, lsu_wen, lsu_ren, lsu_size, lsu_rdata);

  assign dbg_rd    = rd;
  assign dbg_wdata = wdata;

  always @(posedge clk) 
  begin
    if (is_ebreak) npc_trap();
  end
endmodule
