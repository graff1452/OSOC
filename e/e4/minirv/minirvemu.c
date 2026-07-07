#include <stdio.h>
#include <stdint.h>

uint32_t PC = 0;
uint32_t R[32];
uint8_t M[1024];

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

    if (opcode == 0b0010011 && funct3 == 0b000) // ADDI 
    {
        
    }
}