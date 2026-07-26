module axi_mem_d
(
  input         clk,
  input         rst,
  // AR/R (read)
  input  [31:0] araddr,
  input         arvalid,
  output        arready,
  output [31:0] rdata,
  output [1:0]  rresp,
  output        rvalid,
  input         rready,
  // AW/W/B (write)
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
  import "DPI-C" function int  pmem_read(input int raddr);
  import "DPI-C" function void pmem_write(input int waddr, input int wdata, input byte wmask);

  // Step 6: two independent random delays (different LFSR seeds, so a
  // load and a store happening around the same time don't get suspiciously
  // identical delays) -- one for the read response, one for the write
  // response. Same reasoning as axi_mem_i.v: confirms lsu.v's S_R/S_B states
  // genuinely wait for rvalid/bvalid rather than assuming a fixed latency.
  wire [7:0] rnd_r;
  lfsr8 #(8'hA5) u_lfsr_r (clk, rst, rnd_r);
  wire [7:0] rnd_w;
  lfsr8 #(8'h5A) u_lfsr_w (clk, rst, rnd_w);

  // ---- Read side: identical shape to axi_mem_i.v ----
  wire r_state;
  wire ar_hs = arvalid && arready;
  wire r_hs  = rvalid  && rready;
  wire r_advance = r_state ? r_hs : ar_hs;

  wire [1:0] r_wait_left;
  wire [1:0] r_wait_next = ar_hs ? rnd_r[1:0] : (r_wait_left != 2'd0 ? r_wait_left - 2'd1 : r_wait_left);
  Reg #(2, 2'd0) r_rwait (clk, rst, r_wait_next, r_wait_left, 1'b1);

  Reg #(1, 1'b0) r_rstate (clk, rst, ~r_state, r_state, r_advance);

  assign arready = ~r_state;
  assign rvalid  = r_state && (r_wait_left == 2'd0);
  assign rresp   = 2'b00;

  wire [31:0] word_now = pmem_read(araddr);
  wire [31:0] word_captured;
  Reg #(32, 32'h00000000) r_word (clk, rst, word_now, word_captured, ar_hs);
  assign rdata = word_captured;

  // ---- Write side ----
  // Simplification, explicitly noted: this slave requires AW and W to
  // arrive on the SAME cycle (awvalid && wvalid together), rather than
  // tracking each channel's completion fully independently. That's correct
  // for this design because lsu.v's master always drives both simultaneously
  // (its S_AWW state) -- a fully general AXI4-Lite slave would track AW and
  // W separately, since the spec technically permits them to arrive on
  // different cycles, but no master this slave will ever actually talk to
  // does that, so that generality would add real complexity for zero benefit
  // here.
  wire w_state;
  wire req_w   = awvalid && wvalid;
  wire new_req = req_w & ~w_state;
  wire b_hs    = bvalid && bready;

  wire [1:0] w_wait_left;
  wire [1:0] w_wait_next = new_req ? rnd_w[1:0] : (w_wait_left != 2'd0 ? w_wait_left - 2'd1 : w_wait_left);
  Reg #(2, 2'd0) r_wwait (clk, rst, w_wait_next, w_wait_left, 1'b1);

  // w_state only clears once b_hs actually fires (`& ~b_hs`), not
  // unconditionally after 1 cycle -- this is what makes bvalid correctly
  // hold if bready is ever delayed. As of step 6 this is no longer a
  // no-op precaution -- it's actively exercised, since bvalid now
  // genuinely doesn't assert for 1-4 cycles after new_req.
  Reg #(1, 1'b0) r_wstate (clk, rst, new_req | (w_state & ~b_hs), w_state, 1'b1);

  assign awready = ~w_state;
  assign wready  = ~w_state;
  assign bvalid  = w_state && (w_wait_left == 2'd0);
  assign bresp   = 2'b00;

  // Edge-triggered, single-shot on new_req -- same established pattern as
  // every other DPI-C side-effect call in this design (minirv.v's
  // ebreak->npc_trap(), the pre-AXI lsu.v's own pmem_write()).
  always @(posedge clk) begin
    if (new_req) begin
      pmem_write(awaddr, wdata, {4'b0000, wstrb});
    end
  end
endmodule
