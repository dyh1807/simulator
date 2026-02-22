#pragma once

#include "Csr.h"
#include "config.h"
#include <cstdint>
#include <iostream>

// 外设行为模型（当前阶段：提交时生效）。
// 这里先保持与现有语义一致，后续再下沉到 MemSubsystem。
class PeripheralModel {
public:
  Csr *csr = nullptr;
  uint32_t *memory = nullptr;

  void init() {}

  void on_mem_store_effective(uint32_t paddr, uint32_t new_val) {
    if (memory == nullptr) {
      return;
    }

    if (paddr == UART_ADDR_BASE) {
      char temp = new_val & 0xFF;
      std::cout << temp;
      memory[UART_ADDR_BASE / 4] &= 0xffffff00;
      return;
    }

    if (paddr == UART_ADDR_BASE + 1) {
      uint8_t byte_off = paddr & 0x3;
      uint8_t cmd = (new_val >> (byte_off * 8)) & 0xff;
      if (cmd == 7) {
        memory[PLIC_CLAIM_ADDR / 4] = 0xa;
        memory[UART_ADDR_BASE / 4] &= 0xfff0ffff;
      } else if (cmd == 5) {
        memory[UART_ADDR_BASE / 4] =
            (memory[UART_ADDR_BASE / 4] & 0xfff0ffff) | 0x00030000;
      }
      return;
    }

    if (paddr == PLIC_CLAIM_ADDR && ((new_val & 0x000000ff) == 0xa)) {
      memory[PLIC_CLAIM_ADDR / 4] = 0x0;
    }
  }

  void on_commit_store(uint32_t paddr, uint32_t data) {
    if (csr == nullptr || memory == nullptr) {
      return;
    }

    if (paddr == UART_ADDR_BASE + 1) {
      uint8_t cmd = data & 0xff;
      if (cmd == 7) {
        csr->CSR_RegFile_1[csr_mip] = csr->CSR_RegFile[csr_mip] | (1 << 9);
        csr->CSR_RegFile_1[csr_sip] = csr->CSR_RegFile[csr_sip] | (1 << 9);
      }
    }

    if (paddr == PLIC_CLAIM_ADDR && (data & 0x000000ff) == 0xa) {
      csr->CSR_RegFile_1[csr_mip] = csr->CSR_RegFile[csr_mip] & ~(1 << 9);
      csr->CSR_RegFile_1[csr_sip] = csr->CSR_RegFile[csr_sip] & ~(1 << 9);
    }
  }
};
