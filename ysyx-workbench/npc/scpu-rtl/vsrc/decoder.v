module decoder
(
    input [7:0] inst,
    output [1:0] opcode,
    output [1:0] rd,
    output [1:0] rs1,
    output [1:0] rs2,
    output [3:0] imm,
    output [3:0] baddr
);
    assign opcode = inst[7:6];
    assign rd = inst[5:4];
    assign rs1 = inst[3:2];
    assign rs2 = inst[1:0];
    assign imm = inst[3:0];
    assign baddr = inst[5:2];
endmodule
