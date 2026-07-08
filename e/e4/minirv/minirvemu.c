#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t PC = 0;
uint32_t R[32];
uint8_t M[1024 * 1024]; // 1 MB memory
int halted = 0;

void load_program(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    perror("fopen");
    exit(1);
  }
  size_t n = fread(M, 1, sizeof(M), fp);
  printf("Loaded %zu bytes from %s\n", n, filename);
  fclose(fp);
}

void inst_cycle()
{
    uint32_t inst = M[PC] | (M[PC+1] << 8) | (M[PC+2] << 16) | (M[PC+3] << 24);

    uint32_t opcode = inst & 0b1111111;          // bits [6:0],  7 bits
    uint32_t rd     = (inst >> 7)  & 0b11111;  // bits [11:7], 5 bits
    uint32_t funct3 = (inst >> 12) & 0b111;   // bits [14:12],3 bits
    uint32_t rs1    = (inst >> 15) & 0b11111;  // bits [19:15],5 bits
    uint32_t rs2    = (inst >> 20) & 0b11111;  // bits [24:20],5 bits (R-type/S-type only)
    uint32_t funct7 = (inst >> 25) & 0b1111111; // bits [31:25],7 bits (R-type only)
    uint32_t imm_i = (int32_t)(inst) >> 20; // bits [31:20],12 bits (I-type only)
    uint32_t imm_s = ((int32_t)(inst) >> 25 << 5) | ((inst >> 7) & 0b11111); // bits [31:25] and [11:7],12 bits (S-type only)
    uint32_t imm_u = inst & 0xFFFFF000; // bits [31:12],20 bits (U-type only)

    if (opcode == 0b0110011 && funct3 == 0b000) // ADD, R-type
    {
        R[rd] = R[rs1] + R[rs2];
        PC += 4;
    }
    else if (opcode == 0b0010011 && funct3 == 0b000) // ADDI, I-type
    {
        R[rd] = R[rs1] + imm_i;
        PC += 4;
    }
    else if (opcode == 0b0110111) // LUI, U-type
    {
        R[rd] = imm_u;
        PC += 4;
    }
    else if (opcode == 0b1100111 && funct3 == 0b000) // JALR, I-type
    {
        uint32_t target = R[rs1] + imm_i;
        R[rd] = PC + 4;                      
        PC = target;
    }
    else if (opcode == 0b0000011 && funct3 == 0b010) // LW, I-type
    {
        R[rd] = M[R[rs1] + imm_i] | (M[R[rs1] + imm_i + 1] << 8) | (M[R[rs1] + imm_i + 2] << 16) | (M[R[rs1] + imm_i + 3] << 24);
        PC += 4;
    }
    else if (opcode == 0b0000011 && funct3 == 0b100) // LBU, I-type
    {
        R[rd] = M[R[rs1] + imm_i];
        PC += 4;
    }
    else if (opcode == 0b0100011 && funct3 == 0b010) // SW, S-type
    {
        M[R[rs1] + imm_s] = R[rs2] & 0xFF;
        M[R[rs1] + imm_s + 1] = (R[rs2] >> 8) & 0xFF;
        M[R[rs1] + imm_s + 2] = (R[rs2] >> 16) & 0xFF;
        M[R[rs1] + imm_s + 3] = (R[rs2] >> 24) & 0xFF;
        PC += 4;
    }
    else if (opcode == 0b0100011 && funct3 == 0b000) // SB, S-type
    {
        M[R[rs1] + imm_s] = R[rs2] & 0xFF;
        PC += 4;
    }
    else if (inst == 0x00100073) // EBREAK
    {
        if (R[10] == 0) {
            printf("HIT GOOD TRAP\n");
        } else {
            printf("HIT BAD TRAP, a0 = %d\n", (int32_t)R[10]);
        }
        halted = 1;   // tell main() the program is done
    }
    R[0] = 0; // x0 is always zero
}

int main() {
  load_program("sum.bin");

  uint32_t halt_addr = 0x224;
  M[halt_addr + 0] = 0x73;
  M[halt_addr + 1] = 0x00;
  M[halt_addr + 2] = 0x10;
  M[halt_addr + 3] = 0x00;

  long max_cycles = 10000000;
  long i;
  for (i = 0; i < max_cycles && !halted; i++) {
    inst_cycle();
  }

  if (!halted) {
    printf("Did not halt within %ld cycles — PC = 0x%x\n", max_cycles, PC);
  } else {
    printf("Halted after %ld cycles.\n", i);
  }

  return 0;
}