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
#define MEM_SIZE (128 * 1024 * 1024)
#define ENABLE_KEYBOARD // comment this line out to disable keyboard capture entirely
static uint8_t pmem[MEM_SIZE];

static uint64_t get_elapsed_us()
{
    static uint64_t boot_time_us = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_us = (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u;
    if (boot_time_us == 0)
        boot_time_us = now_us;
    return now_us - boot_time_us;
}

#define UART_ADDR 0xa00003f8u
#define RTC_ADDR 0xa0000048u
#define KBD_ADDR 0xa0000060u
#define VGACTL_ADDR 0xa0000100u // word0 (read): (width<<16)|height. word0+4 (write): sync trigger
#define FB_ADDR 0xa1000000u
#define SCREEN_W 400
#define SCREEN_H 300
#define FB_SIZE (SCREEN_W * SCREEN_H * 4) // 480000 bytes

static uint32_t vmem[SCREEN_W * SCREEN_H]; // one uint32_t per pixel (ARGB8888), like NEMU's vmem

// Same key list AM defines in amdev.h's AM_KEYS macro -- duplicated here since this
// file is compiled by the host g++, not minirv-gcc, and can't include AM's guest-side
// headers. Same trick NEMU's own keyboard.c uses for the same reason.
#define KEYDOWN_MASK 0x8000u
#define NPC_KEYS(f)                                                                                              \
    f(ESCAPE) f(F1) f(F2) f(F3) f(F4) f(F5) f(F6) f(F7) f(F8) f(F9) f(F10) f(F11) f(F12)                         \
        f(GRAVE) f(1) f(2) f(3) f(4) f(5) f(6) f(7) f(8) f(9) f(0) f(MINUS) f(EQUALS) f(BACKSPACE)               \
            f(TAB) f(Q) f(W) f(E) f(R) f(T) f(Y) f(U) f(I) f(O) f(P) f(LEFTBRACKET) f(RIGHTBRACKET) f(BACKSLASH) \
                f(CAPSLOCK) f(A) f(S) f(D) f(F) f(G) f(H) f(J) f(K) f(L) f(SEMICOLON) f(APOSTROPHE) f(RETURN)    \
                    f(LSHIFT) f(Z) f(X) f(C) f(V) f(B) f(N) f(M) f(COMMA) f(PERIOD) f(SLASH) f(RSHIFT)           \
                        f(LCTRL) f(APPLICATION) f(LALT) f(SPACE) f(RALT) f(RCTRL)                                \
                            f(UP) f(DOWN) f(LEFT) f(RIGHT) f(INSERT) f(DELETE) f(HOME) f(END) f(PAGEUP) f(PAGEDOWN)
#define NPC_KEY_NAME(k) NPC_KEY_##k,
enum
{
    NPC_KEY_NONE = 0,
    NPC_KEYS(NPC_KEY_NAME)
};

static uint32_t keymap[256] = {};
static bool keymap_ready = false;
static void init_keymap()
{
#define SDL_KEYMAP_ENTRY(k) keymap[SDL_SCANCODE_##k] = NPC_KEY_##k;
    NPC_KEYS(SDL_KEYMAP_ENTRY)
    keymap_ready = true;
}

#define KEY_QUEUE_LEN 1024
static uint32_t key_queue[KEY_QUEUE_LEN];
static int key_head = 0, key_tail = 0;

static void key_enqueue(uint32_t code)
{
    key_queue[key_tail] = code;
    key_tail = (key_tail + 1) % KEY_QUEUE_LEN;
    if (key_tail == key_head)
        key_head = (key_head + 1) % KEY_QUEUE_LEN; // drop oldest on overflow
}

static uint32_t key_dequeue()
{
    if (key_head == key_tail)
        return NPC_KEY_NONE;
    uint32_t code = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_LEN;
    return code;
}

static SDL_Window *vga_window = NULL;
static SDL_Renderer *vga_renderer = NULL;
static SDL_Texture *vga_texture = NULL;

static void init_vga_if_needed()
{
    if (vga_window)
        return; // already initialized -- only open the window once
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_W * 3, SCREEN_H * 3, 0, &vga_window, &vga_renderer);
    SDL_SetWindowTitle(vga_window, "minirv-npc");
    vga_texture = SDL_CreateTexture(vga_renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STATIC, SCREEN_W, SCREEN_H);
}

static void poll_sdl_events()
{
    init_vga_if_needed(); // a window has to exist before it can receive key focus
#ifdef ENABLE_KEYBOARD
    if (!keymap_ready)
        init_keymap();
#endif
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
#ifdef ENABLE_KEYBOARD
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
        {
            uint32_t scancode = e.key.keysym.scancode;
            if (scancode < 256 && keymap[scancode] != NPC_KEY_NONE)
            {
                uint32_t code = keymap[scancode] | (e.type == SDL_KEYDOWN ? KEYDOWN_MASK : 0);
                key_enqueue(code);
            }
        }
#endif
    }
}

static void vga_redraw()
{
    init_vga_if_needed();
    SDL_UpdateTexture(vga_texture, NULL, vmem, SCREEN_W * sizeof(uint32_t));
    SDL_RenderClear(vga_renderer);
    SDL_RenderCopy(vga_renderer, vga_texture, NULL, NULL);
    SDL_RenderPresent(vga_renderer);
    poll_sdl_events();
}

extern "C" int pmem_read(int raddr)
{
    if ((uint32_t)raddr == KBD_ADDR)
    {
#ifdef ENABLE_KEYBOARD
        poll_sdl_events();
        return (int)key_dequeue();
#else
        return (int)NPC_KEY_NONE;
#endif
    }
    if ((uint32_t)raddr == VGACTL_ADDR)
    {
        return (int)((SCREEN_W << 16) | SCREEN_H);
    }
    if ((uint32_t)raddr == RTC_ADDR)
    {
        return (int)(uint32_t)(get_elapsed_us() & 0xFFFFFFFFu);
    }
    if ((uint32_t)raddr == RTC_ADDR + 4)
    {
        return (int)(uint32_t)(get_elapsed_us() >> 32);
    }

    uint32_t addr = ((uint32_t)raddr - PMEM_BASE) & ~0x3u;
    if (addr >= MEM_SIZE)
        return 0; // reads during reset, before pc is valid
    uint32_t val;
    memcpy(&val, &pmem[addr], 4);
    return (int)val;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask)
{
    if ((uint32_t)waddr == UART_ADDR)
    {
        putchar(wdata & 0xFF);
        fflush(stdout);
        return;
    }
    if ((uint32_t)waddr == VGACTL_ADDR + 4)
    {
        vga_redraw();
        return;
    }
    if ((uint32_t)waddr >= FB_ADDR && (uint32_t)waddr < FB_ADDR + FB_SIZE)
    {
        uint32_t fb_off = ((uint32_t)waddr - FB_ADDR) & ~0x3u; // align down to the pixel word,
        uint32_t pixel_index = fb_off / 4;                     // same pattern as the pmem[] path below
        uint8_t *dst = (uint8_t *)&vmem[pixel_index];
        for (int i = 0; i < 4; i++)
        {
            if (wmask & (1 << i))
            {
                dst[i] = (wdata >> (8 * i)) & 0xFF;
            }
        }
        return;
    }

    uint32_t addr = ((uint32_t)waddr - PMEM_BASE) & ~0x3u;
    if (addr >= MEM_SIZE)
        return;
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

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: %s <image.bin>\n", argv[0]);
        return 1;
    }
    load_image(argv[1]);

    VerilatedContext *contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);

    Vminirv *top = new Vminirv{contextp};

    VerilatedFstC *tfp = new VerilatedFstC;
    top->trace(tfp, 5);
    tfp->open("wave.fst");

    vluint64_t time = 0;
    top->rst = 1;
    top->clk = 0;
    top->eval();
    tfp->dump(time++);
    top->clk = 1;
    top->eval();
    tfp->dump(time++);
    top->rst = 0;

    long long cycles = 0;
    const long long MAX_CYCLES = 2000000000LL;
    const long long TRACE_CYCLES = 200000; // only dump waveform for the first ~200k cycles

    while (!sim_finished && cycles < MAX_CYCLES)
    {
        top->clk = 0;
        top->eval();
        if (cycles < TRACE_CYCLES)
            tfp->dump(time++);
        top->clk = 1;
        top->eval();
        if (cycles < TRACE_CYCLES)
            tfp->dump(time++);
        cycles++;
        if (cycles % 10000000 == 0)
        {
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