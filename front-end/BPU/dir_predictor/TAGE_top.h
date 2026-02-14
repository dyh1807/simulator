#ifndef TAGE_TOP_H
#define TAGE_TOP_H

#include "../../frontend.h"
#include "../BPU_configs.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <random>

// ============================================================================
// 从BPU读取全局GHR/FH的接口函数（在某个 .cpp 中实现）
// ============================================================================
const bool* BPU_get_Arch_GHR();
const bool* BPU_get_Spec_GHR();
const uint32_t (*BPU_get_Arch_FH())[TN_MAX];
const uint32_t (*BPU_get_Spec_FH())[TN_MAX];


// ============================================================================
// 1. 基础结构体定义 (Structures)
// ============================================================================

// TAGE index and tag
struct TageIndex {
  uint32_t tage_index[TN_MAX];
  uint8_t tag[TN_MAX];
};

// TAGE index-tag and base index
struct TageIndexTag {
  TageIndex index_info;
  uint32_t base_idx;
};

struct TageTableReadData {
  uint8_t tag[TN_MAX];    // 8-bit tag
  uint8_t cnt[TN_MAX];    // 3-bit cnt
  uint8_t useful[TN_MAX]; // 2-bit useful
  uint8_t base_cnt;       // 2-bit base counter
};

struct PredResult {
  bool pred;
  bool alt_pred;
  int pcpn;    // 3-bit pcpn(0123 and miss)
  int altpcpn; // 3-bit altpcpn(0123 and miss)
  TageIndex index_info;
};

struct UpdateRequest {
  bool cnt_we[TN_MAX];
  uint8_t cnt_wdata[TN_MAX];
  bool useful_we[TN_MAX];
  uint8_t useful_wdata[TN_MAX];
  bool tag_we[TN_MAX];
  uint8_t tag_wdata[TN_MAX];
  bool base_we;
  int base_wdata;
  bool reset_we;
  uint32_t reset_row_idx;
  bool reset_msb_only;
};

struct LSFR_Output {
  bool next_state[4];
  uint8_t random_val;
};

// ============================================================================
// 2. 辅助函数 (Pure Combinational Helpers)
// ============================================================================

static uint8_t sat_inc_3bit(uint8_t val) { return (val >= 7) ? 7 : val + 1; }
static uint8_t sat_dec_3bit(uint8_t val) { return (val == 0) ? 0 : val - 1; }
static uint8_t sat_inc_2bit(uint8_t val) { return (val >= 3) ? 3 : val + 1; }
static uint8_t sat_dec_2bit(uint8_t val) { return (val == 0) ? 0 : val - 1; }

// ============================================================================
// 2.1 GHR/FH更新组合逻辑函数（提取为公共函数供BPU使用）
// ============================================================================

// [comb] GHR Update - 提取为公共静态内联函数
static inline void TAGE_GHR_update_comb_1(const bool current_GHR[GHR_LENGTH], bool real_dir,
                                          bool next_GHR[GHR_LENGTH]) {
  for (int i = GHR_LENGTH - 1; i > 0; i--) {
    next_GHR[i] = current_GHR[i - 1];
  }
  next_GHR[0] = real_dir;
}

// [comb] FH Update - 提取为公共静态内联函数
static inline void TAGE_FH_update_comb_1(const uint32_t current_FH[FH_N_MAX][TN_MAX],
                                         const bool current_GHR[GHR_LENGTH], bool new_history,
                                         uint32_t next_FH[FH_N_MAX][TN_MAX],
                                         const uint32_t fh_len[FH_N_MAX][TN_MAX],
                                         const uint32_t ghr_len[TN_MAX]) {
  for (int k = 0; k < FH_N_MAX; k++) {
    for (int i = 0; i < TN_MAX; i++) {
      bool old_highest;
      uint32_t val = current_FH[k][i];
      uint32_t len = fh_len[k][i];
      old_highest = (val >> (len - 1)) & 0x1;
      val = (val << 1) & ((0x1 << len) - 1);
      val |= new_history ^ old_highest;
      uint32_t ghr_idx = ghr_len[i];
      val ^= current_GHR[ghr_idx-1] << (ghr_idx % len); 
      next_FH[k][i] = val;
    }
  }
}

// ============================================================================
// 3. 核心逻辑类 (TAGE_TOP Class)
// ============================================================================

class TAGE_TOP {
public:
  // ------------------------------------------------------------------------
  // 状态枚举定义（需要在结构体之前定义）
  // ------------------------------------------------------------------------
  enum State {
    S_IDLE = 0,
    S_STAGE2 = 1,
    S_IDLE_WAIT_DATA = 2,
    S_STAGE2_WAIT_DATA = 3
  };

  // ------------------------------------------------------------------------
  // 输入输出接口结构体
  // ------------------------------------------------------------------------
  struct InputPayload {
    bool pred_req;
    uint32_t pc_pred_in;
    bool update_en;
    uint32_t pc_update_in;
    bool real_dir;
    bool pred_in;       // 1-bit
    bool alt_pred_in;   // 1-bit
    uint8_t pcpn_in;    // 3-bit
    uint8_t altpcpn_in; // 3-bit
    uint8_t tage_tag_flat_in[TN_MAX];
    uint32_t tage_idx_flat_in[TN_MAX];
  };

  struct OutputPayload {
    bool pred_out;
    bool alt_pred_out;
    uint8_t pcpn_out;
    uint8_t altpcpn_out;

    uint8_t tage_tag_flat_out[TN_MAX];
    uint32_t tage_idx_flat_out[TN_MAX];

    bool tage_pred_out_valid;
    bool tage_update_done;
    bool busy;
  };

  // 状态输入结构体（包含所有寄存器）
  struct StateInput {
    State state;
    uint32_t FH[FH_N_MAX][TN_MAX];
    bool GHR[GHR_LENGTH];
    bool LSFR[4];
    uint32_t reset_cnt_reg;
    // input latches
    bool do_pred_latch;
    bool do_upd_latch;
    bool upd_real_dir_latch;
    uint32_t upd_pc_latch;
    bool upd_pred_in_latch;
    bool upd_alt_pred_in_latch;
    uint8_t upd_pcpn_in_latch;
    uint8_t upd_altpcpn_in_latch;
    uint8_t upd_tage_tag_flat_latch[TN_MAX];
    uint32_t upd_tage_idx_flat_latch[TN_MAX];
    // pipeline latches
    uint32_t pred_calc_base_idx_latch;
    uint32_t pred_calc_tage_idx_latch[TN_MAX];
    uint8_t pred_calc_tage_tag_latch[TN_MAX];
  };

  // Index生成结果
  struct IndexResult {
    uint32_t table_base_idx;
    uint32_t table_tage_idx[TN_MAX];
    bool table_read_address_valid;
  };

  // 内存读取结果
  struct MemReadResult {
    TageTableReadData table_r;
    bool table_read_data_valid;
  };

  // 组合逻辑计算结果结构体
  struct CombResult {
    State next_state;
    uint32_t table_base_idx;
    uint32_t table_tage_idx[TN_MAX];
    // TageTableReadData table_r;
    TageIndexTag s1_calc;
    LSFR_Output lsfr_out;
    UpdateRequest upd_calc_res;
    PredResult s2_comb_res;
    // GHR/FH更新已迁移到BPU，以下字段保留但不使用（注释掉以避免编译警告）
    // bool next_Spec_GHR[GHR_LENGTH];
    // uint32_t next_Spec_FH[FH_N_MAX][TN_MAX];
    // bool next_Arch_GHR[GHR_LENGTH];
    // uint32_t next_Arch_FH[FH_N_MAX][TN_MAX];
    OutputPayload out_regs;
  };

private:
  // 全局寄存器: GHR/FH已迁移到BPU_TOP统一管理，以下成员已删除
  // uint32_t Arch_FH[FH_N_MAX][TN_MAX];
  // uint32_t Spec_FH[FH_N_MAX][TN_MAX];
  // bool Arch_GHR[GHR_LENGTH];
  // bool Spec_GHR[GHR_LENGTH];

  bool LSFR[4];
  uint32_t reset_cnt_reg;

  // 表项存储 (Memories)
  int base_counter[BASE_ENTRY_NUM];
  uint8_t tag_table[TN_MAX][TN_ENTRY_NUM];
  uint8_t cnt_table[TN_MAX][TN_ENTRY_NUM];
  uint8_t useful_table[TN_MAX][TN_ENTRY_NUM];

  // FH constants
  const uint32_t ghr_length[TN_MAX] = {8, 13, 32, 119};
  const uint32_t fh_length[FH_N_MAX][TN_MAX] = {
      {8, 11, 11, 11}, {8, 8, 8, 8}, {7, 7, 7, 7}};

  // Pipeline Registers
  State state;
  bool do_pred_latch;
  bool do_upd_latch;
  bool upd_real_dir_latch;
  uint32_t upd_pc_latch;
  bool upd_pred_in_latch;
  bool upd_alt_pred_in_latch;
  uint8_t upd_pcpn_in_latch;
  uint8_t upd_altpcpn_in_latch;
  uint8_t upd_tage_tag_flat_latch[TN_MAX];
  uint32_t upd_tage_idx_flat_latch[TN_MAX];

  // Pipeline Regs
  uint32_t pred_calc_base_idx_latch;
  uint32_t pred_calc_tage_idx_latch[TN_MAX];
  uint8_t pred_calc_tage_tag_latch[TN_MAX];

  // For Update Writeback (S1 calc result):
  UpdateRequest upd_calc_winfo_latch; // 包含所有 upd_cnt_we, wdata 等

  // Outputs Registers
  OutputPayload out_regs;

  // SRAM延迟模拟相关变量
  bool sram_delay_active;           // 是否正在进行延迟
  int sram_delay_counter;            // 剩余延迟周期数
  TageTableReadData sram_delayed_data; // 延迟期间保存的数据
  bool sram_new_req_this_cycle;      // 本周期是否有新的读请求（在step_comb中设置，step_seq中使用）
  std::mt19937 rng;                  // 随机数生成器
  std::uniform_int_distribution<int> delay_dist; // 延迟分布（1-5周期）

public:
  // ------------------------------------------------------------------------
  // 构造函数
  // ------------------------------------------------------------------------
  TAGE_TOP() : rng(std::random_device{}()), delay_dist(SRAM_DELAY_MIN, SRAM_DELAY_MAX) { reset(); }

  void reset() {
    // GHR/FH已迁移到BPU_TOP，不再在此初始化
    // memset(Arch_FH, 0, sizeof(Arch_FH));
    // memset(Spec_FH, 0, sizeof(Spec_FH));
    // memset(Arch_GHR, 0, sizeof(Arch_GHR));
    // memset(Spec_GHR, 0, sizeof(Spec_GHR));
    // Verilog: lsfr_reg <= 4'b0001
    LSFR[0] = 0;
    LSFR[1] = 0;
    LSFR[2] = 0;
    LSFR[3] = 1;
    reset_cnt_reg = 0;

    memset(base_counter, 0, sizeof(base_counter));
    memset(tag_table, 0, sizeof(tag_table));
    memset(cnt_table, 0, sizeof(cnt_table));
    memset(useful_table, 0, sizeof(useful_table));

    state = S_IDLE;
    do_pred_latch = false;
    do_upd_latch = false;
    upd_real_dir_latch = false;
    upd_pc_latch = 0;
    upd_pred_in_latch = false;
    upd_alt_pred_in_latch = false;
    upd_pcpn_in_latch = 0;
    upd_altpcpn_in_latch = 0;
    for (int i = 0; i < TN_MAX; ++i) {
      upd_tage_idx_flat_latch[i] = 0;
      upd_tage_tag_flat_latch[i] = 0;
    }
    pred_calc_base_idx_latch = 0;
    for (int i = 0; i < TN_MAX; ++i) {
      pred_calc_tage_idx_latch[i] = 0;
      pred_calc_tage_tag_latch[i] = 0;
    }
    memset(&out_regs, 0, sizeof(OutputPayload));
    // Init pipeline regs to 0
    memset(&upd_calc_winfo_latch, 0, sizeof(UpdateRequest));
    // Init SRAM delay simulation
    sram_delay_active = false;
    sram_delay_counter = 0;
    sram_new_req_this_cycle = false;
    memset(&sram_delayed_data, 0, sizeof(TageTableReadData));
  }

  // ------------------------------------------------------------------------
  // 组合逻辑函数 - 生成Index
  // ------------------------------------------------------------------------
  IndexResult step_comb_gen_index(const InputPayload &inp,
                                  const StateInput &state_in) {
    IndexResult idx;
    memset(&idx, 0, sizeof(IndexResult));

    bool read_pred = state_in.state == S_STAGE2 && state_in.do_pred_latch;
    bool read_upd = state_in.state == S_IDLE && inp.update_en;

    // Table Address Mux Logic
    if (read_pred) {
      idx.table_base_idx = state_in.pred_calc_base_idx_latch;
      for (int i = 0; i < TN_MAX; ++i) {
        idx.table_tage_idx[i] = state_in.pred_calc_tage_idx_latch[i];
      }
      idx.table_read_address_valid = true;

    } else if (read_upd) {
      idx.table_base_idx =
          (inp.pc_update_in >> 2) & ((1 << TAGE_BASE_IDX_WIDTH) - 1);
      for (int i = 0; i < TN_MAX; ++i) {
        idx.table_tage_idx[i] = inp.tage_idx_flat_in[i];
      }
      idx.table_read_address_valid = true;

    } else {
      idx.table_read_address_valid = false;
    }

    return idx;
  }

  // ------------------------------------------------------------------------
  // 内存读取 (TABLE READ) - 带SRAM随机延迟模拟
  // ------------------------------------------------------------------------
  MemReadResult step_comb_mem_read(const IndexResult &idx) {
    MemReadResult mem;
    memset(&mem, 0, sizeof(MemReadResult));

    // 如果正在进行延迟，返回保存的数据
#ifdef SRAM_DELAY_ENABLE
    if (sram_delay_active) {
      mem.table_r = sram_delayed_data;
      if(sram_delay_counter == 0) {
        mem.table_read_data_valid = true;
        sram_delay_active = false;
      }
      else {
        mem.table_read_data_valid = false;
      }
      sram_new_req_this_cycle = false;
      return mem;
    }
#endif

    // Memory Read - 新的读请求
    if (idx.table_read_address_valid) {
      // 立即读取数据并保存
      mem.table_r.base_cnt = base_counter[idx.table_base_idx];
      for (int i = 0; i < TN_MAX; i++) {
        uint32_t mem_idx = idx.table_tage_idx[i];
        mem.table_r.tag[i] = tag_table[i][mem_idx];
        mem.table_r.cnt[i] = cnt_table[i][mem_idx];
        mem.table_r.useful[i] = useful_table[i][mem_idx];
      }
      // 保存数据，标记本周期有新请求
      sram_delayed_data = mem.table_r;
      sram_new_req_this_cycle = true;
      // 初始时数据无效，等待延迟完成
#ifdef SRAM_DELAY_ENABLE
      mem.table_read_data_valid = false;
#else
      mem.table_read_data_valid = true;
#endif
    } else {
      mem.table_read_data_valid = false;
      sram_new_req_this_cycle = false;
    }
    return mem;
  }

  // ------------------------------------------------------------------------
  // 组合逻辑函数 - 计算部分
  // ------------------------------------------------------------------------
  CombResult step_comb_calc(const InputPayload &inp, const StateInput &state_in,
                            const IndexResult &idx, const MemReadResult &mem) {
    CombResult comb;
    memset(&comb, 0, sizeof(CombResult));

    // 复制index结果到输出
    // comb.table_base_idx = idx.table_base_idx;
    // for (int i = 0; i < TN_MAX; ++i) {
    //   comb.table_tage_idx[i] = idx.table_tage_idx[i];
    // }

    DEBUG_LOG_SMALL("[TAGE_TOP] state=%d, inp.pred_req=%d, inp.update_en=%d, pred_latch=%d, upd_latch=%d\n", 
      state_in.state, inp.pred_req, inp.update_en, state_in.do_pred_latch, state_in.do_upd_latch);
    // 1.1 Next State Logic
    switch (state_in.state) {
      case S_IDLE:
        if (inp.pred_req || inp.update_en) {
          if (!inp.update_en)
            comb.next_state = S_STAGE2; // no update req, go straight to stage 2
          else if (mem.table_read_data_valid)
            comb.next_state = S_STAGE2; // data is ready, go straight to stage 2
          else
            comb.next_state =
                S_IDLE_WAIT_DATA; // data is not ready, wait for data
        } else {
          comb.next_state = S_IDLE;
        }
        break;
      case S_STAGE2:
        if (!state_in.do_pred_latch)
          comb.next_state = S_IDLE; // no pred req, go straight to idle
        else if (mem.table_read_data_valid)
          comb.next_state = S_IDLE; // data is ready, go straight to idle
        else
          comb.next_state =
              S_STAGE2_WAIT_DATA; // data is not ready, wait for data
        break;
      case S_IDLE_WAIT_DATA:
        if (mem.table_read_data_valid)
          comb.next_state = S_STAGE2;
        else
          comb.next_state = S_IDLE_WAIT_DATA;
        break;
      case S_STAGE2_WAIT_DATA:
        if (mem.table_read_data_valid)
          comb.next_state = S_IDLE;
        else
          comb.next_state = S_STAGE2_WAIT_DATA;
        break;
      default:
        printf("[TAGE_TOP] ERROR!!: state = %d\n", state_in.state);
        exit(1); // unknown state
        comb.next_state = state_in.state;
        break;
    }

    // 1.4 Stage 1 Calculation
    if (state_in.state == S_IDLE && inp.pred_req) {
      comb.s1_calc = TAGE_pred_comb_1(inp.pc_pred_in, state_in.FH);
    }

    // 1.5 Stage 1 Calculation (计算更新写入值)
    comb.lsfr_out = LSFR_update_comb_1(state_in.LSFR);
    memset(&comb.upd_calc_res, 0, sizeof(UpdateRequest));

    if (comb.next_state == S_STAGE2) { // upd data read ready
      TageTableReadData old_data;
      old_data.base_cnt = mem.table_r.base_cnt;
      for (int i = 0; i < TN_MAX; ++i) {
        old_data.tag[i] = mem.table_r.tag[i];
        old_data.cnt[i] = mem.table_r.cnt[i];
        old_data.useful[i] = mem.table_r.useful[i];
      }

      PredResult last_pred;
      bool upd_real_dir;
      if(state_in.state == S_IDLE){ // from input
        upd_real_dir = inp.real_dir;
        last_pred.pred = inp.pred_in; 
        last_pred.alt_pred = inp.alt_pred_in;
        last_pred.pcpn = inp.pcpn_in;
        last_pred.altpcpn = inp.altpcpn_in;
        for (int i = 0; i < TN_MAX; ++i) {
          last_pred.index_info.tag[i] = inp.tage_tag_flat_in[i];
          last_pred.index_info.tage_index[i] = inp.tage_idx_flat_in[i]; // not used
        }
      } else if(state_in.state == S_IDLE_WAIT_DATA){ // from latch
        upd_real_dir = state_in.upd_real_dir_latch;
        last_pred.pred = state_in.upd_pred_in_latch;
        last_pred.alt_pred = state_in.upd_alt_pred_in_latch;
        last_pred.pcpn = state_in.upd_pcpn_in_latch;
        last_pred.altpcpn = state_in.upd_altpcpn_in_latch;
        for (int i = 0; i < TN_MAX; ++i) {
          last_pred.index_info.tag[i] = state_in.upd_tage_tag_flat_latch[i];
          last_pred.index_info.tage_index[i] = state_in.upd_tage_idx_flat_latch[i];
        }
      }
      comb.upd_calc_res =
          TAGE_update_comb_1(upd_real_dir, last_pred, old_data,
                             comb.lsfr_out.random_val, state_in.reset_cnt_reg);
    }

    // 1.6 Stage 2 Calculation (计算预测值)
    if ((state_in.state == S_STAGE2 || state_in.state == S_STAGE2_WAIT_DATA) &&
        comb.next_state == S_IDLE) { // pred data read ready
      TageTableReadData s2_r_data;
      s2_r_data.base_cnt = mem.table_r.base_cnt;
      for (int i = 0; i < TN_MAX; ++i) {
        s2_r_data.tag[i] = mem.table_r.tag[i];
        s2_r_data.cnt[i] = mem.table_r.cnt[i];
        s2_r_data.useful[i] = mem.table_r.useful[i]; // not used
      }
      TageIndexTag s2_idx_tag;
      s2_idx_tag.base_idx = state_in.pred_calc_base_idx_latch;
      for (int i = 0; i < TN_MAX; ++i) {
        s2_idx_tag.index_info.tag[i] =
            state_in.pred_calc_tage_tag_latch[i]; // only tag is used
        s2_idx_tag.index_info.tage_index[i] = state_in.pred_calc_tage_idx_latch[i];
      }
      comb.s2_comb_res = TAGE_pred_comb_2(s2_r_data, s2_idx_tag);
    }

    // 1.7 Next Global Regs Calculation
    // GHR/FH更新已迁移到BPU_TOP统一管理，以下代码已注释
    // TAGE不再负责GHR/FH的更新，只负责表项的更新
#if 0
    // GHR_update_comb_1(state_in.GHR, state_in.upd_real_dir_latch, comb.next_GHR);
    // FH_update_comb_1(state_in.FH, comb.next_GHR, state_in.upd_real_dir_latch,
    //                  comb.next_FH, fh_length, ghr_length);
    // A. 准备工作：判断当前正在发生什么
    bool is_predicting = (state_in.state != S_IDLE && comb.next_state == S_IDLE && state_in.do_pred_latch);
    bool is_updating   = (state_in.state != S_IDLE && comb.next_state == S_IDLE && state_in.do_upd_latch);
    
    // 检测误预测 (仅在更新有效时更有意义)
    bool misprediction = is_updating && (state_in.upd_pred_in_latch != state_in.upd_real_dir_latch);

    // B. 计算 下一拍的 Architectural State (总是基于 real_dir 更新)
    if (is_updating) {
        GHR_update_comb_1(Arch_GHR, state_in.upd_real_dir_latch, comb.next_Arch_GHR);
        FH_update_comb_1(Arch_FH, Arch_GHR, state_in.upd_real_dir_latch, 
                         comb.next_Arch_FH, fh_length, ghr_length);
    } else {
        // 保持不变
        memcpy(comb.next_Arch_GHR, Arch_GHR, sizeof(Arch_GHR));
        memcpy(comb.next_Arch_FH, Arch_FH, sizeof(Arch_FH));
    }

    // C. 计算 Speculative State 的 "候选值" (基于预测结果)
    bool spec_GHR_cand[GHR_LENGTH];
    uint32_t spec_FH_cand[FH_N_MAX][TN_MAX];
    
    if (is_predicting) {
        // 使用预测结果 (comb.s2_comb_res.pred) 进行推测更新
        GHR_update_comb_1(state_in.GHR, comb.s2_comb_res.pred, spec_GHR_cand);
        FH_update_comb_1(state_in.FH, state_in.GHR, comb.s2_comb_res.pred, 
                         spec_FH_cand, fh_length, ghr_length);
    }

    // D. 决定最终的 Speculative GHR/FH (Recovery Mux)
    // 优先级：Misprediction Recovery > Speculative Update > Hold
    
    if (misprediction) {
        // CASE 1: 发生误预测，强制回滚到 (Arch_Old + real_dir)
        // 注意：这里直接使用刚才计算好的 next_Arch 作为修正后的 Spec 状态
        memcpy(comb.next_Spec_GHR, comb.next_Arch_GHR, sizeof(Arch_GHR));
        memcpy(comb.next_Spec_FH, comb.next_Arch_FH, sizeof(Arch_FH));
    } 
    else if (is_predicting) {
        // CASE 2: 正常预测，进行推测更新
        memcpy(comb.next_Spec_GHR, spec_GHR_cand, sizeof(Spec_GHR));
        memcpy(comb.next_Spec_FH, spec_FH_cand, sizeof(Spec_FH));
    } 
    else {
        // CASE 3: 保持 Spec 状态不变 (可能是单纯的更新且预测正确，或者是空闲)
        // 注意：如果 update 且预测正确，其实不需要动 Spec GHR，因为它已经在当初预测时更新过了
        memcpy(comb.next_Spec_GHR, state_in.GHR, sizeof(Spec_GHR));
        memcpy(comb.next_Spec_FH, state_in.FH, sizeof(Spec_FH));
    }
#endif

    // 1.8 Output Logic
    // comb.out_regs.busy = (state_in.state != S_IDLE);

    if (state_in.state != S_IDLE &&
        comb.next_state == S_IDLE) { // moving to idle
      if (state_in.do_upd_latch) {
        comb.out_regs.tage_update_done = true;
      }
      if (state_in.do_pred_latch) {
        comb.out_regs.pred_out = comb.s2_comb_res.pred;
        comb.out_regs.alt_pred_out = comb.s2_comb_res.alt_pred;
        comb.out_regs.pcpn_out = comb.s2_comb_res.pcpn;
        comb.out_regs.altpcpn_out = comb.s2_comb_res.altpcpn;

        for (int i = 0; i < TN_MAX; ++i) {
          comb.out_regs.tage_tag_flat_out[i] = state_in.pred_calc_tage_tag_latch[i];
          comb.out_regs.tage_idx_flat_out[i] = state_in.pred_calc_tage_idx_latch[i];
        }
        comb.out_regs.tage_pred_out_valid = true;
      }
    }

    return comb;
  }

  // ------------------------------------------------------------------------
  // step_comb
  // ------------------------------------------------------------------------
  CombResult step_comb(const InputPayload &inp) {
    // 构建StateInput
    StateInput state_in;
    state_in.state = state;

    // regs - 通过接口函数从BPU读取全局GHR/FH（做一次快照）
#ifdef SPECULATIVE_ON
    const bool *ghr_src = BPU_get_Spec_GHR();
    const uint32_t (*fh_src)[TN_MAX] = BPU_get_Spec_FH();
#else
    const bool *ghr_src = BPU_get_Arch_GHR();
    const uint32_t (*fh_src)[TN_MAX] = BPU_get_Arch_FH();
#endif

    for (int k = 0; k < FH_N_MAX; ++k) {
      for (int i = 0; i < TN_MAX; ++i) {
        state_in.FH[k][i] = fh_src[k][i];
      }
    }
    for (int i = 0; i < GHR_LENGTH; ++i) {
      state_in.GHR[i] = ghr_src[i];
    }

    for (int i = 0; i < 4; ++i) {
      state_in.LSFR[i] = LSFR[i];
    }
    state_in.reset_cnt_reg = reset_cnt_reg;
    // input latches
    state_in.do_pred_latch = do_pred_latch;
    state_in.do_upd_latch = do_upd_latch;
    state_in.upd_real_dir_latch = upd_real_dir_latch;
    state_in.upd_pred_in_latch = upd_pred_in_latch;
    state_in.upd_alt_pred_in_latch = upd_alt_pred_in_latch;
    state_in.upd_pcpn_in_latch = upd_pcpn_in_latch;
    state_in.upd_altpcpn_in_latch = upd_altpcpn_in_latch;
    for (int i = 0; i < TN_MAX; ++i) {
      state_in.upd_tage_tag_flat_latch[i] = upd_tage_tag_flat_latch[i];
      state_in.upd_tage_idx_flat_latch[i] = upd_tage_idx_flat_latch[i];
    }
    // pipeline latches
    state_in.pred_calc_base_idx_latch = pred_calc_base_idx_latch;
    for (int i = 0; i < TN_MAX; ++i) {
      state_in.pred_calc_tage_idx_latch[i] = pred_calc_tage_idx_latch[i];
      state_in.pred_calc_tage_tag_latch[i] = pred_calc_tage_tag_latch[i];
    }

    IndexResult idx = step_comb_gen_index(inp, state_in);
    MemReadResult mem = step_comb_mem_read(idx);
    return step_comb_calc(inp, state_in, idx, mem);
  }

  // ------------------------------------------------------------------------
  // 时序逻辑函数 (Sequential Logic)
  // ------------------------------------------------------------------------
  void step_seq(bool rst_n, const InputPayload &inp, const CombResult &comb) {

#ifdef SRAM_DELAY_ENABLE
    if (sram_new_req_this_cycle && !sram_delay_active) {
      sram_delay_active = true;
      sram_delay_counter = delay_dist(rng); 
    }  
    if (sram_delay_active) {
      if (sram_delay_counter > 0) {
        sram_delay_counter--;
      } 
    }
#endif
    // Latch Requests
    if (state == S_IDLE && comb.next_state != S_IDLE) { // moving out from IDLE
      do_pred_latch =inp.pred_req; // this will only be changed here, latched!
      
      do_upd_latch = inp.update_en;
      upd_real_dir_latch = inp.real_dir;
      upd_pc_latch = inp.pc_update_in;
      upd_pred_in_latch = inp.pred_in;
      upd_alt_pred_in_latch = inp.alt_pred_in;
      upd_pcpn_in_latch = inp.pcpn_in;
      upd_altpcpn_in_latch = inp.altpcpn_in;
      for (int i = 0; i < TN_MAX; ++i) {
        upd_tage_idx_flat_latch[i] = inp.tage_idx_flat_in[i];
        upd_tage_tag_flat_latch[i] = inp.tage_tag_flat_in[i];
      }

      pred_calc_base_idx_latch = comb.s1_calc.base_idx; // we may latch 0 but this will not cause any problem
      
      for (int i = 0; i < TN_MAX; ++i) {
        pred_calc_tage_idx_latch[i] = comb.s1_calc.index_info.tage_index[i];
        pred_calc_tage_tag_latch[i] = comb.s1_calc.index_info.tag[i];
      }
    }

    if (comb.next_state == S_STAGE2) { // 使用next作为判据保证data_read_ready
      upd_calc_winfo_latch = comb.upd_calc_res;
    }

    // reset_cnt_reg++; // update every cycle

    // // Global Registers Update & Memory Write
    // if (state != S_IDLE && comb.next_state == S_IDLE && do_upd_latch) {
    //   reset_cnt_reg++; // update every upd
    //   for (int i = 0; i < GHR_LENGTH; ++i) {
    //     GHR[i] = comb.next_GHR[i];
    //   }
    //   for (int k = 0; k < FH_N_MAX; ++k) {
    //     for (int i = 0; i < TN_MAX; ++i) {
    //       FH[k][i] = comb.next_FH[k][i];
    //     }
    //   }
    //   for (int i = 0; i < 4; ++i) {
    //     LSFR[i] = comb.lsfr_out.next_state[i];
    //   }
    //   if (upd_calc_winfo_latch.base_we) {
    //     uint32_t wr_base = (upd_pc_latch >> 2) & (TAGE_BASE_IDX_MASK);
    //     base_counter[wr_base] = upd_calc_winfo_latch.base_wdata;
    //   }

    //   for (int i = 0; i < TN_MAX; i++) {
    //     uint32_t wr_idx = upd_tage_idx_flat_latch[i];
    //     if (upd_calc_winfo_latch.cnt_we[i]) {
    //       cnt_table[i][wr_idx] = upd_calc_winfo_latch.cnt_wdata[i];
    //     }
    //     if (upd_calc_winfo_latch.useful_we[i]) {
    //       useful_table[i][wr_idx] = upd_calc_winfo_latch.useful_wdata[i];
    //     }
    //     if (upd_calc_winfo_latch.tag_we[i]) {
    //       tag_table[i][wr_idx] = upd_calc_winfo_latch.tag_wdata[i];
    //     }

    //     if (upd_calc_winfo_latch.reset_we) {
    //       uint32_t row = upd_calc_winfo_latch.reset_row_idx;
    //       if (upd_calc_winfo_latch.reset_msb_only) {
    //         useful_table[i][row] &= 0x1;
    //       } else {
    //         useful_table[i][row] &= 0x2;
    //       }
    //     }
    //   }
    // }

    // Global Registers Update & Memory Write
    // GHR/FH更新已迁移到BPU_TOP，TAGE只负责表项和LSFR的更新
    if (state != S_IDLE && comb.next_state == S_IDLE) { // Condition loose to allow pred update
      
      // GHR/FH更新已迁移到BPU_TOP，以下代码已注释
#if 0
      // 1. Update Speculative State (FH/GHR)
      // 这里的逻辑现在由 comb 计算好的 next_GHR/next_FH 决定 (包含恢复逻辑)
      // 只要是 Predict 或 Update 完成，都允许写入（因为可能涉及恢复或推测）
      if (do_pred_latch || do_upd_latch) {
          for (int i = 0; i < GHR_LENGTH; ++i) {
            Spec_GHR[i] = comb.next_Spec_GHR[i];
          }
          for (int k = 0; k < FH_N_MAX; ++k) {
            for (int i = 0; i < TN_MAX; ++i) {
              Spec_FH[k][i] = comb.next_Spec_FH[k][i];
            }
          }
      }

      // 2. Update Architectural State (Arch_GHR/Arch_FH)
      // 仅在 Update 发生时写入
      if (do_upd_latch) {
        reset_cnt_reg++; // update every upd
        for (int i = 0; i < GHR_LENGTH; ++i) {
          Arch_GHR[i] = comb.next_Arch_GHR[i];
        }
        for (int k = 0; k < FH_N_MAX; ++k) {
          for (int i = 0; i < TN_MAX; ++i) {
            Arch_FH[k][i] = comb.next_Arch_FH[k][i];
          }
        }
      }
#endif
      // 只保留LSFR和表项的更新
      if (do_upd_latch) {
        reset_cnt_reg++; // update every upd
        for (int i = 0; i < 4; ++i) {
          LSFR[i] = comb.lsfr_out.next_state[i];
        }
        if (upd_calc_winfo_latch.base_we) {
          uint32_t wr_base = (upd_pc_latch >> 2) & (TAGE_BASE_IDX_MASK);
          base_counter[wr_base] = upd_calc_winfo_latch.base_wdata;
        }

        for (int i = 0; i < TN_MAX; i++) {
          uint32_t wr_idx = upd_tage_idx_flat_latch[i];
          if (upd_calc_winfo_latch.cnt_we[i]) {
            cnt_table[i][wr_idx] = upd_calc_winfo_latch.cnt_wdata[i];
          }
          if (upd_calc_winfo_latch.useful_we[i]) {
            useful_table[i][wr_idx] = upd_calc_winfo_latch.useful_wdata[i];
          }
          if (upd_calc_winfo_latch.tag_we[i]) {
            tag_table[i][wr_idx] = upd_calc_winfo_latch.tag_wdata[i];
          }

          if (upd_calc_winfo_latch.reset_we) {
            uint32_t row = upd_calc_winfo_latch.reset_row_idx;
            if (upd_calc_winfo_latch.reset_msb_only) {
              useful_table[i][row] &= 0x1;
            } else {
              useful_table[i][row] &= 0x2;
            }
          }
        }
      }
    }

    // State Transition
    state = comb.next_state;

    return;
  }

  // ------------------------------------------------------------------------
  // 周期步进函数 (Cycle Step) - 保留作为兼容接口
  // 完全对应 Verilog 的 always @(posedge clk) 和组合逻辑
  // ------------------------------------------------------------------------
  OutputPayload step(bool rst_n, const InputPayload &inp) {
    if (rst_n) {
      reset();
      DEBUG_LOG("[TAGE_TOP] reset\n");
      OutputPayload out_reg_reset;
      std::memset(&out_reg_reset, 0, sizeof(OutputPayload));
      return out_reg_reset;
    }
    CombResult comb = step_comb(inp);
    step_seq(rst_n, inp, comb);
    comb.out_regs.busy = (state != S_IDLE); // 补丁
    return comb.out_regs;
  }

private:
  // ============================================================
  // 组合逻辑函数实现 (Internal Implementation)
  // ============================================================

  // [Comb 1]
  TageIndexTag TAGE_pred_comb_1(uint32_t PC,
                                const uint32_t FH_in[FH_N_MAX][TN_MAX]) {
    TageIndexTag out;
    memset(&out, 0, sizeof(TageIndexTag));

    out.base_idx = (PC >> 2) & (TAGE_BASE_IDX_MASK);
    for (int i = 0; i < TN_MAX; i++) {
      out.index_info.tag[i] =
          (FH_in[1][i] ^ FH_in[2][i] ^ (PC >> 2)) & (TAGE_TAG_MASK);
      out.index_info.tage_index[i] =
          (FH_in[0][i] ^ (PC >> 2)) & (TAGE_IDX_MASK);
    }
    return out;
  }

  // [Comb 2]
  PredResult TAGE_pred_comb_2(const TageTableReadData &read_data,
                              const TageIndexTag &idx_tag) {
    PredResult res;
    memset(&res, 0, sizeof(PredResult));

    res.index_info = idx_tag.index_info;

    bool base_pred = (read_data.base_cnt >= 2);
    int pcpn = TN_MAX;
    int altpcpn = TN_MAX;

    for (int i = TN_MAX - 1; i >= 0; i--) {
      if (read_data.tag[i] == idx_tag.index_info.tag[i]) {
        pcpn = i;
        break;
      }
    }
    for (int i = pcpn - 1; i >= 0; i--) {
      if (read_data.tag[i] == idx_tag.index_info.tag[i]) {
        altpcpn = i;
        break;
      }
    }

    if (altpcpn >= TN_MAX) {
      res.alt_pred = base_pred;
    } else {
      res.alt_pred = (read_data.cnt[altpcpn] >= 4);
    }
    res.pcpn = pcpn;
    res.altpcpn = altpcpn;

    if (pcpn >= TN_MAX) {
      res.pred = base_pred;
    } else {
      res.pred = (read_data.cnt[pcpn] >= 4);
    }
    return res;
  }

  // [Comb Update]
  UpdateRequest TAGE_update_comb_1(bool real_dir, const PredResult &pred_res,
                                   const TageTableReadData &read_vals,
                                   uint8_t lsfr_rand,
                                   uint32_t current_reset_cnt) {
    UpdateRequest req;
    memset(&req, 0, sizeof(UpdateRequest));

    bool pred_dir = pred_res.pred;
    int pcpn = pred_res.pcpn;

    // 1. Update Provider / Base
    if (pcpn < TN_MAX) {
      if (pred_dir != pred_res.alt_pred) {
        uint8_t new_u = read_vals.useful[pcpn];
        if (pred_dir == real_dir) {
          new_u = sat_inc_2bit(new_u);
        } else {
          new_u = sat_dec_2bit(new_u);
        }
        req.useful_we[pcpn] = true;
        req.useful_wdata[pcpn] = new_u;
      }
      uint8_t new_cnt = read_vals.cnt[pcpn];
      if (real_dir == true) {
        new_cnt = sat_inc_3bit(new_cnt);
      } else {
        new_cnt = sat_dec_3bit(new_cnt);
      }
      req.cnt_we[pcpn] = true;
      req.cnt_wdata[pcpn] = new_cnt;
    } else {
      int new_base = read_vals.base_cnt;
      if (real_dir == true) {
        new_base = sat_inc_2bit(new_base);
      } else {
        new_base = sat_dec_2bit(new_base);
      }
      req.base_we = true;
      req.base_wdata = new_base;
    }

    // 2. Allocation
    if (pred_dir != real_dir) {
      if (pcpn <= TN_MAX - 2 || pcpn == TN_MAX) {
        bool new_entry_found_j = false;
        int j_i = -1;
        bool new_entry_found_k = false;
        int k_i = -1;
        int start_search = (pcpn == TN_MAX) ? 0 : (pcpn + 1);

        for (int i = start_search; i < TN_MAX; i++) {
          if (read_vals.useful[i] == 0) {
            if (!new_entry_found_j) {
              new_entry_found_j = true;
              j_i = i;
              continue;
            } else {
              new_entry_found_k = true;
              k_i = i;
              break;
            }
          }
        }

        if (!new_entry_found_j) {
          for (int i = pcpn + 1; i < TN_MAX; i++) {
            req.useful_we[i] = true;
            req.useful_wdata[i] = sat_dec_2bit(read_vals.useful[i]);
          }
        } else {
          int target_i = -1;
          int random_pick = lsfr_rand % 3; // assumption: rand is 2 bits 0-3
          if (new_entry_found_k && random_pick == 0) {
            target_i = k_i;
          } else {
            target_i = j_i;
          }
          req.tag_we[target_i] = true;
          req.tag_wdata[target_i] = pred_res.index_info.tag[target_i];
          req.cnt_we[target_i] = true;
          req.cnt_wdata[target_i] = real_dir ? 4 : 3;
          req.useful_we[target_i] = true;
          req.useful_wdata[target_i] = 0;
        }
      }
    }

    // 3. Reset Logic
    uint32_t u_cnt = current_reset_cnt &
                     0x7ff; // we leave these numbers here intentionally...
    uint32_t row_cnt = (current_reset_cnt >> 11) & 0xfff;
    bool u_msb_reset = (current_reset_cnt >> 23) & 0x1;

    if (u_cnt == 0) {
      req.reset_we = true;
      req.reset_row_idx = row_cnt;
      req.reset_msb_only = u_msb_reset;
    }
    return req;
  }

  // [comb] LSFR Update
  LSFR_Output LSFR_update_comb_1(const bool current_LSFR[4]) {
    LSFR_Output out;
    bool bit0 = current_LSFR[0];
    bool bit3 = current_LSFR[3];
    bool feedback = bit0 ^ bit3;
    out.next_state[0] = feedback;
    for (int i = 1; i < 4; i++) {
      out.next_state[i] = current_LSFR[i - 1];
    }
    out.random_val = (out.next_state[0] << 1) | out.next_state[1];
    return out;
  }

  // [comb] GHR Update - 已提取为公共静态函数TAGE_GHR_update_comb_1，保留此函数作为兼容接口（内部调用公共函数）
  void GHR_update_comb_1(const bool current_GHR[GHR_LENGTH], bool real_dir,
                         bool next_GHR[GHR_LENGTH]) {
    TAGE_GHR_update_comb_1(current_GHR, real_dir, next_GHR);
  }

  // [comb] FH Update - 已提取为公共静态函数TAGE_FH_update_comb_1，保留此函数作为兼容接口（内部调用公共函数）
  void FH_update_comb_1(const uint32_t current_FH[FH_N_MAX][TN_MAX],
                        const bool current_GHR[GHR_LENGTH], bool new_history,
                        uint32_t next_FH[FH_N_MAX][TN_MAX],
                        const uint32_t fh_len[FH_N_MAX][TN_MAX],
                        const uint32_t ghr_len[TN_MAX]) {
    TAGE_FH_update_comb_1(current_FH, current_GHR, new_history, next_FH, fh_len, ghr_len);
  }
};

#endif // TAGE_TOP_H