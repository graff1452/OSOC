#ifndef NPC_H__
#define NPC_H__

#include <klib-macros.h>
#include ISA_H   // pulls in riscv/riscv.h, which declares outb() among other things

// Fake devices implemented in npc/minirv-rtl/csrc/main.cpp's pmem_read/pmem_write.
// Addresses deliberately match NEMU's own (SERIAL_PORT / RTC_ADDR) — same numbers,
// different backend.
//
// IMPORTANT: main.cpp (compiled by the host g++) and this file (compiled by
// minirv-gcc, targeting the RISC-V guest) are two completely separate compiled
// worlds -- neither can #include the other. These numbers are duplicated on
// purpose and must be kept in sync BY HAND with the matching #defines in
// main.cpp -- there is no compiler check that will catch a mismatch.
#define UART_ADDR   0xa00003f8
#define RTC_ADDR    0xa0000048
#define KBD_ADDR    0xa0000060
#define VGACTL_ADDR 0xa0000100   // word0 (read): (width<<16)|height. word0+4 (write): sync trigger
#define FB_ADDR     0xa1000000
#define SCREEN_W    400
#define SCREEN_H    300

#endif
