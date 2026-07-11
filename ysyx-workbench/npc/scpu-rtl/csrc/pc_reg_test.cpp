#include <verilated.h>
#include "Vpc_reg.h"
#include <cstdio>
#include <cassert>

static void single_cycle(Vpc_reg* dut) 
{
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vpc_reg* dut = new Vpc_reg{contextp};

    dut->rst = 1;
    single_cycle(dut);
    dut->rst = 0;
    printf("after reset: pc=%d (expect 0)\n", dut->pc);
    assert(dut->pc == 0);

    dut->next_pc = 5;
    single_cycle(dut);
    printf("after next_pc=5: pc=%d (expect 5)\n", dut->pc);
    assert(dut->pc == 5);

    dut->next_pc = 9;
    single_cycle(dut);
    printf("after next_pc=9: pc=%d (expect 9)\n", dut->pc);
    assert(dut->pc == 9);

    printf("pc_reg test PASSED\n");
    delete dut;
    return 0;
}
