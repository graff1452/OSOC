#include <verilated.h>
#include "Vregfile.h"
#include <cstdio>
#include <cassert>

static void single_cycle(Vregfile* dut) 
{
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

static void reset(Vregfile* dut, int n) 
{
    dut->rst = 1;
    while (n-- > 0) single_cycle(dut);
    dut->rst = 0;
}

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vregfile* dut = new Vregfile{contextp};

    reset(dut, 2);

    // Write 42 into register 2
    dut->waddr = 2;
    dut->wdata = 42;
    dut->wen = 1;
    single_cycle(dut);
    dut->wen = 0;

    // Read it back
    dut->raddr1 = 2;
    dut->eval();
    printf("Register 2 = %d (expect 42)\n", dut->rdata1);
    assert(dut->rdata1 == 42);

    // Confirm register 0 was untouched (should still be reset value 0)
    dut->raddr2 = 0;
    dut->eval();
    printf("Register 0 = %d (expect 0)\n", dut->rdata2);
    assert(dut->rdata2 == 0);

    printf("regfile test PASSED\n");
    delete dut;
    return 0;
}
