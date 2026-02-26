#include "predecode_checker.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

class PredecodeCheckerModel {
public:
  struct ReadData {
    predecode_checker_in inp_regs;
  };

  struct CheckerCombIn {
    ReadData rd;
  };

  struct CheckerCombOut {
    predecode_checker_out out_regs;
  };

  void checker_seq_read(const predecode_checker_in &inp, ReadData &rd) const {
    std::memset(&rd, 0, sizeof(ReadData));
    rd.inp_regs = inp;
  }

  void checker_comb(const CheckerCombIn &in, CheckerCombOut &out) const {
    std::memset(&out, 0, sizeof(CheckerCombOut));

    for (int i = 0; i < FETCH_WIDTH; i++) {
      switch (in.rd.inp_regs.predecode_type[i]) {
      case PREDECODE_NON_BRANCH:
        out.out_regs.predict_dir_corrected[i] = false;
        break;
      case PREDECODE_DIRECT_JUMP_NO_JAL:
        out.out_regs.predict_dir_corrected[i] = in.rd.inp_regs.predict_dir[i];
        break;
      case PREDECODE_JALR:
      case PREDECODE_JAL:
        out.out_regs.predict_dir_corrected[i] = true;
        break;
      default:
        std::printf("ERROR!!: predecode_type[%d] = %d\n", i,
                    in.rd.inp_regs.predecode_type[i]);
        std::exit(1);
      }
    }

    int first_taken_index = -1;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      if (out.out_regs.predict_dir_corrected[i]) {
        first_taken_index = i;
        break;
      }
    }

    out.out_regs.predict_next_fetch_address_corrected =
        in.rd.inp_regs.predict_next_fetch_address;

    if (first_taken_index != -1) {
      if (in.rd.inp_regs.predecode_type[first_taken_index] ==
              PREDECODE_DIRECT_JUMP_NO_JAL ||
          in.rd.inp_regs.predecode_type[first_taken_index] == PREDECODE_JAL) {
        out.out_regs.predict_next_fetch_address_corrected =
            in.rd.inp_regs.predecode_target_address[first_taken_index];
      }
    } else {
      out.out_regs.predict_next_fetch_address_corrected = in.rd.inp_regs.seq_next_pc;
    }

    out.out_regs.predecode_flush_enable =
        (in.rd.inp_regs.predict_next_fetch_address !=
         out.out_regs.predict_next_fetch_address_corrected);
  }

  void comb_calc_only(const predecode_checker_in &inp, predecode_checker_out &out) const {
    ReadData rd;
    CheckerCombIn comb_in{};
    CheckerCombOut comb_out{};
    checker_seq_read(inp, rd);
    comb_in.rd = rd;
    checker_comb(comb_in, comb_out);
    out = comb_out.out_regs;
  }
};

PredecodeCheckerModel g_predecode_checker_model;

} // namespace

void predecode_checker_top(struct predecode_checker_in *in,
                           struct predecode_checker_out *out) {
  assert(in);
  assert(out);
  predecode_checker_seq_read(in, out);
  predecode_checker_comb_calc(in, out);
  predecode_checker_seq_write();
}

void predecode_checker_seq_read(struct predecode_checker_in *in,
                                struct predecode_checker_out *out) {
  assert(in);
  assert(out);
  std::memset(out, 0, sizeof(predecode_checker_out));
}

void predecode_checker_comb_calc(struct predecode_checker_in *in,
                                 struct predecode_checker_out *out) {
  assert(in);
  assert(out);
  g_predecode_checker_model.comb_calc_only(*in, *out);
}

void predecode_checker_seq_write() {}
