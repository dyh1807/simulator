#include "front_fifo.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

namespace {

class FetchAddressFifoModel {
public:
  struct FetchAddrReadData {
    bool has_front;
    uint32_t front_data;
    uint8_t queue_size;
  };

  struct FetchAddrCombIn {
    fetch_address_FIFO_in inp;
    FetchAddrReadData rd;
  };

  struct FetchAddrCombOut {
    fetch_address_FIFO_out out_regs;
    bool clear_fifo;
    bool push_en;
    uint32_t push_data;
    bool pop_en;
  };

  struct FetchAddrDelta {
    bool clear_fifo;
    bool push_en;
    uint32_t push_data;
    bool pop_en;
  };

  void seq_read(const fetch_address_FIFO_in &inp, fetch_address_FIFO_out &out) {
    (void)inp;
    begin_cycle();
    std::memset(&out, 0, sizeof(fetch_address_FIFO_out));
    out.read_valid = false;
    out.fetch_address = 0;
    out.full = (fifo_.size() >= (FETCH_ADDR_FIFO_SIZE - 1));
    out.empty = fifo_.empty();
  }

  static void fetch_addr_comb(const FetchAddrCombIn &in, FetchAddrCombOut &out) {
    std::memset(&out, 0, sizeof(FetchAddrCombOut));

    out.out_regs.full = false;
    out.out_regs.empty = true;
    out.out_regs.read_valid = false;
    out.out_regs.fetch_address = 0;

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

    if (do_read) {
      out.out_regs.read_valid = true;
      out.out_regs.fetch_address =
          has_data_before_read ? in.rd.front_data : in.inp.fetch_address;
    }

    out.push_en = do_write;
    out.push_data = in.inp.fetch_address;
    out.pop_en = do_read;

    int next_size = static_cast<int>(queue_size_before);
    next_size += out.push_en ? 1 : 0;
    next_size -= out.pop_en ? 1 : 0;
    if (next_size < 0) {
      next_size = 0;
    }
    out.out_regs.full = (next_size >= (FETCH_ADDR_FIFO_SIZE - 1));
    out.out_regs.empty = (next_size == 0);
  }

  void comb_calc(const fetch_address_FIFO_in &inp, fetch_address_FIFO_out &out) {
    ensure_cycle_started();
    std::queue<uint32_t> virtual_fifo = fifo_;
    apply_pending_deltas(virtual_fifo);

    FetchAddrCombIn comb_in{};
    comb_in.inp = inp;
    comb_in.rd.has_front = !virtual_fifo.empty();
    comb_in.rd.front_data = comb_in.rd.has_front ? virtual_fifo.front() : 0;
    comb_in.rd.queue_size = static_cast<uint8_t>(virtual_fifo.size());

    FetchAddrCombOut comb_out{};
    fetch_addr_comb(comb_in, comb_out);
    out = comb_out.out_regs;

    pending_deltas_.push_back(
        FetchAddrDelta{comb_out.clear_fifo, comb_out.push_en, comb_out.push_data,
                       comb_out.pop_en});
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

  void clear_queue(std::queue<uint32_t> &queue_ref) {
    while (!queue_ref.empty()) {
      queue_ref.pop();
    }
  }

  void apply_delta(std::queue<uint32_t> &queue_ref, const FetchAddrDelta &delta) {
    if (delta.clear_fifo) {
      clear_queue(queue_ref);
    }
    if (delta.push_en) {
      if (queue_ref.size() >= FETCH_ADDR_FIFO_SIZE) {
        std::printf("[fetch_address_FIFO] ERROR: fifo overflow\n");
        std::exit(1);
      }
      queue_ref.push(delta.push_data);
    }
    if (delta.pop_en) {
      if (queue_ref.empty()) {
        std::printf("[fetch_address_FIFO] ERROR: fifo underflow on read\n");
        std::exit(1);
      }
      queue_ref.pop();
    }
  }

  void apply_pending_deltas(std::queue<uint32_t> &queue_ref) {
    for (const FetchAddrDelta &delta : pending_deltas_) {
      apply_delta(queue_ref, delta);
    }
  }

  std::queue<uint32_t> fifo_;
  std::vector<FetchAddrDelta> pending_deltas_;
  bool cycle_started_ = false;
};

FetchAddressFifoModel g_fetch_addr_fifo_model;

} // namespace

void fetch_address_FIFO_seq_read(struct fetch_address_FIFO_in *in,
                                 struct fetch_address_FIFO_out *out);
void fetch_address_FIFO_comb_calc(struct fetch_address_FIFO_in *in,
                                  struct fetch_address_FIFO_out *out);
void fetch_address_FIFO_seq_write();

void fetch_address_FIFO_top(struct fetch_address_FIFO_in *in,
                            struct fetch_address_FIFO_out *out) {
  assert(in);
  assert(out);
  fetch_address_FIFO_seq_read(in, out);
  fetch_address_FIFO_comb_calc(in, out);
  fetch_address_FIFO_seq_write();
}

void fetch_address_FIFO_seq_read(struct fetch_address_FIFO_in *in,
                                 struct fetch_address_FIFO_out *out) {
  assert(in);
  assert(out);
  g_fetch_addr_fifo_model.seq_read(*in, *out);
}

void fetch_address_FIFO_comb_calc(struct fetch_address_FIFO_in *in,
                                  struct fetch_address_FIFO_out *out) {
  assert(in);
  assert(out);
  g_fetch_addr_fifo_model.comb_calc(*in, *out);
}

void fetch_address_FIFO_seq_write() { g_fetch_addr_fifo_model.seq_write(); }
