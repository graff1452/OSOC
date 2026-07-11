module scpu
(
  input clk,
  input rst,
  output [7:0] out_val,
  output out_valid
);

  wire [3:0] pc, next_pc, baddr;
  wire [7:0] inst;
  wire [1:0] opcode, rd, rs1, rs2;
  wire [3:0] imm;
  wire [7:0] r0_val, rs1_val, rs2_val, alu_result, wdata;
  wire wen;

  wire [1:0] raddr1_sel = (opcode == 2'b01) ? rd : rs1;

  wire branch_taken = (opcode == 2'b11) && (r0_val != rs2_val);
  assign next_pc = branch_taken ? baddr : (pc + 4'd1);

  assign wen = (opcode == 2'b00) || (opcode == 2'b10);
  assign wdata = (opcode == 2'b10) ? {4'b0000, imm} : alu_result;

  assign out_valid = (opcode == 2'b01);
  assign out_val = rs1_val;

  pc_reg  u_pc  (clk, rst, next_pc, pc);
  imem    u_imem(pc, inst);
  decoder u_dec (inst, opcode, rd, rs1, rs2, imm, baddr);
  regfile u_reg (clk, rst, raddr1_sel, rs2, rs1_val, rs2_val, r0_val, rd, wdata, wen);
  alu     u_alu (rs1_val, rs2_val, alu_result);

endmodule
