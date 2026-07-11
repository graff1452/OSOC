#include <verilated.h>
#include "Valu.h"
#include <cstdio>
#include <cassert>

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Valu* dut = new Valu{contextp};

    dut->a = 7;
    dut->b = 3;
    dut->eval();
    printf("7 + 3 = %d (expect 10)\n", dut->result);
    assert(dut->result == 10);

    dut->a = 200;
    dut->b = 100;
    dut->eval();
    printf("200 + 100 = %d (expect %d, 8-bit wraparound)\n", dut->result, (200+100) & 0xFF);
    assert(dut->result == ((200+100) & 0xFF));

    printf("alu test PASSED\n");
    delete dut;
    return 0;
}
