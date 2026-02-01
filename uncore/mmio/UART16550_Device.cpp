/**
 * @file UART16550_Device.cpp
 * @brief Minimal 16550-like UART MMIO device (TX-only).
 */

#include "UART16550_Device.h"
#include <cstring>
#include <iostream>

namespace mmio {

static constexpr uint32_t UART_REG_THR = 0x0;
static constexpr uint32_t UART_REG_LSR = 0x5;

static constexpr uint8_t UART_LSR_THRE = 0x20; // Transmit-hold-register empty
static constexpr uint8_t UART_LSR_TEMT = 0x40; // Transmitter empty

UART16550_Device::UART16550_Device(uint32_t base_addr) : base(base_addr) {}

void UART16550_Device::read(uint32_t addr, uint8_t *data, uint32_t len) {
  if (!data || len == 0) {
    return;
  }

  std::memset(data, 0, len);

  const uint32_t off = addr - base;

  // Report "always ready to transmit" to avoid software deadlock.
  if (off == UART_REG_LSR) {
    data[0] = static_cast<uint8_t>(UART_LSR_THRE | UART_LSR_TEMT);
  }
}

void UART16550_Device::write(uint32_t addr, const uint8_t *data, uint32_t len,
                             uint32_t wstrb) {
  if (!data || len == 0) {
    return;
  }

  const uint32_t off0 = addr - base;

  // Handle byte-lane writes; THR is 8-bit at offset 0.
  for (uint32_t i = 0; i < len && i < 32; i++) {
    if (((wstrb >> i) & 1u) == 0) {
      continue;
    }
    const uint32_t off = off0 + i;
    if (off == UART_REG_THR) {
      const uint8_t ch = data[i];
      // Keep legacy behavior: do not print ESC (27).
      if (ch != 27) {
        std::cout.put(static_cast<char>(ch));
      }
    }
  }
}

} // namespace mmio

