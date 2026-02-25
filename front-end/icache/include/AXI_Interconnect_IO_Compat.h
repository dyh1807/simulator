#pragma once

#include <cstdint>

namespace axi_interconnect {

// Keep this aligned with axi-interconnect-kit read-port width.
constexpr uint8_t CACHELINE_WORDS = 8;

struct WideData256_t {
  uint32_t words[CACHELINE_WORDS]{};

  void clear() {
    for (uint8_t i = 0; i < CACHELINE_WORDS; ++i) {
      words[i] = 0;
    }
  }

  uint32_t &operator[](int idx) { return words[idx]; }
  const uint32_t &operator[](int idx) const { return words[idx]; }
};

struct ReadMasterReq_t {
  bool valid = false;
  bool ready = false;
  uint32_t addr = 0;
  uint8_t total_size = 0;
  uint8_t id = 0;
};

struct ReadMasterResp_t {
  bool valid = false;
  bool ready = false;
  WideData256_t data{};
  uint8_t id = 0;
};

struct ReadMasterPort_t {
  ReadMasterReq_t req{};
  ReadMasterResp_t resp{};
};

} // namespace axi_interconnect
