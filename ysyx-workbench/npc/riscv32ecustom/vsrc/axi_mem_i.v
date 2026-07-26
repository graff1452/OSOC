module axi_mem_i
(
  input         clk,
  input         rst,
  input  [31:0] araddr,
  input         arvalid,
  output        arready,
  output [31:0] rdata,
  output [1:0]  rresp,
  output        rvalid,
  input         rready
);
  import "DPI-C" function int pmem_read(input int raddr);

  // Step 6: random response delay -- models a memory that doesn't always
  // answer at a fixed latency, so we can confirm ifu.v's S_R state genuinely
  // WAITS for rvalid rather than just assuming it shows up after exactly
  // 1 cycle (which is all it's ever been tested against so far).
  wire [7:0] rnd;
  lfsr8 #(8'hA5) u_lfsr (clk, rst, rnd);

  // 2-state slave: s_idle (0, accepting a new address) / s_resp (1, waiting
  // out the random delay, then rvalid). Same capture-then-hold shape used
  // throughout this whole design since step 2, now with an EXTRA random
  // wait tacked onto the resp phase instead of asserting rvalid the instant
  // we enter it.
  wire state;
  wire ar_hs = arvalid && arready;
  wire r_hs  = rvalid  && rready;

  // wait_left: loaded with a fresh random 0-3 the cycle the AR handshake
  // fires (ar_hs), then counts down once per cycle while in s_resp. rvalid
  // only asserts once BOTH state==s_resp AND wait_left has reached 0 --
  // during the ar_hs cycle itself state is still 0, so rvalid can't
  // accidentally fire off a stale/not-yet-loaded wait_left value.
  wire [1:0] wait_left;
  wire [1:0] wait_next = ar_hs ? rnd[1:0] : (wait_left != 2'd0 ? wait_left - 2'd1 : wait_left);
  Reg #(2, 2'd0) r_wait (clk, rst, wait_next, wait_left, 1'b1);

  wire advance = state ? r_hs : ar_hs;

  Reg #(1, 1'b0) r_state (clk, rst, ~state, state, advance);

  assign arready = ~state;   // only accept a new address while idle
  assign rvalid  = state && (wait_left == 2'd0);
  assign rresp   = 2'b00;    // OKAY -- no error/decode-fault detection yet

  // Captured, not live: during s_idle this still holds the PREVIOUS access's
  // data; it only updates, at the AR handshake's edge, to THIS access's real
  // data -- visible starting the cycle rvalid goes high.
  wire [31:0] word_now = pmem_read(araddr);
  wire [31:0] word_captured;
  Reg #(32, 32'h00000000) r_word (clk, rst, word_now, word_captured, ar_hs);
  assign rdata = word_captured;
endmodule
