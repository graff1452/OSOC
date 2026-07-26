module clint
(
  input  clk,
  input  rst,
  input  [31:0] araddr,
  input         arvalid,
  output        arready,
  output [31:0] rdata,
  output [1:0]  rresp,
  output        rvalid,
  input         rready
);
  // Step 9: real CLINT, mtime only -- no mtimecmp/software-interrupt
  // support, since this design has no interrupt handling at all yet. The
  // handout itself narrows scope to "clock-related functions" for now.
  //
  // mtime increments by 1 every cycle -- the simplest implementation the
  // handout suggests, and a genuinely free-running 64-bit counter, no
  // gating beyond the usual rst.
  wire [63:0] mtime;
  wire [63:0] mtime_next = mtime + 64'd1;
  Reg #(64, 64'd0) r_mtime (clk, rst, mtime_next, mtime, 1'b1);

  // Same 2-state slave shape as every other slave in this design, but
  // unlike axi_mem.v there's no DPI-C call to wait on here -- mtime is
  // already a live register, available the instant it's asked for. Still
  // captured (not read live) at the AR handshake's edge, so the VALUE
  // returned is a stable snapshot from the moment the request was
  // accepted, not whatever mtime happens to be several cycles later once
  // the master finally asserts rready (which, under step 6's random
  // master-side delay, could genuinely be a few cycles away).
  wire state;
  wire ar_hs = arvalid && arready;
  wire r_hs  = rvalid  && rready;
  wire advance = state ? r_hs : ar_hs;

  Reg #(1, 1'b0) r_state (clk, rst, ~state, state, advance);

  assign arready = ~state;
  assign rvalid  = state;
  assign rresp   = 2'b00;

  // araddr[2]==0 selects RTC_ADDR (mtime's low 32 bits); araddr[2]==1
  // selects RTC_ADDR+4 (the high 32 bits) -- the two addresses the xbar
  // routes here differ by exactly 4, which is exactly bit 2.
  wire [31:0] word_now = araddr[2] ? mtime[63:32] : mtime[31:0];
  wire [31:0] word_captured;
  Reg #(32, 32'h00000000) r_word (clk, rst, word_now, word_captured, ar_hs);
  assign rdata = word_captured;
endmodule
