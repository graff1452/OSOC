#include <verilated.h>
#include <verilated_fst_c.h>   // NEW: needed for FST tracing
#include "Vtop.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);   // NEW: tell the context tracing will be used

    Vtop* top = new Vtop{contextp};

    VerilatedFstC* tfp = new VerilatedFstC;   // NEW: the trace object
    top->trace(tfp, 5);                        // NEW: register top's signals, depth 5
    tfp->open("wave.fst");                      // NEW: output file

    vluint64_t time = 0;   // NEW: tracks simulation time for the trace

    for (int i = 0; i < 30; i++) {   // CHANGED: bounded loop, not while(1)
        int a = rand() & 1;
        int b = rand() & 1;
        top->a = a;
        top->b = b;
        top->eval();
        tfp->dump(time);   // NEW: record this timestep
        time++;
        printf("a = %d, b = %d, f = %d\n", a, b, top->f);
        assert(top->f == (a ^ b));
    }

    tfp->close();   // NEW: flush/close the trace file
    top->final();
    delete top;
    return 0;
}
