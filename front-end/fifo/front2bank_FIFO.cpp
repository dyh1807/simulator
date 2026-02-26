#include "front_fifo.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

namespace {

constexpr uint32_t bit_mask_u32(int bits) {
  return (bits >= 32) ? 0xffffffffu : ((1u << bits) - 1u);
}

struct Front2BackEntry {
  std::array<uint32_t, FETCH_WIDTH> fetch_group;
  std::array<bool, FETCH_WIDTH> page_fault_inst;
  std::array<bool, FETCH_WIDTH> inst_valid;
  std::array<bool, FETCH_WIDTH> predict_dir_corrected;
  uint32_t predict_next_fetch_address_corrected;
  std::array<uint32_t, FETCH_WIDTH> predict_base_pc;
  std::array<bool, FETCH_WIDTH> alt_pred;
  std::array<uint8_t, FETCH_WIDTH> altpcpn;
  std::array<uint8_t, FETCH_WIDTH> pcpn;
  uint32_t tage_idx[FETCH_WIDTH][TN_MAX];
  uint32_t tage_tag[FETCH_WIDTH][TN_MAX];
};

class Front2BackFifoModel {
public:
  struct Front2BackReadData {
    bool has_front;
    uint8_t queue_size;
    Front2BackEntry front_entry;
  };

  struct Front2BackCombIn {
    front2back_FIFO_in inp;
    Front2BackReadData rd;
  };

  struct Front2BackCombOut {
    front2back_FIFO_out out_regs;
    bool clear_fifo;
    bool push_en;
    Front2BackEntry push_entry;
    bool pop_en;
  };

  struct Front2BackDelta {
    bool clear_fifo;
    bool push_en;
    Front2BackEntry push_entry;
    bool pop_en;
  };

  void seq_read(const front2back_FIFO_in &inp, front2back_FIFO_out &out) {
    (void)inp;
    begin_cycle();
    std::memset(&out, 0, sizeof(front2back_FIFO_out));
    out.full = (fifo_.size() == FRONT2BACK_FIFO_SIZE);
    out.empty = fifo_.empty();
    out.front2back_FIFO_valid = false;
  }

  static void front2back_comb(const Front2BackCombIn &in, Front2BackCombOut &out) {
    std::memset(&out, 0, sizeof(Front2BackCombOut));
    out.out_regs.full = false;
    out.out_regs.empty = true;
    out.out_regs.front2back_FIFO_valid = false;

    if (in.inp.reset) {
      out.clear_fifo = true;
      return;
    }

    bool has_data_before_read = in.rd.has_front;
    uint8_t queue_size_before = in.rd.queue_size;
    if (in.inp.refetch) {
      out.clear_fifo = true;
      has_data_before_read = false;
      queue_size_before = 0;
    }

    const bool do_write = in.inp.write_enable;
    const bool do_read = in.inp.read_enable && (has_data_before_read || do_write);

    Front2BackEntry write_entry{};
    if (do_write) {
      constexpr uint32_t kPcpnMask = bit_mask_u32(pcpn_t_BITS);
      constexpr uint32_t kTageIdxMask = bit_mask_u32(tage_idx_t_BITS);
      constexpr uint32_t kTageTagMask = bit_mask_u32(tage_tag_t_BITS);
      for (int i = 0; i < FETCH_WIDTH; i++) {
        write_entry.fetch_group[i] = in.inp.fetch_group[i];
        write_entry.page_fault_inst[i] = in.inp.page_fault_inst[i];
        write_entry.inst_valid[i] = in.inp.inst_valid[i];
        write_entry.predict_dir_corrected[i] = in.inp.predict_dir_corrected[i];
        write_entry.predict_base_pc[i] = in.inp.predict_base_pc[i];
        write_entry.alt_pred[i] = in.inp.alt_pred[i];
        write_entry.altpcpn[i] =
            static_cast<uint8_t>(in.inp.altpcpn[i] & kPcpnMask);
        write_entry.pcpn[i] = static_cast<uint8_t>(in.inp.pcpn[i] & kPcpnMask);
        for (int j = 0; j < TN_MAX; j++) {
          write_entry.tage_idx[i][j] = in.inp.tage_idx[i][j] & kTageIdxMask;
          write_entry.tage_tag[i][j] = in.inp.tage_tag[i][j] & kTageTagMask;
        }
      }
      write_entry.predict_next_fetch_address_corrected =
          in.inp.predict_next_fetch_address_corrected;
    }

    if (do_read) {
      constexpr uint32_t kPcpnMask = bit_mask_u32(pcpn_t_BITS);
      constexpr uint32_t kTageIdxMask = bit_mask_u32(tage_idx_t_BITS);
      constexpr uint32_t kTageTagMask = bit_mask_u32(tage_tag_t_BITS);
      const Front2BackEntry &read_entry =
          has_data_before_read ? in.rd.front_entry : write_entry;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        out.out_regs.fetch_group[i] = read_entry.fetch_group[i];
        out.out_regs.page_fault_inst[i] = read_entry.page_fault_inst[i];
        out.out_regs.inst_valid[i] = read_entry.inst_valid[i];
        out.out_regs.predict_dir_corrected[i] = read_entry.predict_dir_corrected[i];
        out.out_regs.predict_base_pc[i] = read_entry.predict_base_pc[i];
        out.out_regs.alt_pred[i] = read_entry.alt_pred[i];
        out.out_regs.altpcpn[i] =
            static_cast<pcpn_t>(read_entry.altpcpn[i] & kPcpnMask);
        out.out_regs.pcpn[i] = static_cast<pcpn_t>(read_entry.pcpn[i] & kPcpnMask);
        for (int j = 0; j < TN_MAX; j++) {
          out.out_regs.tage_idx[i][j] =
              static_cast<tage_idx_t>(read_entry.tage_idx[i][j] & kTageIdxMask);
          out.out_regs.tage_tag[i][j] =
              static_cast<tage_tag_t>(read_entry.tage_tag[i][j] & kTageTagMask);
        }
      }
      out.out_regs.predict_next_fetch_address_corrected =
          read_entry.predict_next_fetch_address_corrected;
      out.out_regs.front2back_FIFO_valid = true;
    }

    out.push_en = do_write;
    out.push_entry = write_entry;
    out.pop_en = do_read;

    int next_size = static_cast<int>(queue_size_before);
    next_size += out.push_en ? 1 : 0;
    next_size -= out.pop_en ? 1 : 0;
    if (next_size < 0) {
      next_size = 0;
    }
    out.out_regs.full = (next_size == FRONT2BACK_FIFO_SIZE);
    out.out_regs.empty = (next_size == 0);
  }

  void comb_calc(const front2back_FIFO_in &inp, front2back_FIFO_out &out) {
    ensure_cycle_started();
    std::queue<Front2BackEntry> virtual_fifo = fifo_;
    apply_pending_deltas(virtual_fifo);

    Front2BackCombIn comb_in{};
    comb_in.inp = inp;
    comb_in.rd.has_front = !virtual_fifo.empty();
    comb_in.rd.queue_size = static_cast<uint8_t>(virtual_fifo.size());
    if (comb_in.rd.has_front) {
      comb_in.rd.front_entry = virtual_fifo.front();
    } else {
      std::memset(&comb_in.rd.front_entry, 0, sizeof(Front2BackEntry));
    }

    Front2BackCombOut comb_out{};
    front2back_comb(comb_in, comb_out);
    out = comb_out.out_regs;
    pending_deltas_.push_back(
        Front2BackDelta{comb_out.clear_fifo, comb_out.push_en,
                        comb_out.push_entry, comb_out.pop_en});
  }

  void seq_write() {
    if (!cycle_started_) {
      return;
    }
    apply_pending_deltas(fifo_);
    pending_deltas_.clear();
    cycle_started_ = false;
  }

private:
  void begin_cycle() {
    pending_deltas_.clear();
    cycle_started_ = true;
  }

  void ensure_cycle_started() {
    if (cycle_started_) {
      return;
    }
    begin_cycle();
  }

  void clear_queue(std::queue<Front2BackEntry> &queue_ref) {
    while (!queue_ref.empty()) {
      queue_ref.pop();
    }
  }

  void apply_delta(std::queue<Front2BackEntry> &queue_ref,
                   const Front2BackDelta &delta) {
    if (delta.clear_fifo) {
      clear_queue(queue_ref);
    }
    if (delta.push_en) {
      if (queue_ref.size() >= FRONT2BACK_FIFO_SIZE) {
        std::printf("[FRONT2BACK_FIFO_TOP] ERROR!!: front2back_fifo.size() >= FRONT2BACK_FIFO_SIZE\n");
        std::exit(1);
      }
      queue_ref.push(delta.push_entry);
    }
    if (delta.pop_en) {
      if (queue_ref.empty()) {
        std::printf("[FRONT2BACK_FIFO_TOP] ERROR!!: front2back_fifo underflow on read\n");
        std::exit(1);
      }
      queue_ref.pop();
    }
  }

  void apply_pending_deltas(std::queue<Front2BackEntry> &queue_ref) {
    for (const Front2BackDelta &delta : pending_deltas_) {
      apply_delta(queue_ref, delta);
    }
  }

  std::queue<Front2BackEntry> fifo_;
  std::vector<Front2BackDelta> pending_deltas_;
  bool cycle_started_ = false;
};

Front2BackFifoModel g_front2back_fifo_model;

} // namespace

void front2back_FIFO_seq_read(struct front2back_FIFO_in *in,
                              struct front2back_FIFO_out *out);
void front2back_FIFO_comb_calc(struct front2back_FIFO_in *in,
                               struct front2back_FIFO_out *out);
void front2back_FIFO_seq_write();

void front2back_FIFO_top(struct front2back_FIFO_in *in,
                         struct front2back_FIFO_out *out) {
  assert(in);
  assert(out);
  front2back_FIFO_seq_read(in, out);
  front2back_FIFO_comb_calc(in, out);
  front2back_FIFO_seq_write();
}

void front2back_FIFO_seq_read(struct front2back_FIFO_in *in,
                              struct front2back_FIFO_out *out) {
  assert(in);
  assert(out);
  g_front2back_fifo_model.seq_read(*in, *out);
}

void front2back_FIFO_comb_calc(struct front2back_FIFO_in *in,
                               struct front2back_FIFO_out *out) {
  assert(in);
  assert(out);
  g_front2back_fifo_model.comb_calc(*in, *out);
}

void front2back_FIFO_seq_write() { g_front2back_fifo_model.seq_write(); }
