#include "front-end/icache/include/icache_module.h"
#include "front-end/icache/include/icache_module_v2.h"
#include "front-end/icache/include/icache_v1_pi_po.h"
#include "front-end/icache/include/icache_v2_pi_po.h"
#include "mmu/include/ptw_module.h"
#include "mmu/include/ptw_pi_po.h"
#include "mmu/include/tlb_module.h"
#include "mmu/include/tlb_pi_po.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>

long long sim_time = 0;

namespace {

struct ModuleStats {
  std::string name;
  size_t pi_width = 0;
  size_t po_width = 0;
  uint32_t same_input_pass = 0;
  uint32_t same_input_fail = 0;
  uint32_t cross_history_pass = 0;
  uint32_t cross_history_fail = 0;
};

void fill_random_bits(bool *bits, size_t width, std::mt19937 &rng) {
  std::uniform_int_distribution<int> dist(0, 1);
  for (size_t i = 0; i < width; ++i) {
    bits[i] = (dist(rng) != 0);
  }
}

bool equal_bits(const bool *a, const bool *b, size_t width) {
  for (size_t i = 0; i < width; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

template <typename Module, typename IOType, size_t PIW, size_t POW>
ModuleStats run_module_checks(
    const std::string &name, uint32_t samples, uint32_t warmup_cycles,
    uint32_t seed, void (*unpack_pi)(const bool *, IOType &),
    void (*pack_po)(const IOType &, bool *)) {
  ModuleStats stats;
  stats.name = name;
  stats.pi_width = PIW;
  stats.po_width = POW;

  std::mt19937 rng(seed);
  std::unique_ptr<bool[]> pi(new bool[PIW]);
  std::unique_ptr<bool[]> pi_a(new bool[PIW]);
  std::unique_ptr<bool[]> pi_b(new bool[PIW]);
  std::unique_ptr<bool[]> po1(new bool[POW]);
  std::unique_ptr<bool[]> po2(new bool[POW]);
  std::unique_ptr<bool[]> po_a(new bool[POW]);
  std::unique_ptr<bool[]> po_b(new bool[POW]);

  // Check 1: same module instance + same PI, run comb twice.
  Module same_instance;
  for (uint32_t i = 0; i < samples; ++i) {
    fill_random_bits(pi.get(), PIW, rng);
    unpack_pi(pi.get(), same_instance.io);
    same_instance.comb();
    pack_po(same_instance.io, po1.get());

    unpack_pi(pi.get(), same_instance.io);
    same_instance.comb();
    pack_po(same_instance.io, po2.get());

    if (equal_bits(po1.get(), po2.get(), POW)) {
      stats.same_input_pass++;
    } else {
      stats.same_input_fail++;
    }
  }

  // Check 2: two instances with different history, then same PI once.
  Module history_a;
  Module history_b;
  for (uint32_t i = 0; i < samples; ++i) {
    for (uint32_t w = 0; w < warmup_cycles; ++w) {
      fill_random_bits(pi_a.get(), PIW, rng);
      unpack_pi(pi_a.get(), history_a.io);
      history_a.comb();
      pack_po(history_a.io, po_a.get());
      history_a.seq();

      fill_random_bits(pi_b.get(), PIW, rng);
      unpack_pi(pi_b.get(), history_b.io);
      history_b.comb();
      pack_po(history_b.io, po_b.get());
      history_b.seq();
    }

    fill_random_bits(pi.get(), PIW, rng);
    unpack_pi(pi.get(), history_a.io);
    history_a.comb();
    pack_po(history_a.io, po_a.get());

    unpack_pi(pi.get(), history_b.io);
    history_b.comb();
    pack_po(history_b.io, po_b.get());

    if (equal_bits(po_a.get(), po_b.get(), POW)) {
      stats.cross_history_pass++;
    } else {
      stats.cross_history_fail++;
    }
  }

  return stats;
}

template <typename StatsPrinter>
void run_all_modules(uint32_t samples, uint32_t warmup_cycles, uint32_t seed,
                     StatsPrinter &&print_stats) {
  auto icache_v1 = run_module_checks<icache_module_n::ICache,
                                     icache_module_n::ICache_IO_t,
                                     icache_module_n::icache_v1_pi_po::PI_WIDTH,
                                     icache_module_n::icache_v1_pi_po::PO_WIDTH>(
      "icache_v1", samples, warmup_cycles, seed + 11,
      icache_module_n::icache_v1_pi_po::unpack_pi,
      icache_module_n::icache_v1_pi_po::pack_po);
  print_stats(icache_v1);

  auto icache_v2 = run_module_checks<icache_module_v2_n::ICacheV2,
                                     icache_module_v2_n::ICacheV2_IO_t,
                                     icache_module_v2_n::icache_v2_pi_po::PI_WIDTH,
                                     icache_module_v2_n::icache_v2_pi_po::PO_WIDTH>(
      "icache_v2", samples, warmup_cycles, seed + 23,
      icache_module_v2_n::icache_v2_pi_po::unpack_pi,
      icache_module_v2_n::icache_v2_pi_po::pack_po);
  print_stats(icache_v2);

  auto tlb = run_module_checks<tlb_module_n::TLBModule, tlb_module_n::TLB_IO_t,
                               tlb_module_n::tlb_pi_po::PI_WIDTH,
                               tlb_module_n::tlb_pi_po::PO_WIDTH>(
      "mmu_tlb", samples, warmup_cycles, seed + 37,
      tlb_module_n::tlb_pi_po::unpack_pi, tlb_module_n::tlb_pi_po::pack_po);
  print_stats(tlb);

  auto ptw = run_module_checks<ptw_module_n::PTWModule, ptw_module_n::PTW_IO_t,
                               ptw_module_n::ptw_pi_po::PI_WIDTH,
                               ptw_module_n::ptw_pi_po::PO_WIDTH>(
      "mmu_ptw", samples, warmup_cycles, seed + 41,
      ptw_module_n::ptw_pi_po::unpack_pi, ptw_module_n::ptw_pi_po::pack_po);
  print_stats(ptw);
}

} // namespace

int main(int argc, char **argv) {
  uint32_t samples = 64;
  uint32_t warmup_cycles = 8;
  uint32_t seed = 12345;
  if (argc > 1) {
    samples = static_cast<uint32_t>(std::stoul(argv[1]));
  }
  if (argc > 2) {
    warmup_cycles = static_cast<uint32_t>(std::stoul(argv[2]));
  }
  if (argc > 3) {
    seed = static_cast<uint32_t>(std::stoul(argv[3]));
  }

  run_all_modules(samples, warmup_cycles, seed, [](const ModuleStats &stats) {
    std::cout << "RESULT"
              << " module=" << stats.name << " PI_WIDTH=" << stats.pi_width
              << " PO_WIDTH=" << stats.po_width
              << " same_input_pass=" << stats.same_input_pass
              << " same_input_fail=" << stats.same_input_fail
              << " cross_history_pass=" << stats.cross_history_pass
              << " cross_history_fail=" << stats.cross_history_fail
              << std::endl;
  });
  return 0;
}
