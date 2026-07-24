// Synthesis-only instruction memory for the B1 frequency evaluation.
// 256 x 32-bit (1KB), read-only. Preloaded with the same 20 hand-picked,
// Python-verified RV32E instruction encodings as before, but via
// generate/genvar + one `initial` statement per array element -- NOT a
// MuxKeyWithDefault linear-scan lookup, and NOT a single `initial` block
// with a runtime for-loop index.
//
// History of why this file has been wrong twice already, for the record:
//   1) `initial` block + runtime `for` loop, indexing `mem[i]` with a
//      variable `i`: Yosys's `proc` pass read this as "written during
//      operation" and created 256 real (but unclocked, hence dead) write
//      ports, silently discarding all ROM content.
//   2) MuxKeyWithDefault (templates.v) with NR_KEY=256: correctly gave real,
//      non-constant ROM content this time, but its linear-scan structure
//      (256 individual equality comparators) triggered yosys.tcl's
//      `share -aggressive` pass into an effectively-unbounded number of
//      pairwise SAT-solver checks across the whole design -- technically
//      making progress, not hung, but impractically slow.
// generate/genvar sidesteps both: each loop iteration is fully expanded at
// *elaboration time* into a separate, fixed-index `initial mem[N] = ...;`
// statement (not a runtime write, and not a comparator), which Yosys
// recognizes as real MEMINIT content and maps to an efficient
// binary-addressed lookup.
module EvalMemI #(
  parameter DEPTH = 256
)(
  input  [31:0] addr,
  output [31:0] rdata
);
  localparam AW = $clog2(DEPTH);   // 8 bits for DEPTH=256

  localparam [31:0] W0  = {12'd5,                       5'd1, 3'b000, 5'd3, 7'b0010011}; // addi x3,x1,5
  localparam [31:0] W1  = {7'b0000000, 5'd2,             5'd1, 3'b000, 5'd3, 7'b0110011}; // add  x3,x1,x2
  localparam [31:0] W2  = {7'b0100000, 5'd2,             5'd1, 3'b000, 5'd3, 7'b0110011}; // sub  x3,x1,x2
  localparam [31:0] W3  = {7'b0000000, 5'd2,             5'd1, 3'b111, 5'd3, 7'b0110011}; // and  x3,x1,x2
  localparam [31:0] W4  = {7'b0000000, 5'd2,             5'd1, 3'b110, 5'd3, 7'b0110011}; // or   x3,x1,x2
  localparam [31:0] W5  = {7'b0000000, 5'd2,             5'd1, 3'b100, 5'd3, 7'b0110011}; // xor  x3,x1,x2
  localparam [31:0] W6  = {7'b0000000, 5'd2,             5'd1, 3'b001, 5'd3, 7'b0110011}; // sll  x3,x1,x2
  localparam [31:0] W7  = {7'b0000000, 5'd2,             5'd1, 3'b011, 5'd3, 7'b0110011}; // sltu x3,x1,x2
  localparam [31:0] W8  = {20'h12345,                                  5'd3, 7'b0110111}; // lui  x3,0x12345
  localparam [31:0] W9  = {20'h12345,                                  5'd3, 7'b0010111}; // auipc x3,0x12345
  localparam [31:0] W10 = {1'b0, 10'd0, 1'b0, 8'd0,                    5'd3, 7'b1101111}; // jal  x3,0
  localparam [31:0] W11 = {12'd0,                        5'd1, 3'b000, 5'd3, 7'b1100111}; // jalr x3,x1,0
  localparam [31:0] W12 = {1'b0, 6'd0, 5'd2, 5'd1, 3'b000, 4'd0, 1'b0,       7'b1100011}; // beq  x1,x2,0
  localparam [31:0] W13 = {12'd0,                        5'd1, 3'b010, 5'd3, 7'b0000011}; // lw   x3,0(x1)
  localparam [31:0] W14 = {12'd0,                        5'd1, 3'b000, 5'd3, 7'b0000011}; // lb   x3,0(x1)
  localparam [31:0] W15 = {7'd0,   5'd2,                 5'd1, 3'b010, 5'd0, 7'b0100011}; // sw   x2,0(x1)
  localparam [31:0] W16 = {7'd0,   5'd2,                 5'd1, 3'b000, 5'd0, 7'b0100011}; // sb   x2,0(x1)
  localparam [31:0] W17 = {12'h300,                      5'd1, 3'b001, 5'd3, 7'b1110011}; // csrrw x3,mstatus,x1
  localparam [31:0] W18 = {12'h300,                      5'd1, 3'b010, 5'd3, 7'b1110011}; // csrrs x3,mstatus,x1
  localparam [31:0] W19 = 32'h00000073;                                                   // ecall

  function [31:0] rom_word;
    input [7:0] idx;
    case (idx % 20)
      0:  rom_word = W0;   1:  rom_word = W1;   2:  rom_word = W2;   3:  rom_word = W3;
      4:  rom_word = W4;   5:  rom_word = W5;   6:  rom_word = W6;   7:  rom_word = W7;
      8:  rom_word = W8;   9:  rom_word = W9;   10: rom_word = W10;  11: rom_word = W11;
      12: rom_word = W12;  13: rom_word = W13;  14: rom_word = W14;  15: rom_word = W15;
      16: rom_word = W16;  17: rom_word = W17;  18: rom_word = W18;
      default: rom_word = W19;
    endcase
  endfunction

  reg [31:0] mem [0:DEPTH-1];

  genvar gi;
  generate
    for (gi = 0; gi < DEPTH; gi = gi + 1) begin : gen_rom_init
      initial mem[gi] = rom_word(gi[7:0]);
    end
  endgenerate

  assign rdata = mem[addr[AW+1:2]];   // word-addressed: skip the 2 byte-offset bits
endmodule
