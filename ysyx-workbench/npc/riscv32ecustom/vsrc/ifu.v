module ifu
(
  input  clk,
  input  rst,
  input  [31:0] next_pc,
  input         exec_done,
  output [31:0] pc,
  output [31:0] inst,
  output        valid,
  // AXI4-Lite read-only master interface (to axi_mem_i.v)
  output [31:0] m_araddr,
  output        m_arvalid,
  input         m_arready,
  input  [31:0] m_rdata,
  input  [1:0]  m_rresp,     // unused for now -- always OKAY, no decode-fault
                              // handling yet (future work, per the handout's
                              // own "error handling and exceptions" section)
  input         m_rvalid,
  output        m_rready
);
  // Step 5: AXI4-Lite master, 3 states:
  //   S_AR   -- send the fetch address, wait for the slave to accept it
  //   S_R    -- address accepted, wait for the slave's data to arrive
  //   S_EXEC -- decode/execute this instruction, held until exec_done
  // valid = (state==S_EXEC) is EXACTLY the same external meaning if_valid
  // has had since step 2 -- minirv.v/main.cpp need no changes for this step,
  // only HOW the instruction gets into `inst` changed (2 real bus
  // handshakes instead of 1 assumed-instant DPI-C read).
  localparam S_AR = 2'd0, S_R = 2'd1, S_EXEC = 2'd2;

  wire [1:0] state;
  wire       ar_hs = m_arvalid && m_arready;   // AR channel handshake this cycle
  wire       r_hs  = m_rvalid  && m_rready;    // R channel handshake this cycle

  wire [1:0] state_next = (state == S_AR) ? (ar_hs ? S_R    : S_AR)
                         : (state == S_R) ? (r_hs  ? S_EXEC : S_R)
                         :                   (exec_done ? S_AR : S_EXEC);  // S_EXEC

  Reg #(2, S_AR) r_state (clk, rst, state_next, state, 1'b1);

  assign valid    = (state == S_EXEC);
  assign m_araddr = pc;

  // Step 6b: master-side random delay. This master doesn't have to be in a
  // hurry -- delaying WHEN it offers arvalid/rready (rather than always
  // offering the instant it's able to) is exactly as valid an AXI master as
  // one that's always eager, and it's the other half of what the handout
  // calls for testing (the slave-side response delay from step 6a already
  // covers "the memory takes its time"; this covers "the CPU takes its
  // time asking"). Same wait-counter idiom the slaves already use: load a
  // fresh random 0-3 the first cycle we WANT to assert something, count
  // down, only actually assert once it reaches 0.
  wire [7:0] rnd_ar;
  lfsr8 #(8'hC3) u_lfsr_ar (clk, rst, rnd_ar);
  wire [7:0] rnd_r;
  lfsr8 #(8'h3C) u_lfsr_r (clk, rst, rnd_r);

  wire in_ar = (state == S_AR);
  wire ar_active;
  wire ar_new = in_ar & ~ar_active;   // first cycle of this S_AR visit
  wire [1:0] ar_wait_left;
  wire [1:0] ar_wait_next = ar_new ? rnd_ar[1:0] : (ar_wait_left != 2'd0 ? ar_wait_left - 2'd1 : ar_wait_left);
  Reg #(2, 2'd0) r_arwait   (clk, rst, ar_wait_next, ar_wait_left, 1'b1);
  Reg #(1, 1'b0) r_aractive (clk, rst, in_ar, ar_active, 1'b1);
  // ~ar_new guards against reading a not-yet-loaded ar_wait_left on the
  // very cycle it's being freshly sampled -- same reasoning as every
  // response-delay counter in axi_mem_i.v/axi_mem_d.v.
  assign m_arvalid = in_ar && ~ar_new && (ar_wait_left == 2'd0);

  wire in_r = (state == S_R);
  wire r_active;
  wire r_new = in_r & ~r_active;
  wire [1:0] r_wait_left;
  wire [1:0] r_wait_next = r_new ? rnd_r[1:0] : (r_wait_left != 2'd0 ? r_wait_left - 2'd1 : r_wait_left);
  Reg #(2, 2'd0) r_rwait   (clk, rst, r_wait_next, r_wait_left, 1'b1);
  Reg #(1, 1'b0) r_ractive (clk, rst, in_r, r_active, 1'b1);
  assign m_rready = in_r && ~r_new && (r_wait_left == 2'd0);

  // pc advances on the one true "leaving EXEC" transition -- same condition
  // step 4 used (`valid & exec_done`), just written against the new state.
  wire pc_wen = (state == S_EXEC) && exec_done;
  Reg #(32, 32'h80000000) r_pc (clk, rst, next_pc, pc, pc_wen);

  // inst is captured at the R handshake's edge, not read combinationally --
  // per the AXI spec, m_rdata is only guaranteed valid the cycle
  // m_rvalid && m_rready both assert. Holds steady through however long
  // EXEC subsequently lasts, same as every step before this one.
  Reg #(32, 32'h00000013) r_inst (clk, rst, m_rdata, inst, r_hs);
endmodule
