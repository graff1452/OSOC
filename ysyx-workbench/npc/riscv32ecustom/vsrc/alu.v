module alu
(
  input  [31:0] a,
  input  [31:0] b,
  input  [3:0]  op,
  output [31:0] result
);
  // op encoding: 0=ADD 1=SUB 2=AND 3=OR 4=XOR 5=SLL 6=SRL 7=SRA 8=SLT 9=SLTU
  // Only the low 5 bits of b matter for shift amount (RV32 shifts are 0-31).

  // Computed as its own wire, not inline in the ternary below: mixing a $signed()
  // cast directly inside a large ternary chain where most other branches are plain
  // unsigned expressions is a known Verilog trap -- the signed interpretation can
  // get silently discarded. Isolating it here sidesteps that entirely.
  wire signed [31:0] a_signed  = a;
  wire        [31:0] sra_result = a_signed >>> b[4:0];

  assign result = (op == 4'd1) ? (a - b)
                : (op == 4'd2) ? (a & b)
                : (op == 4'd3) ? (a | b)
                : (op == 4'd4) ? (a ^ b)
                : (op == 4'd5) ? (a << b[4:0])
                : (op == 4'd6) ? (a >> b[4:0])
                : (op == 4'd7) ? sra_result
                : (op == 4'd8) ? {31'b0, $signed(a) < $signed(b)}
                : (op == 4'd9) ? {31'b0, a < b}
                : (a + b);   // op==0 (ADD), also the default -- covers addi, lui,
                             // auipc, jalr target, load/store address calc
endmodule
