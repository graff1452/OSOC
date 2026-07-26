include $(AM_HOME)/scripts/isa/riscv.mk
include $(AM_HOME)/scripts/platform/npc.mk

CFLAGS  += -DISA_H=\"riscv/riscv.h\"

# platform/npc.mk defaults NPC_SIM to the minirv-rtl binary -- override it here so
# riscv32ecustom-npc uses the separate npc/riscv32ecustom/ simulator instead, without touching
# minirv-npc's own default at all.
NPC_SIM := $(AM_HOME)/../npc/riscv32ecustom/build/Vminirv

COMMON_CFLAGS += -march=rv32e_zicsr -mabi=ilp32e  # overwrite
LDFLAGS       += -melf32lriscv                    # overwrite

AM_SRCS += riscv/npc/libgcc/div.S \
           riscv/npc/libgcc/muldi3.S \
           riscv/npc/libgcc/multi3.c \
           riscv/npc/libgcc/ashldi3.c \
           riscv/npc/libgcc/unused.c