module regfile
(
  input clk,
  input rst,
  input [1:0] raddr1,
  input [1:0] raddr2,
  output [7:0] rdata1,
  output [7:0] rdata2,
  output [7:0] r0_val,
  input [1:0] waddr,
  input [7:0] wdata,
  input wen
);

  wire [7:0] q0, q1, q2, q3;

  Reg #(8, 0) r0 (clk, rst, wdata, q0, wen & (waddr == 2'd0));
  Reg #(8, 0) r1 (clk, rst, wdata, q1, wen & (waddr == 2'd1));
  Reg #(8, 0) r2 (clk, rst, wdata, q2, wen & (waddr == 2'd2));
  Reg #(8, 0) r3 (clk, rst, wdata, q3, wen & (waddr == 2'd3));

  assign r0_val = q0;

  MuxKey #(4, 2, 8) mux1 (rdata1, raddr1, {
    2'd0, q0,
    2'd1, q1,
    2'd2, q2,
    2'd3, q3
  });

  MuxKey #(4, 2, 8) mux2 (rdata2, raddr2, {
    2'd0, q0,
    2'd1, q1,
    2'd2, q2,
    2'd3, q3
  });

endmodule
