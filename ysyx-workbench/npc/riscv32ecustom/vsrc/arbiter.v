module arbiter
(
  input  clk,
  input  rst,
  // From IFU (read-only master)
  input  [31:0] ifu_araddr,
  input         ifu_arvalid,
  output        ifu_arready,
  output [31:0] ifu_rdata,
  output [1:0]  ifu_rresp,
  output        ifu_rvalid,
  input         ifu_rready,
  // From LSU (read-write master)
  input  [31:0] lsu_araddr,
  input         lsu_arvalid,
  output        lsu_arready,
  output [31:0] lsu_rdata,
  output [1:0]  lsu_rresp,
  output        lsu_rvalid,
  input         lsu_rready,
  input  [31:0] lsu_awaddr,
  input         lsu_awvalid,
  output        lsu_awready,
  input  [31:0] lsu_wdata,
  input  [3:0]  lsu_wstrb,
  input         lsu_wvalid,
  output        lsu_wready,
  output [1:0]  lsu_bresp,
  output        lsu_bvalid,
  input         lsu_bready,
  // To the single shared memory (axi_mem.v)
  output [31:0] s_araddr,
  output        s_arvalid,
  input         s_arready,
  input  [31:0] s_rdata,
  input  [1:0]  s_rresp,
  input         s_rvalid,
  output        s_rready,
  output [31:0] s_awaddr,
  output        s_awvalid,
  input         s_awready,
  output [31:0] s_wdata,
  output [3:0]  s_wstrb,
  output        s_wvalid,
  input         s_wready,
  input  [1:0]  s_bresp,
  input         s_bvalid,
  output        s_bready
);
  // Step 7: arbiter between IFU and LSU for the single shared memory's AR/R
  // channels -- the only channels IFU ever uses, and the only ones two
  // masters could ever contend for. AW/W/B are LSU-only (IFU has no write
  // ports at all), so they're a pure passthrough below, not arbitration.
  //
  // Fixed priority, IFU always wins ties -- provably sufficient here, not
  // just "simple enough to not bother": IFU only ever issues a new AR once
  // it's back in FETCH, which only happens the cycle after an instruction
  // fully retires. LSU only ever asserts a request during that SAME
  // retiring instruction's EXEC phase, which is already over by the time
  // IFU's next AR appears. They are structurally incapable of requesting
  // on the same cycle -- this priority scheme exists for well-defined
  // behavior, not because real contention is expected.
  wire grant_ifu = ifu_arvalid;
  wire grant_lsu = lsu_arvalid & ~ifu_arvalid;

  assign s_araddr  = grant_ifu ? ifu_araddr : lsu_araddr;
  assign s_arvalid = grant_ifu | grant_lsu;

  assign ifu_arready = s_arready & grant_ifu;
  assign lsu_arready = s_arready & grant_lsu;

  wire ar_hs = s_arvalid & s_arready;   // a grant was actually accepted this cycle

  // owner: which master's request was most recently granted -- latched
  // only at the moment of an actual AR handshake, so it stays correct for
  // however many cycles the memory takes to respond afterward, regardless
  // of the random response delay from step 6.
  wire owner;
  Reg #(1, 1'b0) r_owner (clk, rst, grant_lsu, owner, ar_hs);

  assign ifu_rvalid = s_rvalid & ~owner;
  assign lsu_rvalid = s_rvalid &  owner;
  assign ifu_rdata  = s_rdata;
  assign lsu_rdata  = s_rdata;
  assign ifu_rresp  = s_rresp;
  assign lsu_rresp  = s_rresp;
  // Route rready from whichever master actually owns the outstanding
  // transaction -- this is what lets IFU's and LSU's own independent
  // master-side random delays (step 6b) keep working correctly even
  // through the arbiter, instead of being silently bypassed.
  assign s_rready = owner ? lsu_rready : ifu_rready;

  // Write channels: LSU-only, pure passthrough.
  assign s_awaddr    = lsu_awaddr;
  assign s_awvalid   = lsu_awvalid;
  assign lsu_awready = s_awready;
  assign s_wdata     = lsu_wdata;
  assign s_wstrb     = lsu_wstrb;
  assign s_wvalid    = lsu_wvalid;
  assign lsu_wready  = s_wready;
  assign lsu_bresp   = s_bresp;
  assign lsu_bvalid  = s_bvalid;
  assign s_bready    = lsu_bready;
endmodule
