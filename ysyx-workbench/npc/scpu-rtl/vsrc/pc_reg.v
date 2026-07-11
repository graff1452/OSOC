module pc_reg
(
  input clk,
  input rst,
  input [3:0] next_pc,
  output [3:0] pc
);
  Reg #(4, 0) r (clk, rst, next_pc, pc, 1'b1);
endmodule
