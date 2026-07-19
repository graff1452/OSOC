module ifu
(
  input  clk,
  input  rst,
  input  [31:0] next_pc,
  output [31:0] pc,
  output reg [31:0] inst
);
  import "DPI-C" function int pmem_read(input int raddr);

  Reg #(32, 32'h80000000) r (clk, rst, next_pc, pc, 1'b1);

  always @(*) 
  begin
    inst = pmem_read(pc);
  end
endmodule
