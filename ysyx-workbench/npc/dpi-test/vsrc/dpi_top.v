module dpi_top(input clk);
  import "DPI-C" function void notify_hit();

  always @(posedge clk) begin
    notify_hit();
  end
endmodule
