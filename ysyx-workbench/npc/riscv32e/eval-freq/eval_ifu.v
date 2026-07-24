// Synthesis-only IFU variant for the B1 frequency evaluation -- identical pc
// register logic to vsrc/ifu.v, but reads from a real synthesizable memory
// instead of the DPI-C pmem_read() call (not synthesizable). Not used by
// rebuild_sim.sh -- eval-only, feeds yosys-sta.
module EvalIFU
(
  input  clk,
  input  rst,
  input  [31:0] next_pc,
  output [31:0] pc,
  output [31:0] inst
);
  Reg #(32, 32'h80000000) r (clk, rst, next_pc, pc, 1'b1);

  EvalMemI u_imem (pc, inst);
endmodule
