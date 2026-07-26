module xbar
(
  input  clk,
  input  rst,
  // Upstream (from arbiter) -- one shared bus, both read and write
  input  [31:0] araddr,
  input         arvalid,
  output        arready,
  output [31:0] rdata,
  output [1:0]  rresp,
  output        rvalid,
  input         rready,
  input  [31:0] awaddr,
  input         awvalid,
  output        awready,
  input  [31:0] wdata,
  input  [3:0]  wstrb,
  input         wvalid,
  output        wready,
  output [1:0]  bresp,
  output        bvalid,
  input         bready,

  // Downstream: axi_mem (SRAM, plus everything KBD/VGA/framebuffer-related
  // still handled by main.cpp's own DPI-C pmem_read/pmem_write -- step 9
  // only moves UART and CLINT into real RTL, per the handout's own scope)
  output [31:0] mem_araddr,
  output        mem_arvalid,
  input         mem_arready,
  input  [31:0] mem_rdata,
  input  [1:0]  mem_rresp,
  input         mem_rvalid,
  output        mem_rready,
  output [31:0] mem_awaddr,
  output        mem_awvalid,
  input         mem_awready,
  output [31:0] mem_wdata,
  output [3:0]  mem_wstrb,
  output        mem_wvalid,
  input         mem_wready,
  input  [1:0]  mem_bresp,
  input         mem_bvalid,
  output        mem_bready,

  // Downstream: uart (write-only -- no AR/R ports exist on this device)
  output [31:0] uart_awaddr,
  output        uart_awvalid,
  input         uart_awready,
  output [31:0] uart_wdata,
  output [3:0]  uart_wstrb,
  output        uart_wvalid,
  input         uart_wready,
  input  [1:0]  uart_bresp,
  input         uart_bvalid,
  output        uart_bready,

  // Downstream: clint (read-only -- no AW/W/B ports exist on this device)
  output [31:0] clint_araddr,
  output        clint_arvalid,
  input         clint_arready,
  input  [31:0] clint_rdata,
  input  [1:0]  clint_rresp,
  input         clint_rvalid,
  output        clint_rready
);
  // Step 9: address-based routing. These constants are the exact same
  // addresses main.cpp's DPI-C layer and abstract-machine's npc.h have
  // used all along (kept in sync by hand between those two files, same as
  // always) -- this xbar intercepts them before they'd ever reach
  // axi_mem's DPI-C calls, replacing that special-casing with real
  // hardware for these two devices specifically.
  //
  // No DECERR/address-fault path yet for addresses outside all three
  // known ranges -- everything not matching uart/clint just falls through
  // to axi_mem by default, same as it always implicitly has (KBD/VGA/
  // framebuffer addresses are "not uart, not clint" and always fell
  // through to axi_mem's own DPI-C handling; that doesn't change here).
  // Real address-range/PMA checking is exactly the kind of "error handling
  // and exceptions" this design has consistently left as explicit future
  // work at every step (rresp/bresp have always been hardwired OKAY).
  localparam UART_ADDR = 32'ha00003f8;
  localparam RTC_ADDR  = 32'ha0000048;

  // ---- Write side: exactly one of {uart, axi_mem} ever gets AW/W/B ----
  // (clint has no write ports; nothing in this design ever writes to it)
  wire sel_uart_w = (awaddr == UART_ADDR);

  assign uart_awaddr  = awaddr;
  assign uart_awvalid = awvalid & sel_uart_w;
  assign uart_wdata   = wdata;
  assign uart_wstrb   = wstrb;
  assign uart_wvalid  = wvalid & sel_uart_w;
  assign uart_bready  = bready & sel_uart_w;

  assign mem_awaddr  = awaddr;
  assign mem_awvalid = awvalid & ~sel_uart_w;
  assign mem_wdata   = wdata;
  assign mem_wstrb   = wstrb;
  assign mem_wvalid  = wvalid & ~sel_uart_w;
  assign mem_bready  = bready & ~sel_uart_w;

  assign awready = sel_uart_w ? uart_awready : mem_awready;
  assign wready  = sel_uart_w ? uart_wready  : mem_wready;
  assign bvalid  = sel_uart_w ? uart_bvalid  : mem_bvalid;
  assign bresp   = sel_uart_w ? uart_bresp   : mem_bresp;

  // ---- Read side: exactly one of {clint, axi_mem} ever gets AR/R ----
  // (uart has no read ports; nothing in this design ever reads from it)
  wire sel_clint_r = (araddr == RTC_ADDR) || (araddr == RTC_ADDR + 32'd4);

  assign clint_araddr  = araddr;
  assign clint_arvalid = arvalid & sel_clint_r;
  assign clint_rready  = rready  & sel_clint_r;

  assign mem_araddr  = araddr;
  assign mem_arvalid = arvalid & ~sel_clint_r;
  assign mem_rready  = rready  & ~sel_clint_r;

  assign arready = sel_clint_r ? clint_arready : mem_arready;
  assign rvalid  = sel_clint_r ? clint_rvalid  : mem_rvalid;
  assign rdata   = sel_clint_r ? clint_rdata   : mem_rdata;
  assign rresp   = sel_clint_r ? clint_rresp   : mem_rresp;
endmodule
