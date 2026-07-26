module lsu
(
  input  clk,
  input  rst,
  input  [31:0] addr,
  input  [31:0] wdata,
  input         wen,
  input         ren,
  input  [2:0]  size,      // matches funct3 directly: 000=byte(signed) 001=half(signed)
                            // 010=word 100=byte(unsigned) 101=half(unsigned)
  output [31:0] rdata,
  output        valid,
  // AXI4-Lite read-write master interface (to axi_mem_d.v)
  output [31:0] m_araddr,
  output        m_arvalid,
  input         m_arready,
  input  [31:0] m_rdata,
  input  [1:0]  m_rresp,
  input         m_rvalid,
  output        m_rready,
  output [31:0] m_awaddr,
  output        m_awvalid,
  input         m_awready,
  output [31:0] m_wdata,
  output [3:0]  m_wstrb,
  output        m_wvalid,
  input         m_wready,
  input  [1:0]  m_bresp,
  input         m_bvalid,
  output        m_bready
);
  // Step 5: AXI4-Lite read-write master. External wen/ren -> valid interface
  // is byte-for-byte the same contract minirv.v has used since step 4 -- only
  // the internal implementation changed. States:
  //   S_IDLE          -- watching wen/ren
  //   S_AR  / S_R      -- read path (load): address, then data
  //   S_AWW / S_B       -- write path (store): AW+W sent TOGETHER on the same
  //                        cycle (this master always drives both at once --
  //                        axi_mem_d.v's write side is deliberately built
  //                        around that same assumption, see its comments),
  //                        then wait for the B response
  //   S_DONE          -- 1-cycle valid pulse, same external timing shape
  //                        step 4's lsu_valid already had
  localparam S_IDLE = 3'd0, S_AR = 3'd1, S_R = 3'd2,
             S_AWW  = 3'd3, S_B  = 3'd4, S_DONE = 3'd5;

  wire [2:0] state;
  wire ar_hs  = m_arvalid && m_arready;
  wire r_hs   = m_rvalid  && m_rready;
  wire aww_hs = m_awvalid && m_awready && m_wvalid && m_wready;
  wire b_hs   = m_bvalid  && m_bready;

  wire [2:0] state_next =
      (state == S_IDLE) ? (ren ? S_AR : wen ? S_AWW : S_IDLE)
    : (state == S_AR)   ? (ar_hs  ? S_R    : S_AR)
    : (state == S_R)    ? (r_hs   ? S_DONE : S_R)
    : (state == S_AWW)  ? (aww_hs ? S_B    : S_AWW)
    : (state == S_B)    ? (b_hs   ? S_DONE : S_B)
    :                      S_IDLE;   // S_DONE always falls straight back to idle

  Reg #(3, S_IDLE) r_state (clk, rst, state_next, state, 1'b1);

  assign valid = (state == S_DONE);

  // Step 6b: master-side random delay on all 4 request-side signals this
  // master drives (m_arvalid, m_rready, m_awvalid+m_wvalid together since
  // they're always sent as a pair, m_bready) -- same wait-counter idiom
  // ifu.v uses, applied 4 times since lsu.v has 4 independent channels
  // instead of ifu.v's 2. Each gets its own LFSR seed so a load and a
  // store happening around the same time in the surrounding design don't
  // get suspiciously correlated delays.
  wire [7:0] rnd_ar, rnd_r, rnd_aww, rnd_b;
  lfsr8 #(8'h1F) u_lfsr_ar  (clk, rst, rnd_ar);
  lfsr8 #(8'hF1) u_lfsr_r   (clk, rst, rnd_r);
  lfsr8 #(8'h2E) u_lfsr_aww (clk, rst, rnd_aww);
  lfsr8 #(8'hE2) u_lfsr_b   (clk, rst, rnd_b);

  // ---- Read path (AR/R) ----
  assign m_araddr = addr;

  wire in_ar = (state == S_AR);
  wire ar_active;
  wire ar_new = in_ar & ~ar_active;
  wire [1:0] ar_wait_left;
  wire [1:0] ar_wait_next = ar_new ? rnd_ar[1:0] : (ar_wait_left != 2'd0 ? ar_wait_left - 2'd1 : ar_wait_left);
  Reg #(2, 2'd0) r_arwait   (clk, rst, ar_wait_next, ar_wait_left, 1'b1);
  Reg #(1, 1'b0) r_aractive (clk, rst, in_ar, ar_active, 1'b1);
  assign m_arvalid = in_ar && ~ar_new && (ar_wait_left == 2'd0);

  wire in_r = (state == S_R);
  wire r_active;
  wire r_new = in_r & ~r_active;
  wire [1:0] r_wait_left;
  wire [1:0] r_wait_next = r_new ? rnd_r[1:0] : (r_wait_left != 2'd0 ? r_wait_left - 2'd1 : r_wait_left);
  Reg #(2, 2'd0) r_rwait   (clk, rst, r_wait_next, r_wait_left, 1'b1);
  Reg #(1, 1'b0) r_ractive (clk, rst, in_r, r_active, 1'b1);
  assign m_rready = in_r && ~r_new && (r_wait_left == 2'd0);

  // Captured at the R handshake's edge, same reasoning as ifu.v: m_rdata is
  // only guaranteed valid the cycle m_rvalid && m_rready both assert.
  wire [31:0] word_captured;
  Reg #(32, 32'h00000000) r_word (clk, rst, m_rdata, word_captured, r_hs);

  // Byte/half extraction + sign-extension: unchanged logic from every prior
  // step, just now operating on the captured word instead of a live DPI-C
  // read. This is deliberately ISA-level interpretation living in the
  // MASTER (lsu.v) -- the slave (axi_mem_d.v) only ever hands back a raw
  // 32-bit word, exactly like a real memory chip would; it has no concept
  // of signed/unsigned or byte/half loads.
  wire [1:0]  offset       = addr[1:0];
  wire [31:0] byte_shifted = word_captured >> (offset * 8);
  wire [31:0] half_shifted = word_captured >> (offset[1] * 16);
  wire [7:0]  byte_val = byte_shifted[7:0];
  wire [15:0] half_val = half_shifted[15:0];

  assign rdata = (size == 3'b000) ? {{24{byte_val[7]}},  byte_val}   // lb  (sign-extended)
               : (size == 3'b100) ? {24'b0,               byte_val}   // lbu (zero-extended)
               : (size == 3'b001) ? {{16{half_val[15]}}, half_val}   // lh  (sign-extended)
               : (size == 3'b101) ? {16'b0,               half_val}   // lhu (zero-extended)
               : word_captured;                                       // lw

  // ---- Write path (AW+W/B) ----
  assign m_awaddr = addr;

  wire in_aww = (state == S_AWW);
  wire aww_active;
  wire aww_new = in_aww & ~aww_active;
  wire [1:0] aww_wait_left;
  wire [1:0] aww_wait_next = aww_new ? rnd_aww[1:0] : (aww_wait_left != 2'd0 ? aww_wait_left - 2'd1 : aww_wait_left);
  Reg #(2, 2'd0) r_awwwait   (clk, rst, aww_wait_next, aww_wait_left, 1'b1);
  Reg #(1, 1'b0) r_awwactive (clk, rst, in_aww, aww_active, 1'b1);
  // AW and W are still sent together, on the SAME delayed cycle -- one
  // counter gates both, since they're one logical "offer" from this master.
  wire aww_go = in_aww && ~aww_new && (aww_wait_left == 2'd0);
  assign m_awvalid = aww_go;
  assign m_wvalid  = aww_go;

  wire in_b = (state == S_B);
  wire b_active;
  wire b_new = in_b & ~b_active;
  wire [1:0] b_wait_left;
  wire [1:0] b_wait_next = b_new ? rnd_b[1:0] : (b_wait_left != 2'd0 ? b_wait_left - 2'd1 : b_wait_left);
  Reg #(2, 2'd0) r_bwait   (clk, rst, b_wait_next, b_wait_left, 1'b1);
  Reg #(1, 1'b0) r_bactive (clk, rst, in_b, b_active, 1'b1);
  assign m_bready = in_b && ~b_new && (b_wait_left == 2'd0);

  // Same byte-lane shifting every prior version of lsu.v used, now feeding a
  // real 4-bit wstrb (one bit per byte lane -- standard AXI width for a
  // 32-bit data bus, literally named wstrb[3:0] in the handout's own
  // diagram) instead of the old ad-hoc 8-bit mask.
  assign m_wdata = (size == 3'b000) ? (wdata << (offset * 8))          // sb
                  : (size == 3'b001) ? (wdata << (offset[1] * 16))     // sh
                  : wdata;                                             // sw
  assign m_wstrb = (size == 3'b000) ? (4'b0001 << offset)              // sb
                  : (size == 3'b001) ? (4'b0011 << (offset[1] * 2))    // sh
                  : 4'b1111;                                           // sw
endmodule
