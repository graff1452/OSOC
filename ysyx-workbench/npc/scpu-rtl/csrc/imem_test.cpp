#include <verilated.h>
#include "Vimem.h"
#include <cstdio>
#include <cassert>

int main(int argc, char** argv) 
{
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vimem* dut = new Vimem{contextp};

    uint8_t expected[16] = 
    {
        0x8A, 0xB1, 0x90, 0xA0, 0x17, 0x29, 0xD1, 0x60,
        0xE3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    for (int a = 0; a < 16; a++) 
    {
        dut->addr = a;
        dut->eval();
        printf("addr=%2d inst=0x%02x (expect 0x%02x)\n", a, dut->inst, expected[a]);
        assert(dut->inst == expected[a]);
    }

    printf("imem test PASSED\n");
    delete dut;
    return 0;
}
