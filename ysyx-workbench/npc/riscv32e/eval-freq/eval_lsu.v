// Synthesis-only LSU variant for the B1 frequency evaluation -- identical
// load/store formatting logic to vsrc/lsu.v, but reads/writes a real
// synthesizable memory instead of DPI-C pmem_read()/pmem_write(). Needs a
// clk input (vsrc/lsu.v doesn't) because a real register array can only be
// written on a clock edge, unlike a same-cycle DPI-C call. Eval-only.
module EvalLSU
(
  input  clk,
  input  [31:0] addr,
  input  [31:0] wdata,
  input         wen,
  input         ren,
  input  [2:0]  size,
  output [31:0] rdata
);
  wire [1:0]  offset       = addr[1:0];
  wire [31:0] word;
  wire [31:0] byte_shifted = word >> (offset * 8);
  wire [31:0] half_shifted = word >> (offset[1] * 16);
  wire [7:0]  byte_val = byte_shifted[7:0];
  wire [15:0] half_val = half_shifted[15:0];

  wire [3:0]  wmask = (size == 3'b000) ? (4'b0001 << offset)
                     : (size == 3'b001) ? (4'b0011 << (offset[1] * 2))
                     : 4'b1111;
  wire [31:0] wdata_shifted = (size == 3'b000) ? (wdata << (offset * 8))
                             : (size == 3'b001) ? (wdata << (offset[1] * 16))
                             : wdata;

  EvalMemD u_dmem (clk, addr, wdata_shifted, wmask, wen, word);

  assign rdata = (size == 3'b000) ? {{24{byte_val[7]}},  byte_val}   // lb
               : (size == 3'b100) ? {24'b0,               byte_val}   // lbu
               : (size == 3'b001) ? {{16{half_val[15]}}, half_val}   // lh
               : (size == 3'b101) ? {16'b0,               half_val}   // lhu
               : word;                                                 // lw
endmodule
