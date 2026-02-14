#include "predecode.h"
#include <cstdint>
#include "RISCV.h"

static inline uint32_t sign_extend(uint32_t val, int bits) {
  const uint32_t mask = 1u << (bits - 1);
  return (val ^ mask) - mask;
}

PredecodeResult predecode_instruction(uint32_t inst, uint32_t pc) {
  PredecodeResult result;
  result.type = PREDECODE_NON_BRANCH;
  result.target_address = 0;

  if (inst == 0 || inst == INST_NOP) {
    result.type = PREDECODE_NON_BRANCH;
    result.target_address = 0;
    return result;
  }

  uint32_t number_op_code_unsigned = inst & 0x7f;

  switch (number_op_code_unsigned) {
    case number_2_opcode_jal: { // jal
      result.type = PREDECODE_JAL;

      uint32_t imm20 = (inst >> 31) & 0x1;
      uint32_t imm10_1 = (inst >> 21) & 0x3ff;
      uint32_t imm11 = (inst >> 20) & 0x1;
      uint32_t imm19_12 = (inst >> 12) & 0xff;
      uint32_t imm_raw =
          (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
      uint32_t imm = sign_extend(imm_raw, 21);

      result.target_address = pc + imm;
      break;
    }
    
    case number_4_opcode_beq: { // beq, bne, blt, bge, bltu, bgeu
      result.type = PREDECODE_DIRECT_JUMP_NO_JAL;

      uint32_t imm12 = (inst >> 31) & 0x1;
      uint32_t imm10_5 = (inst >> 25) & 0x3f;
      uint32_t imm4_1 = (inst >> 8) & 0xf;
      uint32_t imm11 = (inst >> 7) & 0x1;
      uint32_t imm_raw =
          (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
      uint32_t imm = sign_extend(imm_raw, 13);

      result.target_address = pc + imm;
      break;
    }
    
    case number_3_opcode_jalr: { // jalr
      // cout << "pc" << std::hex << pc << " inst" << std::hex << inst << endl;
      result.type = PREDECODE_JALR;
      result.target_address = 0;
      break;
    }
    
    default: {
      result.type = PREDECODE_NON_BRANCH;
      result.target_address = 0;
      break;
    }
  }

  return result;
}
