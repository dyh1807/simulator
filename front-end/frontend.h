#ifndef FRONTEND_H
#define FRONTEND_H
#include "config.h"

// 是否开启2ahead
// #define ENABLE_2AHEAD

#define RESET_PC 0x00000000
#define PMEM_OFFSET RESET_PC

/* ICache model selection:
 * - default: True ICache
 * - define USE_IDEAL_ICACHE for ideal model (performance upper-bound)
 */
// #define USE_IDEAL_ICACHE

#ifndef ICACHE_MISS_LATENCY
#define ICACHE_MISS_LATENCY 100 // Latency of an icache miss in cycles
#endif

/*#define IO_version*/

// #define RAS_ENABLE  // if not defined, return address is predicted by BTB

#define DEBUG_PRINT 0
#define DEBUG_LOG(fmt, ...)                                                    \
  do {                                                                         \
    if (DEBUG_PRINT)                                                           \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)

#define DEBUG_PRINT_SMALL 0
#define DEBUG_LOG_SMALL(fmt, ...)                                              \
  do {                                                                         \
    if (DEBUG_PRINT_SMALL)                                                     \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)

#define DEBUG_PRINT_SMALL_2 0
#define DEBUG_LOG_SMALL_2(fmt, ...)                                            \
  do {                                                                         \
    if (DEBUG_PRINT_SMALL_2)                                                   \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)

#define DEBUG_PRINT_SMALL_3 0
#define DEBUG_LOG_SMALL_3(fmt, ...)                                            \
  do {                                                                         \
    if (DEBUG_PRINT_SMALL_3)                                                   \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)

#define DEBUG_PRINT_SMALL_4 0
#define DEBUG_LOG_SMALL_4(fmt, ...)                                            \
  do {                                                                         \
    if (DEBUG_PRINT_SMALL_4)                                                   \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)
#define DEBUG_PRINT_SMALL_5 0
#define DEBUG_LOG_SMALL_5(fmt, ...)                                            \
  do {                                                                         \
    if (DEBUG_PRINT_SMALL_5)                                                   \
      printf(fmt, ##__VA_ARGS__);                                              \
  } while (0)
// #define IO_GEN_MODE
/*#define MISS_MODE*/
extern int io_gen_cnt;
#endif
