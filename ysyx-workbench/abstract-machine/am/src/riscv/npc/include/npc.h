#ifndef NPC_H__
#define NPC_H__

#include <klib-macros.h>
#include ISA_H   // pulls in riscv/riscv.h, which declares outb() among other things

// Fake devices implemented in npc/minirv-rtl/csrc/main.cpp's pmem_read/pmem_write.
// Addresses deliberately match NEMU's own (SERIAL_PORT / RTC_ADDR) — same numbers,
// different backend.
#define UART_ADDR 0xa00003f8
#define RTC_ADDR  0xa0000048

#endif
