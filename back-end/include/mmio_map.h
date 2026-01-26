#pragma once
#include <config.h>
#include <cstdint>

struct MmioRange {
  uint32_t base;
  uint32_t size;
};

#ifdef MMIO_TEST_BASE
#define MMIO_RANGE_BASE MMIO_TEST_BASE
#ifndef MMIO_TEST_SIZE
#define MMIO_TEST_SIZE MMIO_SIZE
#endif
#define MMIO_RANGE_SIZE MMIO_TEST_SIZE
#else
#define MMIO_RANGE_BASE MMIO_BASE
#define MMIO_RANGE_SIZE MMIO_SIZE
#endif

static constexpr MmioRange kMmioRanges[] = {
    {MMIO_RANGE_BASE, MMIO_RANGE_SIZE},
};

static inline bool is_mmio_addr(uint32_t addr) {
  for (const auto &range : kMmioRanges) {
    if (addr >= range.base && addr < (range.base + range.size)) {
      return true;
    }
  }
  return false;
}
