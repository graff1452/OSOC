#include <verilated.h>
#include <verilated_fst_c.h>   // NEW: needed for FST tracing
#include "Vexample.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);   // NEW: tell the context tracing will be used

    Vexample* example = new Vexample{contextp};

    VerilatedFstC* tfp = new VerilatedFstC;   // NEW: the trace object
    example->trace(tfp, 5);                        // NEW: register example's signals, depth 5
    tfp->open("wave.fst");                      // NEW: output file

    vluint64_t time = 0;   // NEW: tracks simulation time for the trace

    for (int i = 0; i < 30; i++) {   // CHANGED: bounded loop, not while(1)
        int a = rand() & 1;
        int b = rand() & 1;
        example->a = a;
        example->b = b;
        example->eval();
        tfp->dump(time);   // NEW: record this timestep
        time++;
        printf("a = %d, b = %d, f = %d\n", a, b, example->f);
        assert(example->f == (a ^ b));
    }

    tfp->close();   // NEW: flush/close the trace file
    example->final();
    delete example;
    return 0;
}