#include "front_fifo.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

#include "frontend.h"

namespace {

struct FIFOEntry {
  std::array<uint32_t, FETCH_WIDTH> instructions;
  std::array<uint32_t, FETCH_WIDTH> pc;
  std::array<bool, FETCH_WIDTH> page_fault_inst;
  std::array<bool, FETCH_WIDTH> inst_valid;
  std::array<uint8_t, FETCH_WIDTH> predecode_type;
  std::array<uint32_t, FETCH_WIDTH> predecode_target_address;
  uint32_t seq_next_pc;
};

class InstructionFifoModel {
public:
  struct InstructionReadData {
    bool has_front;
    uint8_t queue_size;
    FIFOEntry front_entry;
  };

  struct InstructionCombIn {
    instruction_FIFO_in inp;
    InstructionReadData rd;
  };

  struct InstructionCombOut {
    instruction_FIFO_out out_regs;
    bool clear_fifo;
    bool push_en;
    FIFOEntry push_entry;
    bool pop_en;
  };

  struct InstructionDelta {
    bool clear_fifo;
    bool push_en;
    FIFOEntry push_entry;
    bool pop_en;
  };

  void seq_read(const instruction_FIFO_in &inp, instruction_FIFO_out &out) {
    (void)inp;
    begin_cycle();
    std::memset(&out, 0, sizeof(instruction_FIFO_out));
    out.full = (fifo_.size() == INSTRUCTION_FIFO_SIZE);
    out.empty = fifo_.empty();
    out.FIFO_valid = false;
  }

  static void instruction_fifo_comb(const InstructionCombIn &in,
                                    InstructionCombOut &out) {
    std::memset(&out, 0, sizeof(InstructionCombOut));
    out.out_regs.full = false;
    out.out_regs.empty = true;
    out.out_regs.FIFO_valid = false;

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

    FIFOEntry write_entry{};
    if (do_write) {
      for (int i = 0; i < FETCH_WIDTH; i++) {
        write_entry.instructions[i] = in.inp.fetch_group[i];
        write_entry.pc[i] = in.inp.pc[i];
        write_entry.page_fault_inst[i] = in.inp.page_fault_inst[i];
        write_entry.inst_valid[i] = in.inp.inst_valid[i];
        write_entry.predecode_type[i] = in.inp.predecode_type[i];
        write_entry.predecode_target_address[i] = in.inp.predecode_target_address[i];
      }
      write_entry.seq_next_pc = in.inp.seq_next_pc;
    }

    if (do_read) {
      const FIFOEntry &read_entry = has_data_before_read ? in.rd.front_entry : write_entry;
      for (int i = 0; i < FETCH_WIDTH; i++) {
        out.out_regs.instructions[i] = read_entry.instructions[i];
        out.out_regs.pc[i] = read_entry.pc[i];
        out.out_regs.page_fault_inst[i] = read_entry.page_fault_inst[i];
        out.out_regs.inst_valid[i] = read_entry.inst_valid[i];
        out.out_regs.predecode_type[i] = read_entry.predecode_type[i];
        out.out_regs.predecode_target_address[i] = read_entry.predecode_target_address[i];
      }
      out.out_regs.seq_next_pc = read_entry.seq_next_pc;
      out.out_regs.FIFO_valid = true;
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
    out.out_regs.empty = (next_size == 0);
    out.out_regs.full = (next_size == INSTRUCTION_FIFO_SIZE);
  }

  void comb_calc(const instruction_FIFO_in &inp, instruction_FIFO_out &out) {
    ensure_cycle_started();
    std::queue<FIFOEntry> virtual_fifo = fifo_;
    apply_pending_deltas(virtual_fifo);

    InstructionCombIn comb_in{};
    comb_in.inp = inp;
    comb_in.rd.has_front = !virtual_fifo.empty();
    comb_in.rd.queue_size = static_cast<uint8_t>(virtual_fifo.size());
    if (comb_in.rd.has_front) {
      comb_in.rd.front_entry = virtual_fifo.front();
    } else {
      std::memset(&comb_in.rd.front_entry, 0, sizeof(FIFOEntry));
    }

    InstructionCombOut comb_out{};
    instruction_fifo_comb(comb_in, comb_out);
    out = comb_out.out_regs;
    pending_deltas_.push_back(InstructionDelta{comb_out.clear_fifo, comb_out.push_en,
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

  void clear_fifo(std::queue<FIFOEntry> &queue_ref) {
    while (!queue_ref.empty()) {
      queue_ref.pop();
    }
  }

  void apply_delta(std::queue<FIFOEntry> &queue_ref, const InstructionDelta &delta) {
    if (delta.clear_fifo) {
      clear_fifo(queue_ref);
    }
    if (delta.push_en) {
      if (queue_ref.size() >= INSTRUCTION_FIFO_SIZE) {
        std::printf("[INSTRUCTION_FIFO_TOP] ERROR!!: fifo.size() >= INSTRUCTION_FIFO_SIZE\n");
        std::exit(1);
      }
      queue_ref.push(delta.push_entry);
    }
    if (delta.pop_en) {
      if (queue_ref.empty()) {
        std::printf("[INSTRUCTION_FIFO_TOP] ERROR!!: fifo underflow on read\n");
        std::exit(1);
      }
      queue_ref.pop();
    }
  }

  void apply_pending_deltas(std::queue<FIFOEntry> &queue_ref) {
    for (const InstructionDelta &delta : pending_deltas_) {
      apply_delta(queue_ref, delta);
    }
  }

  std::queue<FIFOEntry> fifo_;
  std::vector<InstructionDelta> pending_deltas_;
  bool cycle_started_ = false;
};

InstructionFifoModel g_instruction_fifo;

} // namespace

void instruction_FIFO_seq_read(struct instruction_FIFO_in *in,
                               struct instruction_FIFO_out *out);
void instruction_FIFO_comb_calc(struct instruction_FIFO_in *in,
                                struct instruction_FIFO_out *out);
void instruction_FIFO_seq_write();

void instruction_FIFO_top(struct instruction_FIFO_in *in,
                          struct instruction_FIFO_out *out) {
  assert(in);
  assert(out);
  instruction_FIFO_seq_read(in, out);
  instruction_FIFO_comb_calc(in, out);
  instruction_FIFO_seq_write();
}

void instruction_FIFO_seq_read(struct instruction_FIFO_in *in,
                               struct instruction_FIFO_out *out) {
  assert(in);
  assert(out);
  g_instruction_fifo.seq_read(*in, *out);
}

void instruction_FIFO_comb_calc(struct instruction_FIFO_in *in,
                                struct instruction_FIFO_out *out) {
  assert(in);
  assert(out);
  g_instruction_fifo.comb_calc(*in, *out);
}

void instruction_FIFO_seq_write() { g_instruction_fifo.seq_write(); }
