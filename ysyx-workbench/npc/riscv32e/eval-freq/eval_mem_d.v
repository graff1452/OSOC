// Synthesis-only data memory for the B1 frequency evaluation. Same 256x32
// (1KB) sizing as eval_mem_i.v, but with a real synchronous write port
// (one enable bit per byte lane) so the LSU's store path stays real logic
// instead of being optimized away like the read-only instruction memory.
module EvalMemD #(
  parameter DEPTH = 256
)(
  input         clk,
  input  [31:0] addr,
  input  [31:0] wdata,
  input  [3:0]  wmask,   // one bit per byte lane
  input         wen,
  output [31:0] rdata
);
  localparam AW = $clog2(DEPTH);

  reg [31:0] mem [0:DEPTH-1];
  wire [AW-1:0] windex = addr[AW+1:2];

  always @(posedge clk) begin
    if (wen) begin
      if (wmask[0]) mem[windex][7:0]   <= wdata[7:0];
      if (wmask[1]) mem[windex][15:8]  <= wdata[15:8];
      if (wmask[2]) mem[windex][23:16] <= wdata[23:16];
      if (wmask[3]) mem[windex][31:24] <= wdata[31:24];
    end
  end

  assign rdata = mem[windex];
endmodule
