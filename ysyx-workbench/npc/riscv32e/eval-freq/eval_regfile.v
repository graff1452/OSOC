// Synthesis-only register file for the B1 frequency evaluation -- identical
// read/write logic to vsrc/regfile.v, minus the `export "DPI-C"` debug hook
// (not synthesizable, and irrelevant to a frequency measurement anyway).
module EvalRegisterFile #(ADDR_WIDTH = 4, DATA_WIDTH = 32) (
  input clk,
  input [DATA_WIDTH-1:0] wdata,
  input [ADDR_WIDTH-1:0] waddr,
  input wen,
  input [ADDR_WIDTH-1:0] raddr1,
  input [ADDR_WIDTH-1:0] raddr2,
  output [DATA_WIDTH-1:0] rdata1,
  output [DATA_WIDTH-1:0] rdata2
);
  reg [DATA_WIDTH-1:0] rf [2**ADDR_WIDTH-1:0];
  always @(posedge clk) begin
    if (wen) rf[waddr] <= wdata;
  end

  assign rdata1 = (raddr1 == 0) ? {DATA_WIDTH{1'b0}} : rf[raddr1];
  assign rdata2 = (raddr2 == 0) ? {DATA_WIDTH{1'b0}} : rf[raddr2];
endmodule
