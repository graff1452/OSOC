#include <verilated.h>
#include "Vdecoder.h"
#include <cstdio>
#include <cassert>

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vdecoder* dut = new Vdecoder{contextp};

    dut->inst = 0xD1;
    dut->eval();

    printf("opcode=%d rd=%d rs1=%d rs2=%d imm=%d baddr=%d\n",
           dut->opcode, dut->rd, dut->rs1, dut->rs2, dut->imm, dut->baddr);

    assert(dut->opcode == 0b11);
    assert(dut->baddr == 4);
    assert(dut->rs2 == 1);

    printf("decoder test PASSED\n");
    delete dut;
    return 0;
}