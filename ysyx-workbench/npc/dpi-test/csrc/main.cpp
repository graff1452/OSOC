#include <verilated.h>
#include "Vdpi_top.h"
#include <stdio.h>

extern "C" void notify_hit() {
    printf("notify_hit() called from Verilog!\n");
}

int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);

    Vdpi_top* top = new Vdpi_top{contextp};

    for (int i = 0; i < 3; i++) {
        top->clk = 0; top->eval();
        top->clk = 1; top->eval();
    }

    top->final();
    delete top;
    return 0;
}
