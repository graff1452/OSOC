module lsu
(
  input  [31:0] addr,
  input  [31:0] wdata,
  input         wen,
  input         ren,
  input  [1:0]  size,      // 0 = byte, 2 = word
  output [31:0] rdata
);
  import "DPI-C" function int  pmem_read(input int raddr);
  import "DPI-C" function void pmem_write(input int waddr, input int wdata, input byte wmask);

  wire [1:0]  offset    = addr[1:0];
  wire [31:0] word      = pmem_read(addr);
  wire [31:0] byte_val  = word >> (offset * 8);

  assign rdata = (size == 2'd0) ? {24'b0, byte_val[7:0]} : word;

  always @(*) begin
    if (wen) begin
      if (size == 2'd0)
        pmem_write(addr, wdata << (offset * 8), 8'b00000001 << offset);
      else
        pmem_write(addr, wdata, 8'b11111111);
    end
  end
endmodule
