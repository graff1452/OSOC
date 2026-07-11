module imem(
  input [3:0] addr,
  output [7:0] inst
);
  MuxKey #(16, 4, 8) rom (inst, addr, {
    4'd0,  8'h8A,
    4'd1,  8'hB1,
    4'd2,  8'h90,
    4'd3,  8'hA0,
    4'd4,  8'h17,
    4'd5,  8'h29,
    4'd6,  8'hD1,
    4'd7,  8'h60,
    4'd8,  8'hE3,
    4'd9,  8'h00,
    4'd10, 8'h00,
    4'd11, 8'h00,
    4'd12, 8'h00,
    4'd13, 8'h00,
    4'd14, 8'h00,
    4'd15, 8'h00
  });
endmodule
