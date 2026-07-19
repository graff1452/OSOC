module lsu
(
  input  [31:0] addr,
  input  [31:0] wdata,
  input         wen,
  input         ren,
  input  [2:0]  size,      // matches funct3 directly: 000=byte(signed) 001=half(signed)
                            // 010=word 100=byte(unsigned) 101=half(unsigned)
  output [31:0] rdata
);
  import "DPI-C" function int  pmem_read(input int raddr);
  import "DPI-C" function void pmem_write(input int waddr, input int wdata, input byte wmask);

  wire [1:0]  offset       = addr[1:0];
  wire [31:0] word         = pmem_read(addr);
  wire [31:0] byte_shifted = word >> (offset * 8);
  wire [31:0] half_shifted = word >> (offset[1] * 16);   // halfword accesses are 2-byte aligned,
                                                          // only offset's bit 1 (0 or 2) matters
  wire [7:0]  byte_val = byte_shifted[7:0];
  wire [15:0] half_val = half_shifted[15:0];

  assign rdata = (size == 3'b000) ? {{24{byte_val[7]}},  byte_val}   // lb  (sign-extended)
               : (size == 3'b100) ? {24'b0,               byte_val}   // lbu (zero-extended)
               : (size == 3'b001) ? {{16{half_val[15]}}, half_val}   // lh  (sign-extended)
               : (size == 3'b101) ? {16'b0,               half_val}   // lhu (zero-extended)
               : word;                                                 // lw

  always @(*) 
  begin
    if (wen) 
    begin
      if (size == 3'b000)                                              // sb
        pmem_write(addr, wdata << (offset * 8), 8'b00000001 << offset);
      else if (size == 3'b001)                                         // sh
        pmem_write(addr, wdata << (offset[1] * 16), 8'b00000011 << (offset[1] * 2));
      else                                                              // sw
        pmem_write(addr, wdata, 8'b11111111);
    end
  end
endmodule
