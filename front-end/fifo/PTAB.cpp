#include "front_fifo.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

#include "frontend.h"

namespace {

constexpr uint32_t bit_mask_u32(int bits) {
  return (bits >= 32) ? 0xffffffffu : ((1u << bits) - 1u);
}

struct PTABEntry {
  bool predict_dir[FETCH_WIDTH];
  uint32_t predict_next_fetch_address;
  uint32_t predict_base_pc[FETCH_WIDTH];
  bool alt_pred[FETCH_WIDTH];
  uint8_t altpcpn[FETCH_WIDTH];
  uint8_t pcpn[FETCH_WIDTH];
  uint32_t tage_idx[FETCH_WIDTH][TN_MAX];
  uint32_t tage_tag[FETCH_WIDTH][TN_MAX];
  bool need_mini_flush;
  bool dummy_entry;
};

class PTABModel {
public:
  struct PtabReadData {
    bool has_front;
    uint8_t queue_size;
    PTABEntry front_entry;
  };

  struct PtabCombIn {
    PTAB_in inp;
    PtabReadData rd;
  };

  struct PtabCombOut {
    PTAB_out out_regs;
    bool clear_ptab;
    bool push_write_en;
    PTABEntry push_write_entry;
    bool push_dummy_en;
    PTABEntry push_dummy_entry;
    bool pop_en;
  };

  struct PtabDelta {
    bool clear_ptab;
    bool push_write_en;
    PTABEntry push_write_entry;
    bool push_dummy_en;
    PTABEntry push_dummy_entry;
    bool pop_en;
  };

  void seq_read(const PTAB_in &inp, PTAB_out &out) {
    (void)inp;
    begin_cycle();
    std::memset(&out, 0, sizeof(PTAB_out));
    out.full = (ptab_.size() >= (PTAB_SIZE - 1));
    out.empty = ptab_.empty();
    out.dummy_entry = false;
  }

  static void ptab_comb(const PtabCombIn &in, PtabCombOut &out) {
    std::memset(&out, 0, sizeof(PtabCombOut));
    out.out_regs.full = false;
    out.out_regs.empty = true;
    out.out_regs.dummy_entry = false;

    if (in.inp.reset) {
      out.clear_ptab = true;
      return;
    }

    bool has_data_before_read = in.rd.has_front;
    uint8_t queue_size_before = in.rd.queue_size;
    if (in.inp.refetch) {
      out.clear_ptab = true;
      has_data_before_read = false;
      queue_size_before = 0;
    }

    const bool do_write = in.inp.write_enable;
    const bool push_dummy = in.inp.write_enable && in.inp.need_mini_flush;
    const bool do_read = in.inp.read_enable && (has_data_before_read || do_write || push_dummy);

    PTABEntry write_entry{};
    if (do_write) {
      constexpr uint32_t kPcpnMask = bit_mask_u32(pcpn_t_BITS);
      constexpr uint32_t kTageIdxMask = bit_mask_u32(tage_idx_t_BITS);
      constexpr uint32_t kTageTagMask = bit_mask_u32(tage_tag_t_BITS);
      for (int i = 0; i < FETCH_WIDTH; i++) {
        write_entry.predict_dir[i] = in.inp.predict_dir[i];
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
      write_entry.predict_next_fetch_address = in.inp.predict_next_fetch_address;
      write_entry.need_mini_flush = in.inp.need_mini_flush;
      write_entry.dummy_entry = false;
    }

    PTABEntry dummy_entry{};
    if (push_dummy) {
      std::memset(&dummy_entry, 0, sizeof(dummy_entry));
      dummy_entry.dummy_entry = true;
    }

    if (do_read) {
      constexpr uint32_t kPcpnMask = bit_mask_u32(pcpn_t_BITS);
      constexpr uint32_t kTageIdxMask = bit_mask_u32(tage_idx_t_BITS);
      constexpr uint32_t kTageTagMask = bit_mask_u32(tage_tag_t_BITS);
      const PTABEntry &read_entry =
          has_data_before_read ? in.rd.front_entry : write_entry;
      out.out_regs.dummy_entry = read_entry.dummy_entry;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        out.out_regs.predict_dir[i] = read_entry.predict_dir[i];
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
      out.out_regs.predict_next_fetch_address = read_entry.predict_next_fetch_address;
    }

    out.push_write_en = do_write;
    out.push_write_entry = write_entry;
    out.push_dummy_en = push_dummy;
    out.push_dummy_entry = dummy_entry;
    out.pop_en = do_read;

    int next_size = static_cast<int>(queue_size_before);
    next_size += out.push_write_en ? 1 : 0;
    next_size += out.push_dummy_en ? 1 : 0;
    next_size -= out.pop_en ? 1 : 0;
    if (next_size < 0) {
      next_size = 0;
    }
    out.out_regs.full = (next_size >= (PTAB_SIZE - 1));
    out.out_regs.empty = (next_size == 0);
  }

  void comb_calc(const PTAB_in &inp, PTAB_out &out) {
    ensure_cycle_started();
    std::queue<PTABEntry> virtual_ptab = ptab_;
    apply_pending_deltas(virtual_ptab);

    PtabCombIn comb_in{};
    comb_in.inp = inp;
    comb_in.rd.has_front = !virtual_ptab.empty();
    comb_in.rd.queue_size = static_cast<uint8_t>(virtual_ptab.size());
    if (comb_in.rd.has_front) {
      comb_in.rd.front_entry = virtual_ptab.front();
    } else {
      std::memset(&comb_in.rd.front_entry, 0, sizeof(PTABEntry));
    }

    PtabCombOut comb_out{};
    ptab_comb(comb_in, comb_out);
    out = comb_out.out_regs;
    pending_deltas_.push_back(
        PtabDelta{comb_out.clear_ptab, comb_out.push_write_en,
                  comb_out.push_write_entry, comb_out.push_dummy_en,
                  comb_out.push_dummy_entry, comb_out.pop_en});
  }

  void seq_write() {
    if (!cycle_started_) {
      return;
    }
    apply_pending_deltas(ptab_);
    pending_deltas_.clear();
    cycle_started_ = false;
  }

  bool peek_mini_flush() {
    if (ptab_.empty()) {
      return false;
    }
    DEBUG_LOG_SMALL_4("ptab_peeking for pc=%x,need_mini_flush=%d\n",
                      ptab_.front().predict_base_pc[0], ptab_.front().need_mini_flush);
    return ptab_.front().need_mini_flush;
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

  void clear_queue(std::queue<PTABEntry> &queue_ref) {
    while (!queue_ref.empty()) {
      queue_ref.pop();
    }
  }

  void apply_delta(std::queue<PTABEntry> &queue_ref, const PtabDelta &delta) {
    if (delta.clear_ptab) {
      clear_queue(queue_ref);
    }
    if (delta.push_write_en) {
      if (queue_ref.size() >= PTAB_SIZE) {
        std::printf("[PTAB_TOP] ERROR!!: ptab.size() >= PTAB_SIZE\n");
        std::exit(1);
      }
      queue_ref.push(delta.push_write_entry);
    }
    if (delta.push_dummy_en) {
      if (queue_ref.size() >= PTAB_SIZE) {
        std::printf("[PTAB_TOP] dummy entry push ERROR!!: ptab.size() >= PTAB_SIZE\n");
        std::exit(1);
      }
      queue_ref.push(delta.push_dummy_entry);
    }
    if (delta.pop_en) {
      if (queue_ref.empty()) {
        std::printf("[PTAB_TOP] ERROR!!: ptab underflow on read\n");
        std::exit(1);
      }
      queue_ref.pop();
    }
  }

  void apply_pending_deltas(std::queue<PTABEntry> &queue_ref) {
    for (const PtabDelta &delta : pending_deltas_) {
      apply_delta(queue_ref, delta);
    }
  }

  std::queue<PTABEntry> ptab_;
  std::vector<PtabDelta> pending_deltas_;
  bool cycle_started_ = false;
};

PTABModel g_ptab_model;

} // namespace

bool ptab_peek_mini_flush() { return g_ptab_model.peek_mini_flush(); }

void PTAB_seq_read(struct PTAB_in *in, struct PTAB_out *out);
void PTAB_comb_calc(struct PTAB_in *in, struct PTAB_out *out);
void PTAB_seq_write();

void PTAB_top(struct PTAB_in *in, struct PTAB_out *out) {
  assert(in);
  assert(out);
  PTAB_seq_read(in, out);
  PTAB_comb_calc(in, out);
  PTAB_seq_write();
}

void PTAB_seq_read(struct PTAB_in *in, struct PTAB_out *out) {
  assert(in);
  assert(out);
  g_ptab_model.seq_read(*in, *out);
}

void PTAB_comb_calc(struct PTAB_in *in, struct PTAB_out *out) {
  assert(in);
  assert(out);
  g_ptab_model.comb_calc(*in, *out);
}

void PTAB_seq_write() { g_ptab_model.seq_write(); }
