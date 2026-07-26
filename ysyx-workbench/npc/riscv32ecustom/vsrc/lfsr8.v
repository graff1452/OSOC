module lfsr8 #(parameter SEED = 8'hA5) (
  input  clk,
  input  rst,
  output [7:0] value
);
  // 8-bit Fibonacci LFSR, standard maximal-length tap set (x^8+x^6+x^5+x^4+1,
  // taps at bits 7,5,4,3 zero-indexed). Free-running: shifts every single
  // cycle unconditionally, regardless of anything else happening in the
  // design -- so whichever cycle a consumer happens to sample `value` on,
  // they get a pseudo-random byte with no fixed relationship to their own
  // request timing. SEED must never be 0 (an all-zero LFSR state feeds back
  // 0 forever and never moves); the two consumers below use different SEEDs
  // so their sampled delays aren't identical every single transaction.
  wire feedback = value[7] ^ value[5] ^ value[4] ^ value[3];
  wire [7:0] next = {value[6:0], feedback};
  Reg #(8, SEED) r_lfsr (clk, rst, next, value, 1'b1);
endmodule
