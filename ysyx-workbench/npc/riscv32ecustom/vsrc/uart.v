module uart
(
  input  clk,
  input  rst,
  input  [31:0] awaddr,
  input         awvalid,
  output        awready,
  input  [31:0] wdata,
  input  [3:0]  wstrb,
  input         wvalid,
  output        wready,
  output [1:0]  bresp,
  output        bvalid,
  input         bready
);
  // Step 9: real AXI4-Lite UART, replacing the DPI-C putchar()/fflush()
  // special-case in main.cpp's pmem_write for this exact address. AW+W are
  // sent together, same simplification axi_mem.v's write side already
  // uses (matches lsu.v's actual master behavior -- it always drives both
  // simultaneously).
  //
  // On a write, the low byte is printed directly with a plain Verilog
  // $write() -- no DPI-C needed here at all, unlike every other device
  // this project has touched so far. This is genuinely simpler, not a
  // shortcut: $write() is a standard Verilog system task, synthesizable
  // simulators just treat it as "not real hardware" the same way $display
  // already is throughout testbenches, and the handout explicitly
  // suggests it as the intended approach for this exact device.
  wire w_state;
  wire req_w   = awvalid && wvalid;
  wire new_req = req_w & ~w_state;
  wire b_hs    = bvalid && bready;

  // Same "hold until the response is actually accepted" discipline as
  // axi_mem.v's write side, not an unconditional 1-cycle pulse -- this
  // module needs to be a well-behaved AXI4-Lite slave under step 6's
  // random-delay testing too, same as every other slave in this design.
  Reg #(1, 1'b0) r_wstate (clk, rst, new_req | (w_state & ~b_hs), w_state, 1'b1);

  assign awready = ~w_state;
  assign wready  = ~w_state;
  assign bvalid  = w_state;
  assign bresp   = 2'b00;

  // wstrb[0] gates this the same way a real byte-addressable device would
  // only respond to a write that actually touches its one byte -- this
  // device only has a single 8-bit register, so only byte lane 0 matters.
  always @(posedge clk) begin
    if (new_req && wstrb[0]) begin
      $write("%c", wdata[7:0]);
    end
  end
endmodule
