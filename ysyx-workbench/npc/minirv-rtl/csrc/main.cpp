#include <verilated.h>
#include <verilated_fst_c.h>
#include "Vminirv.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define PMEM_BASE 0x80000000u
#define MEM_SIZE  (128 * 1024 * 1024)
static uint8_t pmem[MEM_SIZE];

extern "C" int pmem_read(int raddr) 
{
    uint32_t addr = ((uint32_t)raddr - PMEM_BASE) & ~0x3u;
    if (addr >= MEM_SIZE) return 0;   // reads during reset, before pc is valid
    uint32_t val;
    memcpy(&val, &pmem[addr], 4);
    return (int)val;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) 
{
    uint32_t addr = ((uint32_t)waddr - PMEM_BASE) & ~0x3u;
    if (addr >= MEM_SIZE) return;
    for (int i = 0; i < 4; i++) 
    {
        if (wmask & (1 << i)) 
        {
            pmem[addr + i] = (wdata >> (8 * i)) & 0xFF;
        }
    }
}

static void load_image(const char *path) 
{
    FILE *fp = fopen(path, "rb");
    if (!fp) 
    {
        printf("ERROR: could not open image file '%s'\n", path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(pmem, 1, size, fp);
    fclose(fp);
    printf("loaded %ld bytes from %s\n", size, path);
}

bool sim_finished = false;
extern "C" void npc_trap() { sim_finished = true; }

int main(int argc, char** argv) 
{
    if (argc < 2) 
    {
        printf("usage: %s <image.bin>\n", argv[0]);
        return 1;
    }
    load_image(argv[1]);

    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);

    Vminirv* top = new Vminirv{contextp};

    VerilatedFstC* tfp = new VerilatedFstC;
    top->trace(tfp, 5);
    tfp->open("wave.fst");

    vluint64_t time = 0;
    top->rst = 1;
    top->clk = 0; top->eval(); tfp->dump(time++);
    top->clk = 1; top->eval(); tfp->dump(time++);
    top->rst = 0;

    int cycles = 0;
    const int MAX_CYCLES = 1000000;

    while (!sim_finished && cycles < MAX_CYCLES) 
    {
        top->clk = 0; top->eval(); tfp->dump(time++);
        top->clk = 1; top->eval(); tfp->dump(time++);
        cycles++;
    }

    if (sim_finished) 
    {
        int a0 = top->dbg_a0;
        if (a0 == 0) 
        {
            printf("HIT GOOD TRAP -- a0 = %d, simulation ended after %d cycles\n", a0, cycles);
        } 
        else 
        {
            printf("HIT BAD TRAP -- a0 = %d, simulation ended after %d cycles\n", a0, cycles);
        }
    } 
    else 
    {
        printf("HIT BAD TRAP (timeout) -- ran %d cycles without ebreak\n", cycles);
    }

    tfp->close();
    top->final();
    delete top;
    return 0;
}
