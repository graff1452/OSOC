#include <verilated.h>
#include <verilated_fst_c.h>
#include "Vminirv.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#define PMEM_BASE 0x80000000u
#define MEM_SIZE  (128 * 1024 * 1024)
static uint8_t pmem[MEM_SIZE];

static uint64_t get_elapsed_us()
{
    static uint64_t boot_time_us = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_us = (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u;
    if (boot_time_us == 0) boot_time_us = now_us;
    return now_us - boot_time_us;
}

#define UART_ADDR   0xa00003f8u
#define RTC_ADDR    0xa0000048u
#define VGACTL_ADDR 0xa0000100u   // word0 (read): (width<<16)|height. word0+4 (write): sync trigger
#define FB_ADDR     0xa1000000u
#define SCREEN_W    400
#define SCREEN_H    300
#define FB_SIZE     (SCREEN_W * SCREEN_H * 4)   // 480000 bytes

static uint32_t vmem[SCREEN_W * SCREEN_H];   // one uint32_t per pixel (ARGB8888), like NEMU's vmem

static SDL_Window   *vga_window   = NULL;
static SDL_Renderer *vga_renderer = NULL;
static SDL_Texture  *vga_texture  = NULL;

static void init_vga_if_needed()
{
    if (vga_window) return;   // already initialized -- only open the window once
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_W, SCREEN_H, 0, &vga_window, &vga_renderer);
    SDL_SetWindowTitle(vga_window, "minirv-npc");
    vga_texture = SDL_CreateTexture(vga_renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STATIC, SCREEN_W, SCREEN_H);
}

static void vga_redraw()
{
    init_vga_if_needed();
    SDL_UpdateTexture(vga_texture, NULL, vmem, SCREEN_W * sizeof(uint32_t));
    SDL_RenderClear(vga_renderer);
    SDL_RenderCopy(vga_renderer, vga_texture, NULL, NULL);
    SDL_RenderPresent(vga_renderer);

    // drain the event queue so the OS doesn't think the window has frozen --
    // NPC has no keyboard device, so the events themselves are just discarded
    SDL_Event e;
    while (SDL_PollEvent(&e)) {}
}

extern "C" int pmem_read(int raddr) 
{
    if ((uint32_t)raddr == VGACTL_ADDR) {
        return (int)((SCREEN_W << 16) | SCREEN_H);
    }
    if ((uint32_t)raddr == RTC_ADDR) {
        return (int)(uint32_t)(get_elapsed_us() & 0xFFFFFFFFu);
    }
    if ((uint32_t)raddr == RTC_ADDR + 4) {
        return (int)(uint32_t)(get_elapsed_us() >> 32);
    }

    uint32_t addr = ((uint32_t)raddr - PMEM_BASE) & ~0x3u;
    if (addr >= MEM_SIZE) return 0;   // reads during reset, before pc is valid
    uint32_t val;
    memcpy(&val, &pmem[addr], 4);
    return (int)val;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) 
{
    if ((uint32_t)waddr == UART_ADDR) {
        putchar(wdata & 0xFF);
        fflush(stdout);
        return;
    }
    if ((uint32_t)waddr == VGACTL_ADDR + 4) {
        vga_redraw();
        return;
    }
    if ((uint32_t)waddr >= FB_ADDR && (uint32_t)waddr < FB_ADDR + FB_SIZE) {
        uint32_t fb_off = ((uint32_t)waddr - FB_ADDR) & ~0x3u;   // align down to the pixel word,
        uint32_t pixel_index = fb_off / 4;                       // same pattern as the pmem[] path below
        uint8_t *dst = (uint8_t*)&vmem[pixel_index];
        for (int i = 0; i < 4; i++) {
            if (wmask & (1 << i)) {
                dst[i] = (wdata >> (8 * i)) & 0xFF;
            }
        }
        return;
    }

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

    long long cycles = 0;
    const long long MAX_CYCLES = 2000000000LL;
    const long long TRACE_CYCLES = 200000;   // only dump waveform for the first ~200k cycles

    while (!sim_finished && cycles < MAX_CYCLES) 
    {
        top->clk = 0; top->eval(); if (cycles < TRACE_CYCLES) tfp->dump(time++);
        top->clk = 1; top->eval(); if (cycles < TRACE_CYCLES) tfp->dump(time++);
        cycles++;
        if (cycles % 10000000 == 0) {
            fprintf(stderr, "... %lld cycles simulated\n", cycles);
        }
    }

    if (sim_finished) 
    {
        int a0 = top->dbg_a0;
        if (a0 == 0) 
        {
            printf("HIT GOOD TRAP -- a0 = %d, simulation ended after %lld cycles\n", a0, cycles);
        } 
        else 
        {
            printf("HIT BAD TRAP -- a0 = %d, simulation ended after %lld cycles\n", a0, cycles);
        }
    } 
    else 
    {
        printf("HIT BAD TRAP (timeout) -- ran %lld cycles without ebreak\n", cycles);
    }

    tfp->close();
    top->final();
    delete top;
    return 0;
}
