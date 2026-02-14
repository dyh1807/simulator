#include "Csr.h"
#include "config.h"
#include "Idu.h"
#include "RISCV.h"
#include "ref.h"
#include <cstdint>
#include <cstdlib>
#include "util.h"

// 中间信号
static tag_t alloc_tag[DECODE_WIDTH]; // 分配的新 Tag
static wire<1> issue_fire[DECODE_WIDTH];
static wire<1> front_accept;

void decode(InstInfo &uop, uint32_t instructinn);

void Idu::init() {
  ftq.init();
  ftq.in.rob_commit = in.commit;
  ftq.in.alloc_req = &ftq_alloc_req;
  ftq.out.alloc_resp = &ftq_alloc_resp;
  ftq.out.status = &ftq_status;
  ftq.out.lookup = &ftq_lookup;

  for (int i = 1; i < MAX_BR_NUM; i++) {
    tag_vec[i] = true;
    tag_vec_1[i] = true;
  }

  tag_vec_1[0] = false;
  now_tag_1 = now_tag = 0;
  enq_ptr_1 = enq_ptr = 1;
  for (int i = 0; i < MAX_BR_NUM; i++) {
    tag_list[i] = 0;
    tag_list_1[i] = 0;
  }
  br_latch = {};
  br_latch_1 = {};

  ibuf_head = 0;
  ibuf_tail = 0;
  ibuf_count = 0;
  for (int i = 0; i < IDU_INST_BUFFER_SIZE; i++) {
    ibuf[i] = {};
  }
  for (int i = 0; i < DECODE_WIDTH; i++) {
    issue_fire[i] = false;
  }
  front_accept = false;
}

// 译码并分配 Tag
void Idu::comb_decode() {
  wire<1> alloc_valid[DECODE_WIDTH];
  int alloc_num = 0;
  for (int i = 0; i < MAX_BR_NUM && alloc_num < max_br_per_cycle; i++) {
    if (tag_vec[i]) {
      alloc_tag[alloc_num] = i;
      alloc_valid[alloc_num] = true;
      alloc_num++;
    }
  }
  for (int i = alloc_num; i < DECODE_WIDTH; i++) {
    alloc_tag[i] = 0;
    alloc_valid[i] = false;
  }

  for (int i = 0; i < DECODE_WIDTH; i++) {
    out.dec2ren->valid[i] = false;
    out.dec2ren->uop[i] = {};
  }

  int decode_slots = ibuf_count < DECODE_WIDTH ? ibuf_count : DECODE_WIDTH;
  for (int i = 0; i < decode_slots; i++) {
    int idx = (ibuf_head + i) % IDU_INST_BUFFER_SIZE;
    const IbufEntry &entry = ibuf[idx];
    if (!entry.valid)
      continue;

    out.dec2ren->valid[i] = true;
    if (entry.page_fault_inst) {
      out.dec2ren->uop[i].uop_num = 1;
      out.dec2ren->uop[i].page_fault_inst = true;
      out.dec2ren->uop[i].page_fault_load = false;
      out.dec2ren->uop[i].page_fault_store = false;
      out.dec2ren->uop[i].type = NOP;
      out.dec2ren->uop[i].src1_en = false;
      out.dec2ren->uop[i].src2_en = false;
      out.dec2ren->uop[i].dest_en = false;
      out.dec2ren->uop[i].instruction = entry.inst;
    } else {
      decode(out.dec2ren->uop[i], entry.inst);
    }

    out.dec2ren->uop[i].pc = entry.pc;
    out.dec2ren->uop[i].ftq_idx = entry.ftq_idx;
    out.dec2ren->uop[i].ftq_offset = entry.ftq_offset;
    out.dec2ren->uop[i].ftq_is_last = entry.ftq_is_last;
  }

  int br_num = 0;
  bool stall = false;
  int i = 0;
  for (; i < DECODE_WIDTH; i++) {
    if (!out.dec2ren->valid[i]) {
      out.dec2ren->uop[i].tag = 0;
      continue;
    }
    out.dec2ren->uop[i].tag = (br_num == 0) ? now_tag : alloc_tag[br_num - 1];
    if (is_branch(out.dec2ren->uop[i].type)) {
      if (!alloc_valid[br_num]) {
#ifdef CONFIG_PERF_COUNTER
        ctx->perf.idu_tag_stall++;
#endif
        stall = true;
        break;
      }
      br_num++;
    }
  }

  if (stall) {
    for (; i < DECODE_WIDTH; i++) {
      out.dec2ren->valid[i] = false;
      out.dec2ren->uop[i].tag = 0;
    }
  }

}

void Idu::comb_branch() {
  // Init next state
  for (int i = 0; i < MAX_BR_NUM; i++) {
    tag_vec_1[i] = tag_vec[i];
    tag_list_1[i] = tag_list[i];
  }
  enq_ptr_1 = enq_ptr;
  now_tag_1 = now_tag;

  // 如果一周期实现不方便，可以用状态机多周期实现
  if (br_latch.mispred) {
    out.dec_bcast->mispred = true;
    out.dec_bcast->br_tag = br_latch.br_tag;
    out.dec_bcast->redirect_rob_idx = br_latch.redirect_rob_idx;

    LOOP_DEC(enq_ptr_1, MAX_BR_NUM);
    int enq_pre = (enq_ptr_1 + MAX_BR_NUM - 1) % MAX_BR_NUM;
    out.dec_bcast->br_mask = 0;
    // TODO: 换while
    while (tag_list[enq_pre] != br_latch.br_tag) {
      out.dec_bcast->br_mask |= 1ULL << tag_list[enq_ptr_1];
      tag_vec_1[tag_list[enq_ptr_1]] = true;
      LOOP_DEC(enq_ptr_1, MAX_BR_NUM);
      LOOP_DEC(enq_pre, MAX_BR_NUM);
    }
    out.dec_bcast->br_mask |= 1ULL << tag_list[enq_ptr_1];
    now_tag_1 = tag_list[enq_ptr_1];
    LOOP_INC(enq_ptr_1, MAX_BR_NUM);
  } else {
    out.dec_bcast->br_mask = 0;
    out.dec_bcast->mispred = false;
  }
}

void Idu::comb_ftq_begin() {
  ftq.comb_begin();
  ftq.comb_status();
}

void Idu::comb_flush() {
  ftq.in.flush_req = false;
  ftq.in.recover_req = false;
  ftq.in.recover_tail = 0;

  if (in.rob_bcast->flush) {
    for (int i = 1; i < MAX_BR_NUM; i++) {
      tag_vec_1[i] = true;
    }
    tag_vec_1[0] = false;
    now_tag_1 = 0;
    enq_ptr_1 = 1;
    tag_list_1[0] = 0;

    ftq.in.flush_req = true;
  }

  // Mispred: 回滚 FTQ tail 到误预测分支的下一个条目
  if (br_latch.mispred) {
    int mispred_ftq = br_latch.ftq_idx;
    int new_tail = (mispred_ftq + 1) % FTQ_SIZE;
    ftq.in.recover_req = true;
    ftq.in.recover_tail = new_tail;
  }

  ftq.comb_ctrl();
}

void Idu::comb_fire() {
  for (int i = 0; i < DECODE_WIDTH; i++) {
    issue_fire[i] = false;
  }
  for (int i = 0; i < FETCH_WIDTH; i++) {
    out.dec2front->fire[i] = false;
  }
  front_accept = false;
  ftq_alloc_req.valid = false;
  ftq_alloc_req.entry = {};

  if (br_latch.mispred || in.rob_bcast->flush) {
    out.dec2front->ready = false;
    for (int i = 0; i < DECODE_WIDTH; i++) {
      out.dec2ren->valid[i] = false;
    }
    return;
  }

  int incoming_valid_num = 0;
  for (int i = 0; i < FETCH_WIDTH; i++) {
    incoming_valid_num += in.front2dec->valid[i] ? 1 : 0;
  }
  int free_slots = IDU_INST_BUFFER_SIZE - ibuf_count;
  out.dec2front->ready = !ftq_status.full && (incoming_valid_num <= free_slots);

  if (out.dec2front->ready && incoming_valid_num > 0) {
    FTQEntry ftq_entry;
    ftq_entry.start_pc = in.front2dec->pc[0];
    ftq_entry.next_pc = in.front2dec->predict_next_fetch_address[0];
    for (int i = 0; i < FETCH_WIDTH; i++) {
      ftq_entry.pred_taken_mask[i] = in.front2dec->predict_dir[i];
      ftq_entry.alt_pred[i] = in.front2dec->alt_pred[i];
      ftq_entry.altpcpn[i] = in.front2dec->altpcpn[i];
      ftq_entry.pcpn[i] = in.front2dec->pcpn[i];
      for (int j = 0; j < 4; j++) {
        ftq_entry.tage_idx[i][j] = in.front2dec->tage_idx[i][j];
        ftq_entry.tage_tag[i][j] = in.front2dec->tage_tag[i][j];
      }
    }
    ftq_alloc_req.valid = true;
    ftq_alloc_req.entry = ftq_entry;
    front_accept = true;
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out.dec2front->fire[i] = in.front2dec->valid[i];
    }
  }

  int br_num = 0;
  for (int i = 0; i < DECODE_WIDTH; i++) {
    issue_fire[i] = out.dec2ren->valid[i] && in.ren2dec->ready;
    if (issue_fire[i] && is_branch(out.dec2ren->uop[i].type)) {
      now_tag_1 = alloc_tag[br_num];
      tag_vec_1[alloc_tag[br_num]] = false;
      tag_list_1[enq_ptr_1] = alloc_tag[br_num];
      LOOP_INC(enq_ptr_1, MAX_BR_NUM);
      br_num++;
    }
  }

  ftq.comb_alloc_req();
}

void Idu::comb_ftq_commit_reclaim() { ftq.comb_commit_reclaim(); }

void Idu::comb_release_tag() {
  for (int i = 0; i < COMMIT_WIDTH; i++) {
    if (in.commit->commit_entry[i].valid &&
        is_branch(in.commit->commit_entry[i].uop.type)) {
      tag_vec_1[in.commit->commit_entry[i].uop.tag] = true;
    }
  }
}

void Idu::seq() {
  now_tag = now_tag_1;
  for (int i = 0; i < MAX_BR_NUM; i++) {
    tag_vec[i] = tag_vec_1[i];
    tag_list[i] = tag_list_1[i];
  }
  enq_ptr = enq_ptr_1;

  if (in.rob_bcast->flush || br_latch.mispred) {
#ifdef CONFIG_PERF_COUNTER
    if (ibuf_count > 0) {
      if (in.rob_bcast->flush) {
        ctx->perf.squash_flush_idu += ibuf_count;
        ctx->perf.squash_flush_total += ibuf_count;
      } else {
        ctx->perf.squash_mispred_idu += ibuf_count;
        ctx->perf.squash_mispred_total += ibuf_count;
      }
      ctx->perf.pending_squash_slots += ibuf_count;
    }
#endif
    ibuf_head = 0;
    ibuf_tail = 0;
    ibuf_count = 0;
    for (int i = 0; i < IDU_INST_BUFFER_SIZE; i++) {
      ibuf[i] = {};
    }
  } else {
    int deq_num = 0;
    for (int i = 0; i < DECODE_WIDTH; i++) {
      if (issue_fire[i]) {
        deq_num++;
      } else {
        break;
      }
    }
    for (int i = 0; i < deq_num; i++) {
      ibuf[ibuf_head] = {};
      ibuf_head = (ibuf_head + 1) % IDU_INST_BUFFER_SIZE;
    }
    ibuf_count -= deq_num;
    if (ibuf_count < 0) {
      ibuf_count = 0;
    }

    if (front_accept) {
      Assert(ftq_alloc_resp.success && "FTQ alloc failed in Idu::seq");
      int idx = ftq_alloc_resp.idx;
      Assert(idx >= 0 && "Invalid FTQ allocation index");
      int last_fire_idx = -1;
      for (int i = FETCH_WIDTH - 1; i >= 0; i--) {
        if (out.dec2front->fire[i]) {
          last_fire_idx = i;
          break;
        }
      }

      for (int i = 0; i < FETCH_WIDTH; i++) {
        if (!out.dec2front->fire[i]) {
          continue;
        }
        Assert(ibuf_count < IDU_INST_BUFFER_SIZE && "IDU ibuf overflow");
        IbufEntry &entry = ibuf[ibuf_tail];
        entry.valid = true;
        entry.inst = in.front2dec->inst[i];
        entry.pc = in.front2dec->pc[i];
        entry.page_fault_inst = in.front2dec->page_fault_inst[i];
        entry.predict_dir = in.front2dec->predict_dir[i];
        entry.alt_pred = in.front2dec->alt_pred[i];
        entry.altpcpn = in.front2dec->altpcpn[i];
        entry.pcpn = in.front2dec->pcpn[i];
        for (int j = 0; j < 4; j++) {
          entry.tage_idx[j] = in.front2dec->tage_idx[i][j];
        }
        entry.ftq_idx = idx;
        entry.ftq_offset = i;
        entry.ftq_is_last = (i == last_fire_idx);
        ibuf_tail = (ibuf_tail + 1) % IDU_INST_BUFFER_SIZE;
        ibuf_count++;
      }
    }
  }

  // Latch Exu Branch Result
  if (!in.rob_bcast->flush) {
    br_latch.mispred = in.exu2id->mispred;
    br_latch.redirect_pc = in.exu2id->redirect_pc;
    br_latch.redirect_rob_idx = in.exu2id->redirect_rob_idx;
    br_latch.br_tag = in.exu2id->br_tag;
    br_latch.ftq_idx = in.exu2id->ftq_idx;
  } else {
    br_latch = {};
  }

  ftq.seq();
}

void Idu::decode(InstInfo &uop, uint32_t inst) {
  // 操作数来源以及type
  // uint32_t imm;
  int uop_num = 1;
  uop.difftest_skip = false;

  uint32_t opcode = BITS(inst, 6, 0);
  uint32_t number_funct3_unsigned = BITS(inst, 14, 12);
  uint32_t number_funct7_unsigned = BITS(inst, 31, 25);
  uint32_t reg_d_index = BITS(inst, 11, 7);
  uint32_t reg_a_index = BITS(inst, 19, 15);
  uint32_t reg_b_index = BITS(inst, 24, 20);
  uint32_t csr_idx = inst >> 20;

  // 准备立即数
  uop.instruction = inst;
  uop.diag_val = inst;
  uop.dest_areg = reg_d_index;
  uop.src1_areg = reg_a_index;
  uop.src2_areg = reg_b_index;
  uop.src1_is_pc = false;
  uop.src2_is_imm = true;
  uop.func3 = number_funct3_unsigned;
  uop.func7 = number_funct7_unsigned;
  uop.csr_idx = csr_idx;
  uop.page_fault_inst = false;
  uop.page_fault_load = false;
  uop.page_fault_store = false;
  uop.illegal_inst = false;
  uop.type = NOP;
  static uint64_t global_inst_idx = 0;
  uop.inst_idx = global_inst_idx++;

  switch (opcode) {
  case number_0_opcode_lui: { // lui
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src1_areg = 0;
    uop.src2_en = false;
    uop.type = ADD;
    uop.func3 = 0;
    uop.imm = immU(inst);
    break;
  }
  case number_1_opcode_auipc: { // auipc
    uop.dest_en = true;
    uop.src1_en = false;
    uop.src2_en = false;
    uop.src1_is_pc = true;
    uop.type = ADD;
    uop.func3 = 0;
    uop.imm = immU(inst);
    break;
  }
  case number_2_opcode_jal: { // jal
    uop_num = 2;              // 前端pre-decode预先解决jal
    uop.dest_en = true;
    uop.src1_en = false;
    uop.src2_en = false;
    uop.src1_is_pc = true;
    uop.src2_is_imm = true;
    uop.func3 = 0;
    uop.type = JAL;
    uop.imm = immJ(inst);
    break;
  }
  case number_3_opcode_jalr: { // jalr
    uop_num = 2;
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = false;
    uop.src1_is_pc = true;
    uop.src2_is_imm = true;
    uop.func3 = 0;
    uop.type = JALR;
    uop.imm = immI(inst);

    break;
  }
  case number_4_opcode_beq: { // beq, bne, blt, bge, bltu, bgeu
    uop.dest_en = false;
    uop.src1_en = true;
    uop.src2_en = true;
    uop.type = BR;
    uop.imm = immB(inst);
    break;
  }
  case number_5_opcode_lb: { // lb, lh, lw, lbu, lhu
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = false;
    uop.type = LOAD;
    uop.imm = immI(inst);
    break;
  }
  case number_6_opcode_sb: { // sb, sh, sw
    uop_num = 2;
    uop.dest_en = false;
    uop.src1_en = true;
    uop.src2_en = true;
    uop.type = STORE;
    uop.imm = immS(inst);
    break;
  }
  case number_7_opcode_addi: { // addi, slti, sltiu, xori, ori, andi, slli,
    // srli, srai
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = false;
    uop.type = ADD;
    uop.imm = immI(inst);
    break;
  }
  case number_8_opcode_add: { // add, sub, sll, slt, sltu, xor, srl, sra, or,
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = true;
    uop.src2_is_imm = false;
    if (number_funct7_unsigned == 1) { // mul div
      if (number_funct3_unsigned & 0b100) {
        uop.type = DIV;
      } else {
        uop.type = MUL;
      }
    } else {
      uop.type = ADD;
    }
    break;
  }
  case number_9_opcode_fence: { // fence, fence.i
    uop.dest_en = false;
    uop.src1_en = false;
    uop.src2_en = false;

    // Check funct3 for FENCE.I (001)
    if (number_funct3_unsigned == 0b001) {
      uop.type = FENCE_I; // Strict separation
    } else {
      uop.type = NOP; // Ordinary FENCE is NOP
    }
    break;
  }
  case number_10_opcode_ecall: { // ecall, ebreak, csrrw, csrrs, csrrc,
                                 // csrrwi, csrrsi, csrrci
    uop.src2_is_imm =
        number_funct3_unsigned & 0b100 &&
        (number_funct3_unsigned & 0b001 || number_funct3_unsigned & 0b010);

    if (number_funct3_unsigned & 0b001 || number_funct3_unsigned & 0b010) {
      if (csr_idx != number_mtvec && csr_idx != number_mepc &&
          csr_idx != number_mcause && csr_idx != number_mie &&
          csr_idx != number_mip && csr_idx != number_mtval &&
          csr_idx != number_mscratch && csr_idx != number_mstatus &&
          csr_idx != number_mideleg && csr_idx != number_medeleg &&
          csr_idx != number_sepc && csr_idx != number_stvec &&
          csr_idx != number_scause && csr_idx != number_sscratch &&
          csr_idx != number_stval && csr_idx != number_sstatus &&
          csr_idx != number_sie && csr_idx != number_sip &&
          csr_idx != number_satp && csr_idx != number_mhartid &&
          csr_idx != number_misa) {
        uop.type = NOP;
        uop.dest_en = false;
        uop.src1_en = false;
        uop.src2_en = false;

        if (csr_idx == number_time || csr_idx == number_timeh)
          uop.illegal_inst = true;

      } else {
        uop.type = CSR;
        uop.dest_en = true;
        uop.src1_en = true;
        uop.src2_en = !uop.src2_is_imm;
        uop.imm = reg_a_index;
      }
    } else {
      uop.dest_en = false;
      uop.src1_en = false;
      uop.src2_en = false;

      if (inst == INST_ECALL) {
        uop.type = ECALL;
      } else if (inst == INST_EBREAK) {
        uop.type = EBREAK;
      } else if (inst == INST_MRET) {
        uop.type = MRET;
      } else if (inst == INST_WFI) {
        uop.type = WFI;
      } else if (inst == INST_SRET) {
        uop.type = SRET;
      } else if (number_funct7_unsigned == 0b0001001 &&
                 number_funct3_unsigned == 0 && reg_d_index == 0) {
        uop.type = SFENCE_VMA;
        uop.src1_en = true;
        uop.src2_en = true;
      } else {
        uop.type = NOP;
        /*uop[0].illegal_inst = true;*/
        /*cout << hex << inst << endl;*/
        /*Assert(0);*/
      }
    }
    break;
  }

  case number_11_opcode_lrw: {
    uop_num = 3;
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = true;
    uop.imm = 0;
    uop.type = AMO;

    if ((number_funct7_unsigned >> 2) == AmoOp::LR) {
      uop_num = 1;
      uop.src2_en = false;
    }

    break;
  }

  case number_12_opcode_float: {
    uop.dest_en = true;
    uop.src1_en = true;
    uop.src2_en = true;
    uop.src2_is_imm = false;
    uop.type = FP;
    break;
  }

  default: {
    uop.dest_en = false;
    uop.src1_en = false;
    uop.src2_en = false;
    uop.type = NOP;
    uop.illegal_inst = true;
    break;
  }
  }

  uop.uop_num = uop_num;

  if (uop.type == AMO && uop.dest_areg == 0 && (uop.func7 >> 2) != AmoOp::LR &&
      (uop.func7 >> 2) != AmoOp::SC) {
    uop.dest_areg = 32;
  }

  if (uop.dest_areg == 0)
    uop.dest_en = false;
}

IduIO Idu::get_hardware_io() {
  IduIO hardware;

  // --- Inputs ---
  for (int i = 0; i < FETCH_WIDTH; i++) {
    hardware.from_front.valid[i] = in.front2dec->valid[i];
    hardware.from_front.inst[i] = in.front2dec->inst[i];
  }
  hardware.from_ren.ready = in.ren2dec->ready;

  hardware.from_back.flush = in.rob_bcast->flush;
  hardware.from_back.flush = in.rob_bcast->flush;
  hardware.from_back.mispred = br_latch.mispred;
  hardware.from_back.br_tag = br_latch.br_tag;

  // --- Outputs ---
  hardware.to_front.ready = out.dec2front->ready;
  for (int i = 0; i < FETCH_WIDTH; i++) {
    hardware.to_front.fire[i] = out.dec2front->fire[i];
  }

  for (int i = 0; i < DECODE_WIDTH; i++) {
    hardware.to_ren.valid[i] = out.dec2ren->valid[i];
    hardware.to_ren.uop[i] = DecRenUop::filter(out.dec2ren->uop[i]);
  }

  hardware.to_back.mispred = out.dec_bcast->mispred;
  hardware.to_back.br_mask = out.dec_bcast->br_mask;
  hardware.to_back.br_tag = out.dec_bcast->br_tag;

  return hardware;
}
