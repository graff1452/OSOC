#include <verilated.h>
#include <verilated_fst_c.h>
#include <verilated_dpi.h>
#include "Vminirv.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dlfcn.h>
#include <capstone/capstone.h>
#include <elf.h>

#define PMEM_BASE 0x80000000u
#define MEM_SIZE  (128 * 1024 * 1024)
#define ENABLE_KEYBOARD   // comment this line out to disable keyboard capture entirely
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
#define KBD_ADDR    0xa0000060u
#define VGACTL_ADDR 0xa0000100u   // word0 (read): (width<<16)|height. word0+4 (write): sync trigger
#define FB_ADDR     0xa1000000u
#define SCREEN_W    400
#define SCREEN_H    300
#define FB_SIZE     (SCREEN_W * SCREEN_H * 4)   // 480000 bytes

static uint32_t vmem[SCREEN_W * SCREEN_H];   // one uint32_t per pixel (ARGB8888), like NEMU's vmem

// Same key list AM defines in amdev.h's AM_KEYS macro -- duplicated here since this
// file is compiled by the host g++, not minirv-gcc, and can't include AM's guest-side
// headers. Same trick NEMU's own keyboard.c uses for the same reason.
#define KEYDOWN_MASK 0x8000u
#define NPC_KEYS(f) \
  f(ESCAPE) f(F1) f(F2) f(F3) f(F4) f(F5) f(F6) f(F7) f(F8) f(F9) f(F10) f(F11) f(F12) \
  f(GRAVE) f(1) f(2) f(3) f(4) f(5) f(6) f(7) f(8) f(9) f(0) f(MINUS) f(EQUALS) f(BACKSPACE) \
  f(TAB) f(Q) f(W) f(E) f(R) f(T) f(Y) f(U) f(I) f(O) f(P) f(LEFTBRACKET) f(RIGHTBRACKET) f(BACKSLASH) \
  f(CAPSLOCK) f(A) f(S) f(D) f(F) f(G) f(H) f(J) f(K) f(L) f(SEMICOLON) f(APOSTROPHE) f(RETURN) \
  f(LSHIFT) f(Z) f(X) f(C) f(V) f(B) f(N) f(M) f(COMMA) f(PERIOD) f(SLASH) f(RSHIFT) \
  f(LCTRL) f(APPLICATION) f(LALT) f(SPACE) f(RALT) f(RCTRL) \
  f(UP) f(DOWN) f(LEFT) f(RIGHT) f(INSERT) f(DELETE) f(HOME) f(END) f(PAGEUP) f(PAGEDOWN)
#define NPC_KEY_NAME(k) NPC_KEY_##k,
enum { NPC_KEY_NONE = 0, NPC_KEYS(NPC_KEY_NAME) };

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
    if (key_tail == key_head) key_head = (key_head + 1) % KEY_QUEUE_LEN;  // drop oldest on overflow
}

static uint32_t key_dequeue()
{
    if (key_head == key_tail) return NPC_KEY_NONE;
    uint32_t code = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_LEN;
    return code;
}

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

static void poll_sdl_events()
{
    init_vga_if_needed();   // a window has to exist before it can receive key focus
#ifdef ENABLE_KEYBOARD
    if (!keymap_ready) init_keymap();
#endif
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef ENABLE_KEYBOARD
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            uint32_t scancode = e.key.keysym.scancode;
            if (scancode < 256 && keymap[scancode] != NPC_KEY_NONE) {
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
    if ((uint32_t)raddr == KBD_ADDR) {
#ifdef ENABLE_KEYBOARD
        poll_sdl_events();
        return (int)key_dequeue();
#else
        return (int)NPC_KEY_NONE;
#endif
    }
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

static long g_image_size = 0;

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
    g_image_size = size;
}

bool sim_finished = false;
extern "C" void npc_trap() { sim_finished = true; }
extern "C" int get_reg_val(int idx);   // DPI-C export from regfile.v

static const char *reg_names[16] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5"
};

// ---- DiffTest: compare every instruction's result against NEMU (as REF) ----
// Mirrors NEMU's own riscv32_CPU_state layout exactly. NEMU here is configured
// RV32IM (32 registers), not RV32E, so this struct needs all 32 slots even though
// our real hardware only has 16 -- the extra slots are simply never touched by
// real RV32E programs, whose 4-bit register fields can't encode x16-x31 at all.
struct DifftestCPUState { uint32_t gpr[32]; uint32_t pc; };
enum { DIFFTEST_TO_DUT = 0, DIFFTEST_TO_REF = 1 };

static void (*ref_difftest_memcpy)(uint32_t addr, void *buf, size_t n, int direction) = nullptr;
static void (*ref_difftest_regcpy)(void *dut, int direction) = nullptr;
static void (*ref_difftest_exec)(uint64_t n) = nullptr;
static void (*ref_difftest_init)(int port) = nullptr;
// ---- itrace: disassemble every retired instruction, keep a ring buffer of the
// most recent ones for crash/mismatch context (mirrors NEMU's own iringbuf).
// Reuses NEMU's already-built capstone library via dlopen -- the same approach
// NEMU itself uses internally (see nemu/src/utils/disasm.c) -- rather than
// building our own separate copy.
static csh    cs_handle;
static size_t (*cs_disasm_dl)(csh, const uint8_t*, size_t, uint64_t, size_t, cs_insn**) = nullptr;
static void   (*cs_free_dl)(cs_insn*, size_t) = nullptr;
static bool   itrace_ready = false;

static void itrace_init(const char *capstone_so_path)
{
    void *handle = dlopen(capstone_so_path, RTLD_LAZY);
    if (!handle) { fprintf(stderr, "dlopen('%s') failed: %s\n", capstone_so_path, dlerror()); exit(1); }

    cs_err (*cs_open_dl)(cs_arch, cs_mode, csh*) = (cs_err(*)(cs_arch,cs_mode,csh*))dlsym(handle, "cs_open");
    cs_disasm_dl = (size_t(*)(csh,const uint8_t*,size_t,uint64_t,size_t,cs_insn**))dlsym(handle, "cs_disasm");
    cs_free_dl   = (void(*)(cs_insn*,size_t))dlsym(handle, "cs_free");
    if (!cs_open_dl || !cs_disasm_dl || !cs_free_dl) {
        fprintf(stderr, "dlsym failed for capstone functions: %s\n", dlerror());
        exit(1);
    }
    cs_err ret = cs_open_dl(CS_ARCH_RISCV, CS_MODE_RISCV32, &cs_handle);
    if (ret != CS_ERR_OK) { fprintf(stderr, "cs_open failed (err=%d)\n", ret); exit(1); }

    itrace_ready = true;
    printf("itrace enabled, capstone loaded from %s\n", capstone_so_path);
}

#define IRINGBUF_SIZE 16
struct IringEntry { uint32_t pc; uint32_t inst; };
static IringEntry iringbuf[IRINGBUF_SIZE];
static int iringbuf_idx   = 0;
static int iringbuf_count = 0;

static void itrace_format(char *buf, size_t bufsize, uint32_t pc, uint32_t inst)
{
    uint8_t bytes[4] = { (uint8_t)inst, (uint8_t)(inst >> 8), (uint8_t)(inst >> 16), (uint8_t)(inst >> 24) };
    char asmstr[192] = "(disasm unavailable -- pass --itrace <capstone.so>)";
    if (itrace_ready) {
        cs_insn *insn;
        size_t count = cs_disasm_dl(cs_handle, bytes, 4, pc, 0, &insn);
        if (count == 1) {
            snprintf(asmstr, sizeof(asmstr), "%s%s%s", insn->mnemonic,
                     insn->op_str[0] ? "\t" : "", insn->op_str);
            cs_free_dl(insn, count);
        } else {
            snprintf(asmstr, sizeof(asmstr), "(bad)");
        }
    }
    snprintf(buf, bufsize, "0x%08x: %02x %02x %02x %02x  %s",
             pc, bytes[3], bytes[2], bytes[1], bytes[0], asmstr);
}

// Cheap: just two integer stores, safe to call every single cycle regardless of
// whether --itrace is even enabled. The expensive part (string formatting, the
// capstone disassembly call) is deferred to itrace_display_ring()/cmd_si below,
// which only run when something is actually about to be shown. Recording the
// formatted string here instead (as an earlier version did) meant a real
// snprintf() call on every cycle even with itrace fully disabled -- measurably
// slower on long runs like FCEUX, for a ring buffer nothing was reading yet.
static void itrace_record(uint32_t pc, uint32_t inst)
{
    iringbuf[iringbuf_idx].pc   = pc;
    iringbuf[iringbuf_idx].inst = inst;
    iringbuf_idx = (iringbuf_idx + 1) % IRINGBUF_SIZE;
    if (iringbuf_count < IRINGBUF_SIZE) iringbuf_count++;
}

static void itrace_display_ring()
{
    int n = iringbuf_count;
    int start = (iringbuf_idx - n + IRINGBUF_SIZE) % IRINGBUF_SIZE;
    for (int i = 0; i < n; i++) {
        int idx = (start + i) % IRINGBUF_SIZE;
        char buf[256];
        itrace_format(buf, sizeof(buf), iringbuf[idx].pc, iringbuf[idx].inst);
        printf("%s %s\n", (i == n - 1) ? "-->" : "   ", buf);
    }
}

// ---- mtrace: log real load/store instructions only (not instruction fetch --
// ifu.v calls pmem_read every single cycle regardless, which would flood this
// with noise; dbg_mem_wen/ren correctly distinguish "this is a real LSU access").
static bool g_mtrace_enabled = false;

static void mtrace_log(uint32_t addr, bool is_write, uint32_t value)
{
    fprintf(stderr, "[mtrace] %s 0x%08x = 0x%08x\n", is_write ? "write" : "read ", addr, value);
}

// ---- expression evaluation, mirrors NEMU's own expr.c: same tokenizer-via-
// regex approach, same recursive-descent/operator-precedence evaluator.
#include <regex.h>

static Vminirv        *g_top   = nullptr;
static VerilatedFstC   *g_tfp   = nullptr;
static vluint64_t       g_time  = 0;
static long long        g_cycles = 0;
static const long long  MAX_CYCLES   = 2000000000LL;
static const long long  TRACE_CYCLES = 200000;   // only dump waveform for the first ~200k cycles

enum { TK_NOTYPE = 256, TK_EQ, TK_NUM, TK_NEQ, TK_AND, TK_REG };

struct ExprRule { const char *regex; int token_type; };
static ExprRule expr_rules[] = {
    {" +", TK_NOTYPE},
    {"\\+", '+'},
    {"-", '-'},
    {"\\*", '*'},
    {"/", '/'},
    {"\\(", '('},
    {"\\)", ')'},
    {"[0-9]+", TK_NUM},
    {"\\$[a-zA-Z0-9]+", TK_REG},   // e.g. $pc, $a0, $x5
    {"==", TK_EQ},
    {"!=", TK_NEQ},
    {"&&", TK_AND},
};
#define NR_EXPR_RULES (sizeof(expr_rules) / sizeof(expr_rules[0]))
static regex_t expr_re[NR_EXPR_RULES];
static bool expr_regex_ready = false;

static void expr_init_regex()
{
    for (size_t i = 0; i < NR_EXPR_RULES; i++) {
        int ret = regcomp(&expr_re[i], expr_rules[i].regex, REG_EXTENDED);
        if (ret != 0) {
            char err[128];
            regerror(ret, &expr_re[i], err, sizeof(err));
            fprintf(stderr, "regex compile failed: %s\n", err);
            exit(1);
        }
    }
    expr_regex_ready = true;
}

struct ExprToken { int type; char str[32]; };
static ExprToken expr_tokens[1024];
static int expr_nr_token = 0;

static bool expr_make_token(const char *e)
{
    if (!expr_regex_ready) expr_init_regex();
    int position = 0;
    expr_nr_token = 0;
    int elen = (int)strlen(e);
    while (position < elen) {
        size_t i;
        regmatch_t pmatch;
        for (i = 0; i < NR_EXPR_RULES; i++) {
            if (regexec(&expr_re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                int substr_len = (int)pmatch.rm_eo;
                if (expr_rules[i].token_type != TK_NOTYPE) {
                    if (expr_nr_token >= 1024) { printf("too many tokens\n"); return false; }
                    expr_tokens[expr_nr_token].type = expr_rules[i].token_type;
                    int len = substr_len < 31 ? substr_len : 31;
                    strncpy(expr_tokens[expr_nr_token].str, e + position, len);
                    expr_tokens[expr_nr_token].str[len] = '\0';
                    expr_nr_token++;
                }
                position += substr_len;
                break;
            }
        }
        if (i == NR_EXPR_RULES) {
            printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
            return false;
        }
    }
    return true;
}

static bool expr_check_parens(int p, int q)
{
    if (expr_tokens[p].type != '(' || expr_tokens[q].type != ')') return false;
    int balance = 0;
    for (int i = p; i <= q; i++) {
        if (expr_tokens[i].type == '(') balance++;
        if (expr_tokens[i].type == ')') balance--;
        if (balance == 0 && i != q) return false;
    }
    return balance == 0;
}

static int expr_op_prec(int type)
{
    switch (type) {
        case TK_AND: return 0;
        case TK_EQ: case TK_NEQ: return 1;
        case '+': case '-': return 2;
        case '*': case '/': return 3;
        default: return -1;
    }
}

// $pc, $zero/$ra/.../$a5 (ABI names, matching reg_names[]), or $x0-$x15
static bool reg_str2val(const char *name, uint32_t *out)
{
    if (strcmp(name, "pc") == 0) { *out = g_top->pc; return true; }
    for (int i = 0; i < 16; i++) {
        if (strcmp(name, reg_names[i]) == 0) { *out = (uint32_t)get_reg_val(i); return true; }
    }
    if (name[0] == 'x') {
        int idx = atoi(name + 1);
        if (idx >= 0 && idx < 16) { *out = (uint32_t)get_reg_val(idx); return true; }
    }
    return false;
}

static int32_t expr_eval(int p, int q, bool *success)
{
    if (p > q) { printf("Bad expression\n"); *success = false; return 0; }
    if (p == q) {
        if (expr_tokens[p].type == TK_NUM) return (int32_t)atoi(expr_tokens[p].str);
        if (expr_tokens[p].type == TK_REG) {
            uint32_t val;
            if (!reg_str2val(expr_tokens[p].str + 1, &val)) {
                printf("Unknown register '%s'\n", expr_tokens[p].str);
                *success = false;
                return 0;
            }
            return (int32_t)val;
        }
        printf("Expected a number or register\n");
        *success = false;
        return 0;
    }
    if (expr_check_parens(p, q)) return expr_eval(p + 1, q - 1, success);

    int op = -1, min_prec = 1000, depth = 0;
    for (int i = p; i <= q; i++) {
        if (expr_tokens[i].type == '(') { depth++; continue; }
        if (expr_tokens[i].type == ')') { depth--; continue; }
        if (depth > 0) continue;
        int prec = expr_op_prec(expr_tokens[i].type);
        if (prec == -1) continue;
        if (prec <= min_prec) { min_prec = prec; op = i; }
    }
    if (op == -1) { printf("No operator found\n"); *success = false; return 0; }

    int32_t v1 = expr_eval(p, op - 1, success);
    if (!*success) return 0;
    int32_t v2 = expr_eval(op + 1, q, success);
    if (!*success) return 0;

    switch (expr_tokens[op].type) {
        case '+': return v1 + v2;
        case '-': return v1 - v2;
        case '*': return v1 * v2;
        case '/':
            if (v2 == 0) { printf("Division by zero\n"); *success = false; return 0; }
            return v1 / v2;
        case TK_EQ:  return v1 == v2;
        case TK_NEQ: return v1 != v2;
        case TK_AND: return v1 && v2;
        default: *success = false; return 0;
    }
}

static int32_t expr_evaluate(const char *e, bool *success)
{
    if (!expr_make_token(e)) { *success = false; return 0; }
    *success = true;
    return expr_eval(0, expr_nr_token - 1, success);
}

// ---- watchpoints, mirrors NEMU's own watchpoint.c: fixed-size pool, two
// linked lists (active list + free list) threaded through the same array.
#define NR_WP 32
struct WP { int NO; char expr_str[128]; int32_t old_val; WP *next; };
static WP wp_pool[NR_WP];
static WP *wp_head = nullptr, *wp_free_list = nullptr;
static bool g_wp_triggered = false;

static void wp_pool_init()
{
    for (int i = 0; i < NR_WP; i++) {
        wp_pool[i].NO = i;
        wp_pool[i].next = (i == NR_WP - 1) ? nullptr : &wp_pool[i + 1];
    }
    wp_head = nullptr;
    wp_free_list = wp_pool;
}

static WP* wp_new()
{
    if (!wp_free_list) { printf("No free watchpoints left\n"); return nullptr; }
    WP *wp = wp_free_list;
    wp_free_list = wp_free_list->next;
    wp->next = wp_head;
    wp_head = wp;
    return wp;
}

static void wp_free_one(WP *wp)
{
    if (wp_head == wp) {
        wp_head = wp_head->next;
    } else {
        WP *p = wp_head;
        while (p && p->next != wp) p = p->next;
        if (p) p->next = wp->next;
    }
    wp->next = wp_free_list;
    wp_free_list = wp;
}

static WP* wp_find(int no)
{
    for (WP *p = wp_head; p; p = p->next) if (p->NO == no) return p;
    return nullptr;
}

// Called once per retired instruction, mirrors NEMU's check_watchpoints():
// re-evaluate every active watchpoint's expression, report + stop if changed.
static void check_watchpoints()
{
    for (WP *p = wp_head; p; p = p->next) {
        bool success;
        int32_t new_val = expr_evaluate(p->expr_str, &success);
        if (success && new_val != p->old_val) {
            printf("Watchpoint %d: %s\n", p->NO, p->expr_str);
            printf("Old value = %d\n", p->old_val);
            printf("New value = %d\n", new_val);
            p->old_val = new_val;
            g_wp_triggered = true;
        }
    }
}

static bool difftest_enabled  = false;

// ---- ftrace: track function calls/returns via jal/jalr, mirroring NEMU's own
// ftrace.c: same ELF symbol table parsing, same indent-by-call-depth display.
#define MAX_FUNCS 512
struct FuncSym { uint32_t addr; uint32_t size; char name[64]; };
static FuncSym g_funcs[MAX_FUNCS];
static int  g_nr_funcs     = 0;
static int  g_ftrace_depth = 0;
static bool g_ftrace_enabled = false;

static void ftrace_init(const char *elf_path)
{
    FILE *fp = fopen(elf_path, "rb");
    if (!fp) { fprintf(stderr, "ftrace: could not open '%s'\n", elf_path); exit(1); }

    Elf32_Ehdr ehdr;
    if (fread(&ehdr, sizeof(ehdr), 1, fp) != 1) { fprintf(stderr, "ftrace: bad ELF header\n"); exit(1); }

    Elf32_Shdr *shdrs = (Elf32_Shdr*)malloc(sizeof(Elf32_Shdr) * ehdr.e_shnum);
    fseek(fp, ehdr.e_shoff, SEEK_SET);
    if (fread(shdrs, sizeof(Elf32_Shdr), ehdr.e_shnum, fp) != (size_t)ehdr.e_shnum) {
        fprintf(stderr, "ftrace: could not read section headers\n"); exit(1);
    }

    Elf32_Shdr *shstrtab_hdr = &shdrs[ehdr.e_shstrndx];
    char *shstrtab = (char*)malloc(shstrtab_hdr->sh_size);
    fseek(fp, shstrtab_hdr->sh_offset, SEEK_SET);
    if (fread(shstrtab, shstrtab_hdr->sh_size, 1, fp) != 1) { fprintf(stderr, "ftrace: read error\n"); exit(1); }

    Elf32_Shdr *symtab_hdr = nullptr, *strtab_hdr = nullptr;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        char *name = shstrtab + shdrs[i].sh_name;
        if (strcmp(name, ".symtab") == 0) symtab_hdr = &shdrs[i];
        if (strcmp(name, ".strtab") == 0) strtab_hdr = &shdrs[i];
    }
    if (!symtab_hdr || !strtab_hdr) {
        fprintf(stderr, "ftrace: '%s' has no symbol table (was it stripped?)\n", elf_path);
        exit(1);
    }

    char *strtab = (char*)malloc(strtab_hdr->sh_size);
    fseek(fp, strtab_hdr->sh_offset, SEEK_SET);
    if (fread(strtab, strtab_hdr->sh_size, 1, fp) != 1) { fprintf(stderr, "ftrace: read error\n"); exit(1); }

    int nr_syms = symtab_hdr->sh_size / sizeof(Elf32_Sym);
    Elf32_Sym *syms = (Elf32_Sym*)malloc(symtab_hdr->sh_size);
    fseek(fp, symtab_hdr->sh_offset, SEEK_SET);
    if (fread(syms, symtab_hdr->sh_size, 1, fp) != 1) { fprintf(stderr, "ftrace: read error\n"); exit(1); }

    for (int i = 0; i < nr_syms && g_nr_funcs < MAX_FUNCS; i++) {
        if (ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0) {
            g_funcs[g_nr_funcs].addr = syms[i].st_value;
            g_funcs[g_nr_funcs].size = syms[i].st_size;
            snprintf(g_funcs[g_nr_funcs].name, sizeof(g_funcs[g_nr_funcs].name), "%s", strtab + syms[i].st_name);
            g_nr_funcs++;
        }
    }

    free(shstrtab); free(strtab); free(syms); free(shdrs);
    fclose(fp);
    g_ftrace_enabled = true;
    printf("ftrace enabled, loaded %d function symbols from %s\n", g_nr_funcs, elf_path);
}

static const char *addr_to_func(uint32_t addr)
{
    for (int i = 0; i < g_nr_funcs; i++) {
        if (addr >= g_funcs[i].addr && addr < g_funcs[i].addr + g_funcs[i].size) return g_funcs[i].name;
    }
    return "???";
}

static void ftrace_call(uint32_t pc, uint32_t target)
{
    printf("0x%08x:", pc);
    for (int i = 0; i < g_ftrace_depth; i++) printf("  ");
    printf("call [%s@0x%08x]\n", addr_to_func(target), target);
    g_ftrace_depth++;
}

static void ftrace_ret(uint32_t pc)
{
    if (g_ftrace_depth > 0) g_ftrace_depth--;
    printf("0x%08x:", pc);
    for (int i = 0; i < g_ftrace_depth; i++) printf("  ");
    printf("ret  [%s]\n", addr_to_func(pc));
}
static bool difftest_mismatch = false;
static bool g_touched_device  = false;   // recomputed fresh every cycle from settled port values

static void difftest_load(const char *so_path)
{
    void *handle = dlopen(so_path, RTLD_LAZY);
    if (!handle) { fprintf(stderr, "dlopen('%s') failed: %s\n", so_path, dlerror()); exit(1); }

    ref_difftest_memcpy = (void(*)(uint32_t,void*,size_t,int))dlsym(handle, "difftest_memcpy");
    ref_difftest_regcpy = (void(*)(void*,int))dlsym(handle, "difftest_regcpy");
    ref_difftest_exec   = (void(*)(uint64_t))dlsym(handle, "difftest_exec");
    ref_difftest_init   = (void(*)(int))dlsym(handle, "difftest_init");
    if (!ref_difftest_memcpy || !ref_difftest_regcpy || !ref_difftest_exec || !ref_difftest_init) {
        fprintf(stderr, "dlsym failed for one or more difftest_* functions: %s\n", dlerror());
        exit(1);
    }

    ref_difftest_init(0);
    ref_difftest_memcpy(PMEM_BASE, pmem, g_image_size, DIFFTEST_TO_REF);

    DifftestCPUState init_state;
    memset(&init_state, 0, sizeof(init_state));
    init_state.pc = PMEM_BASE;
    ref_difftest_regcpy(&init_state, DIFFTEST_TO_REF);

    difftest_enabled = true;
    printf("DiffTest enabled, REF loaded from %s\n", so_path);
}

// Called once per real instruction retired (this core is single-cycle: one
// instruction per clock cycle, so once per step_n_cycles loop iteration).
static bool is_device_addr(uint32_t addr)
{
    if (addr == UART_ADDR) return true;
    if (addr == RTC_ADDR || addr == RTC_ADDR + 4) return true;
    if (addr == KBD_ADDR) return true;
    if (addr == VGACTL_ADDR || addr == VGACTL_ADDR + 4) return true;
    if (addr >= FB_ADDR && addr < FB_ADDR + FB_SIZE) return true;
    return false;
}

static void difftest_check_one_instruction()
{
    if (g_touched_device) {
        // Do NOT call ref_difftest_exec here -- NEMU has no device model when
        // built for DiffTest (its own Kconfig forbids it), and would abort
        // trying to actually execute a store/load to a device address (its own
        // memory checker treats it as out-of-bounds and crashes the whole
        // process). Instead, just force REF's state to match DUT's
        // post-instruction state directly -- REF "catches up" without ever
        // being asked to interpret this particular instruction at all.
        DifftestCPUState dut_state;
        memset(&dut_state, 0, sizeof(dut_state));
        for (int i = 0; i < 16; i++) dut_state.gpr[i] = (uint32_t)get_reg_val(i);
        dut_state.pc = g_top->pc;
        ref_difftest_regcpy(&dut_state, DIFFTEST_TO_REF);
        return;
    }

    ref_difftest_exec(1);
    DifftestCPUState ref_state;
    ref_difftest_regcpy(&ref_state, DIFFTEST_TO_DUT);

    for (int i = 0; i < 16; i++) {
        uint32_t dut_val = (uint32_t)get_reg_val(i);
        if (dut_val != ref_state.gpr[i]) {
            printf("DIFFTEST MISMATCH at cycle %lld: x%d (%s)  dut=0x%08x  ref=0x%08x\n",
                   g_cycles, i, reg_names[i], dut_val, ref_state.gpr[i]);
            difftest_mismatch = true;
        }
    }
    uint32_t dut_pc = g_top->pc;
    if (dut_pc != ref_state.pc) {
        printf("DIFFTEST MISMATCH at cycle %lld: pc  dut=0x%08x  ref=0x%08x\n",
               g_cycles, dut_pc, ref_state.pc);
        difftest_mismatch = true;
    }
}

static void step_n_cycles(long long n)
{
    for (long long i = 0; i < n && !sim_finished && g_cycles < MAX_CYCLES && !difftest_mismatch && !g_wp_triggered; i++)
    {
        g_top->clk = 0; g_top->eval(); if (g_cycles < TRACE_CYCLES) g_tfp->dump(g_time++);
        // Sample here, between the edges: pc/inst/dbg_mem_* still combinationally
        // reflect the instruction actually executing THIS cycle. Reading them after
        // the next posedge (below) would instead show a preview of the NEXT
        // instruction's decode, since Verilator settles that same combinational
        // chain (including the next fetch) within the same eval() that updates pc --
        // this is exactly the bug that caused false device-touch detections before.
        g_touched_device = (g_top->dbg_mem_wen || g_top->dbg_mem_ren)
                          && is_device_addr(g_top->dbg_mem_addr);
        if (g_mtrace_enabled) {
            if (g_top->dbg_mem_wen)      mtrace_log(g_top->dbg_mem_addr, true,  g_top->dbg_mem_wdata);
            else if (g_top->dbg_mem_ren) mtrace_log(g_top->dbg_mem_addr, false, g_top->dbg_wdata);
        }
        itrace_record(g_top->pc, g_top->dbg_inst);
        uint32_t ftrace_site_pc = g_top->pc;      // this instruction's own address
        uint32_t ftrace_inst    = g_top->dbg_inst; // this instruction's raw bits
        g_top->clk = 1; g_top->eval(); if (g_cycles < TRACE_CYCLES) g_tfp->dump(g_time++);
        g_cycles++;
        if (g_ftrace_enabled) {
            uint32_t opcode = ftrace_inst & 0x7f;
            uint32_t rd     = (ftrace_inst >> 7)  & 0x1f;
            uint32_t rs1    = (ftrace_inst >> 15) & 0x1f;
            uint32_t target = g_top->pc;   // pc has now advanced -- this IS the destination
            if (opcode == 0x6f) {                // jal
                if (rd != 0) ftrace_call(ftrace_site_pc, target);
            } else if (opcode == 0x67) {         // jalr
                if (rd == 0 && rs1 == 1) ftrace_ret(ftrace_site_pc);
                else if (rd != 0) ftrace_call(ftrace_site_pc, target);
            }
        }
        if (difftest_enabled && !sim_finished) {
            difftest_check_one_instruction();
        }
        if (!sim_finished) check_watchpoints();
        if (g_cycles % 10000000 == 0) {
            fprintf(stderr, "... %lld cycles simulated\n", g_cycles);
        }
    }
}

static void report_result()
{
    if (difftest_mismatch)
    {
        printf("DIFFTEST FAILED -- DUT and REF diverged, see mismatch above (%lld cycles)\n", g_cycles);
        printf("Last %d instructions:\n", iringbuf_count);
        itrace_display_ring();
    }
    else if (g_wp_triggered)
    {
        printf("Stopped at watchpoint, %lld cycles\n", g_cycles);
    }
    else if (sim_finished) 
    {
        int a0 = g_top->dbg_a0;
        if (a0 == 0) 
            printf("HIT GOOD TRAP -- a0 = %d, simulation ended after %lld cycles\n", a0, g_cycles);
        else 
        {
            printf("HIT BAD TRAP -- a0 = %d, simulation ended after %lld cycles\n", a0, g_cycles);
            printf("Last %d instructions:\n", iringbuf_count);
            itrace_display_ring();
        }
    } 
    else if (g_cycles >= MAX_CYCLES) 
    {
        printf("HIT BAD TRAP (timeout) -- ran %lld cycles without ebreak\n", g_cycles);
        printf("Last %d instructions:\n", iringbuf_count);
        itrace_display_ring();
    }
}

static void cmd_si(const char *args)
{
    g_wp_triggered = false;   // a previous watchpoint stop shouldn't block further stepping
    long long n = 1;
    if (args && *args) n = atoll(args);
    step_n_cycles(n);
    if (iringbuf_count > 0) {
        int idx = (iringbuf_idx - 1 + IRINGBUF_SIZE) % IRINGBUF_SIZE;
        char buf[256];
        itrace_format(buf, sizeof(buf), iringbuf[idx].pc, iringbuf[idx].inst);
        printf("%s\n", buf);
    }
    if (sim_finished || g_cycles >= MAX_CYCLES || difftest_mismatch || g_wp_triggered) report_result();
}

static void cmd_info_r()
{
    printf("pc      = 0x%08x\n", g_top->pc);
    for (int i = 0; i < 16; i++) {
        printf("x%-2d (%-4s) = 0x%08x\n", i, reg_names[i], (uint32_t)get_reg_val(i));
    }
}

static void cmd_x(const char *args)
{
    if (!args || !*args) { printf("Usage: x N ADDR\n"); return; }
    char buf[256];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *n_str = strtok(buf, " ");
    char *addr_str = strtok(NULL, " ");
    if (!n_str || !addr_str) { printf("Usage: x N ADDR\n"); return; }
    int n = atoi(n_str);
    uint32_t addr = (uint32_t)strtoul(addr_str, NULL, 0);
    for (int i = 0; i < n; i++) {
        uint32_t val = (uint32_t)pmem_read((int)(addr + i * 4));
        printf("0x%08x: 0x%08x\n", addr + i * 4, val);
    }
}

static void cmd_c()
{
    g_wp_triggered = false;   // a previous watchpoint stop shouldn't block continuing
    step_n_cycles(MAX_CYCLES - g_cycles);
    report_result();
}

static void cmd_w(const char *args)
{
    if (!args || !*args) { printf("Usage: w EXPR\n"); return; }
    bool success;
    int32_t val = expr_evaluate(args, &success);
    if (!success) { printf("Bad expression\n"); return; }
    WP *wp = wp_new();
    if (!wp) return;
    strncpy(wp->expr_str, args, sizeof(wp->expr_str) - 1);
    wp->expr_str[sizeof(wp->expr_str) - 1] = 0;
    wp->old_val = val;
    printf("Watchpoint %d: %s\n", wp->NO, wp->expr_str);
}

static void cmd_d(const char *args)
{
    if (!args || !*args) { printf("Usage: d N\n"); return; }
    int no = atoi(args);
    WP *wp = wp_find(no);
    if (!wp) { printf("No watchpoint number %d\n", no); return; }
    wp_free_one(wp);
    printf("Deleted watchpoint %d\n", no);
}

static void cmd_info_w()
{
    if (!wp_head) { printf("No watchpoints.\n"); return; }
    printf("Num  What\n");
    for (WP *p = wp_head; p; p = p->next) printf("%-4d %s\n", p->NO, p->expr_str);
}

static void cmd_p(const char *args)
{
    if (!args || !*args) { printf("Usage: p EXPR\n"); return; }
    bool success;
    int32_t val = expr_evaluate(args, &success);
    if (!success) { printf("Bad expression\n"); return; }
    printf("%d (0x%08x)\n", val, (uint32_t)val);
}

static void sdb_loop()
{
    char *line;
    while ((line = readline("(npc) ")) != NULL) 
    {
        if (*line) add_history(line);
        char *cmd  = strtok(line, " ");
        char *args = strtok(NULL, "");
        if (!cmd) { free(line); continue; }

        if      (strcmp(cmd, "si") == 0) cmd_si(args);
        else if (strcmp(cmd, "info") == 0 && args && strcmp(args, "r") == 0) cmd_info_r();
        else if (strcmp(cmd, "info") == 0 && args && strcmp(args, "w") == 0) cmd_info_w();
        else if (strcmp(cmd, "x") == 0) cmd_x(args);
        else if (strcmp(cmd, "c") == 0) cmd_c();
        else if (strcmp(cmd, "iringbuf") == 0) itrace_display_ring();
        else if (strcmp(cmd, "w") == 0) cmd_w(args);
        else if (strcmp(cmd, "d") == 0) cmd_d(args);
        else if (strcmp(cmd, "p") == 0) cmd_p(args);
        else if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) { free(line); break; }
        else printf("Unknown command '%s'. Try: si [N], info r, info w, x N ADDR, c, iringbuf, w EXPR, d N, p EXPR, q\n", cmd);

        free(line);
        if (sim_finished || g_cycles >= MAX_CYCLES || difftest_mismatch) break;
    }
}

int main(int argc, char** argv) 
{
    bool interactive = false;
    const char *image_path = nullptr;
    const char *diff_so_path = nullptr;
    const char *capstone_so_path = nullptr;
    const char *ftrace_elf_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) interactive = true;
        else if (strcmp(argv[i], "--diff") == 0 && i + 1 < argc) diff_so_path = argv[++i];
        else if (strcmp(argv[i], "--itrace") == 0 && i + 1 < argc) capstone_so_path = argv[++i];
        else if (strcmp(argv[i], "--mtrace") == 0) g_mtrace_enabled = true;
        else if (strcmp(argv[i], "--ftrace") == 0 && i + 1 < argc) ftrace_elf_path = argv[++i];
        else image_path = argv[i];
    }
    if (!image_path) 
    {
        printf("usage: %s [-i] [--diff <ref.so>] [--itrace <libcapstone.so>] [--mtrace] [--ftrace <image.elf>] <image.bin>\n", argv[0]);
        return 1;
    }
    load_image(image_path);
    if (capstone_so_path) itrace_init(capstone_so_path);
    if (ftrace_elf_path)  ftrace_init(ftrace_elf_path);

    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);

    g_top = new Vminirv{contextp};

    g_tfp = new VerilatedFstC;
    g_top->trace(g_tfp, 5);
    g_tfp->open("wave.fst");

    g_top->rst = 1;
    g_top->clk = 0; g_top->eval(); g_tfp->dump(g_time++);
    g_top->clk = 1; g_top->eval(); g_tfp->dump(g_time++);
    g_top->rst = 0;

    // The register file's get_reg_val() is a DPI-C *export* (Verilog -> C++), not an
    // import -- calling it from C++ requires telling Verilator which module instance
    // to reach first, since export functions are tied to a specific hierarchical scope.
    svScope reg_scope = svGetScopeFromName("TOP.minirv.u_reg");
    if (!reg_scope) reg_scope = svGetScopeFromName("minirv.u_reg");
    if (reg_scope) {
        svSetScope(reg_scope);
    } else {
        fprintf(stderr, "WARNING: could not find DPI scope for u_reg -- 'info r' will not work\n");
    }

    if (diff_so_path) {
        difftest_load(diff_so_path);
    }
    wp_pool_init();

    if (interactive) 
    {
        sdb_loop();
    } 
    else 
    {
        // unchanged default behavior -- every existing automated test relies on this
        step_n_cycles(MAX_CYCLES);
        report_result();
    }

    g_tfp->close();
    g_top->final();
    delete g_top;
    return 0;
}
