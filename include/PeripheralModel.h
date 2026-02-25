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

  uint32_t apply_store_word(uint32_t paddr, uint32_t data, uint8_t func3) {
    uint32_t byte_off = paddr & 0x3u;
    uint32_t wstrb = 0;
    uint32_t wdata = 0;
    switch (func3 & 0x3u) {
    case 0:
      wstrb = (1u << byte_off);
      wdata = (data & 0xFFu) << (byte_off * 8);
      break;
    case 1:
      wstrb = (0x3u << byte_off);
      wdata = (data & 0xFFFFu) << (byte_off * 8);
      break;
    default:
      wstrb = 0xFu;
      wdata = data;
      break;
    }
    uint32_t wmask = 0;
    for (int i = 0; i < 4; i++) {
      if ((wstrb >> i) & 0x1u) {
        wmask |= (0xFFu << (i * 8));
      }
    }
    uint32_t old_val = memory[paddr >> 2];
    uint32_t new_val = (old_val & ~wmask) | (wdata & wmask);
    memory[paddr >> 2] = new_val;
    return new_val;
  }

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
      return;
    }
  }

  void on_commit_store(uint32_t paddr, uint32_t data, uint8_t func3) {
    if (csr == nullptr || memory == nullptr) {
      return;
    }

    bool is_mmio = ((paddr & UART_ADDR_MASK) == UART_ADDR_BASE) ||
                   ((paddr & PLIC_ADDR_MASK) == PLIC_ADDR_BASE);
    if (is_mmio) {
      (void)apply_store_word(paddr, data, func3);
      if (paddr == UART_ADDR_BASE) {
        // 与 ref 对齐：UART TX 写入后数据寄存器最低字节保持为 0。
        memory[UART_ADDR_BASE / 4] &= 0xFFFFFF00u;
      }
    }

    if (paddr == UART_ADDR_BASE + 1) {
      uint8_t cmd = data & 0xff;
      if (cmd == 7) {
        memory[PLIC_CLAIM_ADDR / 4] = 0xa;
        memory[UART_ADDR_BASE / 4] &= 0xfff0ffff;
        csr->CSR_RegFile_1[csr_mip] = csr->CSR_RegFile[csr_mip] | (1 << 9);
        csr->CSR_RegFile_1[csr_sip] = csr->CSR_RegFile[csr_sip] | (1 << 9);
      } else if (cmd == 5) {
        memory[UART_ADDR_BASE / 4] =
            (memory[UART_ADDR_BASE / 4] & 0xfff0ffff) | 0x00030000;
      }
    }

    if (paddr == UART_ADDR_BASE) {
      char temp = static_cast<char>(data & 0xFFu);
      std::cout << temp << std::flush;
    }

    if (paddr == PLIC_CLAIM_ADDR && (data & 0x000000ff) == 0xa) {
      // 与 ref 的提交语义对齐：提交该 store 即完成 claim 清除。
      memory[PLIC_CLAIM_ADDR / 4] = 0x0;
      csr->CSR_RegFile_1[csr_mip] = csr->CSR_RegFile[csr_mip] & ~(1 << 9);
      csr->CSR_RegFile_1[csr_sip] = csr->CSR_RegFile[csr_sip] & ~(1 << 9);
    }

  }
};
