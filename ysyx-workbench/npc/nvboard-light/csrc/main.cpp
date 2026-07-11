#include <nvboard.h>
#include <Vtop.h>

static Vtop dut;

void single_cycle() 
{
    dut.clk = 0; dut.eval();
    dut.clk = 1; dut.eval();
}

void reset(int n) 
{
    dut.rst = 1;
    while (n-- > 0) single_cycle();
    dut.rst = 0;
}

void nvboard_bind_all_pins(Vtop* top);

int main() 
{
    nvboard_bind_all_pins(&dut);
    nvboard_init();
    reset(10);
    while (1) 
    {
        single_cycle();
        nvboard_update();
    }

    nvboard_quit();
    return 0;
}