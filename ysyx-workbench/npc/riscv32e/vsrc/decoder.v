module decoder
(
  input  [31:0] inst,
  output [6:0]  opcode,
  output [2:0]  funct3,
  output [6:0]  funct7,
  output [3:0]  rd,
  output [3:0]  rs1,
  output [3:0]  rs2,
  output [31:0] imm_i,
  output [31:0] imm_u,
  output [31:0] imm_s,
  output [31:0] imm_b,
  output [31:0] imm_j
);
  assign opcode = inst[6:0];
  assign funct3 = inst[14:12];
  assign funct7 = inst[31:25];
  assign rd     = inst[10:7];
  assign rs1    = inst[18:15];
  assign rs2    = inst[23:20];
  assign imm_i  = {{20{inst[31]}}, inst[31:20]};
  assign imm_u  = {inst[31:12], 12'b0};
  assign imm_s  = {{20{inst[31]}}, inst[31:25], inst[11:7]};
  // B-type: imm[12|11|10:5|4:1|0], bit 0 always 0 (branch targets are 2-byte aligned)
  assign imm_b  = {{19{inst[31]}}, inst[31], inst[7], inst[30:25], inst[11:8], 1'b0};
  // J-type: imm[20|10:1|11|19:12|0], bit 0 always 0, used only by jal
  assign imm_j  = {{11{inst[31]}}, inst[31], inst[19:12], inst[20], inst[30:21], 1'b0};
endmodule
