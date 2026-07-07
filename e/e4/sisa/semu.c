#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint8_t PC = 0;
uint8_t R[4];
uint8_t M[16];

void inst_cycle()
{
    uint8_t inst = M[PC];
    uint8_t opcode  = (inst >> 6)   & 0b11;

    if (opcode == 0b00) //ADD
    {
        uint8_t rd = (inst >> 4) & 0b11;
        uint8_t rs1 = (inst >> 2) & 0b11;
        uint8_t rs2 = inst & 0b11;
        R[rd] = R[rs1] + R[rs2];
        PC++;
    }
    else if (opcode == 0b10) //LI
    {
        uint8_t rd = (inst >> 4) & 0b11;
        uint8_t imm = inst & 0b1111;
        R[rd] = imm;
        PC++;
    }
    else if (opcode == 0b11) //BNER0
    {
        uint8_t addr = (inst >> 2) & 0b1111;
        uint8_t rs2 = inst & 0b11;
        if (R[0] != R[rs2])
        {
            PC = addr;
        }
        else
        {
            PC++;
        }
    }
    else if (opcode == 0b01) { // OUT (printf)
        uint8_t rs = (inst >> 4) & 0b11;
        printf("%d\n", R[rs]);
        PC++;
    }
}

int main(int argc, char *argv[]) {
  M[0]  = 0xB1;
  M[1]  = 0x17;
  M[2]  = 0x29;
  M[3]  = 0xC5;
  M[4]  = 0x60;
  M[5]  = 0x3F;
  M[6]  = 0x3F;
  M[7]  = 0x3F;
  M[8]  = 0x3F;
  M[9]  = 0x3F;
  M[10] = 0xEB;

  R[0] = atoi(argv[1]);

  for (int i = 0; i < 100; i++) {
    inst_cycle();
  }

  return 0;
}