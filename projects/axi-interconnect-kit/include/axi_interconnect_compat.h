#pragma once

#include <cstdint>

// Minimal signal aliases used by AXI interconnect/uncore modules.
using wire1_t = bool;
using wire2_t = uint8_t;
using wire3_t = uint8_t;
using wire4_t = uint8_t;
using wire5_t = uint8_t;
using wire6_t = uint8_t;
using wire7_t = uint8_t;
using wire8_t = uint8_t;
using wire9_t = uint16_t;
using wire10_t = uint16_t;
using wire11_t = uint16_t;
using wire12_t = uint16_t;
using wire13_t = uint16_t;
using wire14_t = uint16_t;
using wire15_t = uint16_t;
using wire16_t = uint16_t;
using wire17_t = uint32_t;
using wire18_t = uint32_t;
using wire19_t = uint32_t;
using wire20_t = uint32_t;
using wire21_t = uint32_t;
using wire22_t = uint32_t;
using wire23_t = uint32_t;
using wire24_t = uint32_t;
using wire25_t = uint32_t;
using wire26_t = uint32_t;
using wire27_t = uint32_t;
using wire28_t = uint32_t;
using wire29_t = uint32_t;
using wire30_t = uint32_t;
using wire31_t = uint32_t;
using wire32_t = uint32_t;
using wire64_t = uint64_t;

// Configurable defaults for standalone build (can be overridden via -D).
#ifndef AXI_KIT_DDR_LATENCY
#define AXI_KIT_DDR_LATENCY 100
#endif

#ifndef AXI_KIT_DEBUG
#define AXI_KIT_DEBUG 0
#endif

#ifndef AXI_KIT_DCACHE_LOG
#define AXI_KIT_DCACHE_LOG 0
#endif

#ifndef AXI_KIT_UART_BASE
#define AXI_KIT_UART_BASE 0x10000000u
#endif

#ifndef AXI_KIT_MMIO_BASE
#ifdef MMIO_TEST_BASE
#define AXI_KIT_MMIO_BASE MMIO_TEST_BASE
#else
#define AXI_KIT_MMIO_BASE AXI_KIT_UART_BASE
#endif
#endif

#ifndef AXI_KIT_MMIO_SIZE
#ifdef MMIO_TEST_SIZE
#define AXI_KIT_MMIO_SIZE MMIO_TEST_SIZE
#else
#define AXI_KIT_MMIO_SIZE 0x00001000u
#endif
#endif

// Legacy compatibility macros used by extracted code.
#ifndef ICACHE_MISS_LATENCY
#define ICACHE_MISS_LATENCY AXI_KIT_DDR_LATENCY
#endif

#ifndef DEBUG
#define DEBUG AXI_KIT_DEBUG
#endif

#ifndef DCACHE_LOG
#define DCACHE_LOG AXI_KIT_DCACHE_LOG
#endif

#ifndef UART_BASE
#define UART_BASE AXI_KIT_UART_BASE
#endif

#ifndef MMIO_BASE
#define MMIO_BASE AXI_KIT_MMIO_BASE
#endif

#ifndef MMIO_SIZE
#define MMIO_SIZE AXI_KIT_MMIO_SIZE
#endif
