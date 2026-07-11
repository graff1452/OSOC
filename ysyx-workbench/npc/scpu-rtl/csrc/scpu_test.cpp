#include <verilated.h>
#include "Vscpu.h"
#include <cstdio>
#include <cassert>

static void single_cycle(Vscpu* dut) 
{
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

static void reset(Vscpu* dut, int n) 
{
    dut->rst = 1;
    while (n-- > 0) single_cycle(dut);
    dut->rst = 0;
}

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vscpu* dut = new Vscpu{contextp};

    reset(dut, 2);

    bool got_output = false;
    int result = -1;

    for (int i = 0; i < 100; i++) 
    {
      single_cycle(dut);
      if (dut->out_valid) 
      {
        got_output = true;
        result = dut->out_val;
        printf("OUT fired at cycle %d: value = %d\n", i, result);
      }
    }

    printf("got_output = %d, result = %d (expect 1, 55)\n", got_output, result);
    assert(got_output);
    assert(result == 55);

    printf("scpu test PASSED\n");
    delete dut;
    return 0;
}
