#ifndef BPU_TOP_H
#define BPU_TOP_H

#include "../frontend.h"
#include "./dir_predictor/TAGE_top.h"
#include "./target_predictor/BTB_top.h"
#include "BPU_configs.h"
#include "SimCpu.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

class BPU_TOP;             // 前向声明类
extern BPU_TOP *g_bpu_top; // 再声明全局指针

uint32_t bpu_sim_time = 0; // only for debug

// comb helper function
int get_bank_sel(uint32_t pc) {
  int bank_sel = (pc >> 2) % BPU_BANK_NUM;
  return bank_sel;
}

// comb helper function
uint32_t get_bank_pc(uint32_t pc) {
  if((BPU_BANK_NUM & (BPU_BANK_NUM - 1)) != 0) {
    // return pc;
    uint32_t bank_pc = pc >> 2;
    bank_pc = bank_pc / BPU_BANK_NUM;
    bank_pc = bank_pc << 2;
    return bank_pc;
  }
  else{
    uint32_t n = BPU_BANK_NUM;
    int highest_bit_pos = 0;
    while (n > 1) {
      n >>= 1;
      highest_bit_pos++;
    }
    uint32_t bank_pc = pc >> highest_bit_pos; // 2 + highest_bit_pos - 2
    return bank_pc;
  }
}


class BPU_TOP {
  // 友元声明：允许TAGE_TOP访问BPU_TOP的私有成员（用于读取GHR/FH）
  friend class TAGE_TOP;
  friend const bool* BPU_get_Arch_GHR();
  friend const bool* BPU_get_Spec_GHR();
  friend const uint32_t (*BPU_get_Arch_FH())[TN_MAX];
  friend const uint32_t (*BPU_get_Spec_FH())[TN_MAX];

public:
  struct InputPayload {
    // I-Cache & Backend Control
    bool refetch;
    uint32_t refetch_address;
    bool icache_read_ready;

    // Update Interface
    uint32_t in_update_base_pc[COMMIT_WIDTH];
    bool in_upd_valid[COMMIT_WIDTH];
    bool in_actual_dir[COMMIT_WIDTH];
    uint8_t in_actual_br_type[COMMIT_WIDTH]; // 3-bit each
    uint32_t in_actual_targets[COMMIT_WIDTH];

    bool in_pred_dir[COMMIT_WIDTH];
    bool in_alt_pred[COMMIT_WIDTH];
    uint8_t in_pcpn[COMMIT_WIDTH];    // 3-bit each
    uint8_t in_altpcpn[COMMIT_WIDTH]; // 3-bit each
    uint8_t in_tage_tags[COMMIT_WIDTH][TN_MAX];
    uint32_t in_tage_idxs[COMMIT_WIDTH][TN_MAX];
  };

  struct OutputPayload {
    uint32_t fetch_address;
    bool icache_read_valid;
    uint32_t predict_next_fetch_address;
    bool PTAB_write_enable;
    bool out_pred_dir[FETCH_WIDTH];
    bool out_alt_pred[FETCH_WIDTH];
    uint8_t out_pcpn[FETCH_WIDTH];
    uint8_t out_altpcpn[FETCH_WIDTH];
    uint8_t out_tage_tags[FETCH_WIDTH][TN_MAX];
    uint32_t out_tage_idxs[FETCH_WIDTH][TN_MAX];
    uint32_t out_pred_base_pc; // used for predecode
    bool update_queue_full;
    // 2-Ahead Predictor outputs
    // 下下行取指地址
    uint32_t two_ahead_target;
    // 指示要不要多消耗inst FIFO
    bool mini_flush_req;
    // 指示要不要多写一次fetch_address_fifo
    bool mini_flush_correct;
    // 如果要多写，多写的地址
    uint32_t mini_flush_target;
  };

private:
  // ========================================================================
  // 内部数据结构 (Internal Structures)
  // ========================================================================

  // Update Queue Entry Structure --- for one bank slot!
  struct QueueEntry {
    uint32_t base_pc;
    bool valid_mask;
    bool actual_dir;
    uint8_t br_type;
    uint32_t targets;
    bool pred_dir;
    bool alt_pred;
    uint8_t pcpn;
    uint8_t altpcpn;
    uint8_t tage_tags[TN_MAX];
    uint32_t tage_idxs[TN_MAX];
  };

  // 2-Ahead Predictor Structures
  struct SimpleAheadEntry {
    bool valid;
    bool taken;
    uint32_t target;
  };

  struct LastBlockEntry {
    bool valid;
    uint32_t last_pc;
  };

  enum State {
    S_IDLE = 0,
    S_WORKING = 1,  // TAGE和BTB并行执行
    S_REFEATCH = 2  // refetch时的并行更新
  };

  // ========================================================================
  // Registers & Memory
  // ========================================================================

  // Global History Registers & Folded Histories (Arch + Spec)
  // GHR/FH从TAGE迁移到BPU统一管理
  bool Arch_GHR[GHR_LENGTH];
  bool Spec_GHR[GHR_LENGTH];
  uint32_t Arch_FH[FH_N_MAX][TN_MAX];
  uint32_t Spec_FH[FH_N_MAX][TN_MAX];

  // FH constants (从TAGE复制，用于调用FH_update函数)
  const uint32_t ghr_length[TN_MAX] = {8, 13, 32, 119};
  const uint32_t fh_length[FH_N_MAX][TN_MAX] = {
      {8, 11, 11, 11}, {8, 8, 8, 8}, {7, 7, 7, 7}};

  // PC & Memory
  uint32_t pc_reg;
  uint8_t inst_type_mem[BPU_TYPE_ENTRY_NUM]
                       [BPU_BANK_NUM]; // 3-bit stored in uint8

  State state;
  // Transaction Flags (Latched in IDLE) // doing prediction and updating
  bool do_pred_latch;
  bool do_upd_latch[BPU_BANK_NUM]; // indicate whether to update this bank

  bool pc_can_send_to_icache;            // 当前pc是不是可以发送给icache
  uint32_t pred_base_pc_fired; // 当前预测流程中正在处理的pc基地址

  // 存储TAGE和BTB的预测结果（缓存先到的结果）对于FETCH_WIDTH建立！
  bool tage_calc_pred_dir_latch[FETCH_WIDTH];
  bool tage_calc_altpred_latch[FETCH_WIDTH];
  uint8_t tage_calc_pcpn_latch[FETCH_WIDTH];
  uint8_t tage_calc_altpcpn_latch[FETCH_WIDTH];
  uint8_t tage_pred_calc_tags_latch[FETCH_WIDTH][TN_MAX];
  uint32_t tage_pred_calc_idxs_latch[FETCH_WIDTH][TN_MAX];
  bool tage_result_valid_latch[FETCH_WIDTH]; // 标记TAGE预测结果是否已缓存
  uint32_t btb_pred_target_latch[FETCH_WIDTH];
  bool btb_result_valid_latch[FETCH_WIDTH]; // 标记BTB预测结果是否已缓存

  // Done信号：标记每个bank的TAGE/BTB是否完成
  bool tage_done[BPU_BANK_NUM];  // TAGE完成信号
  bool btb_done[BPU_BANK_NUM];   // BTB完成信号

  // Queue Registers
  QueueEntry update_queue[Q_DEPTH][BPU_BANK_NUM];
  uint32_t q_wr_ptr[BPU_BANK_NUM];
  uint32_t q_rd_ptr[BPU_BANK_NUM];
  uint32_t q_count[BPU_BANK_NUM];

  // bool out_pred_dir_latch[FETCH_WIDTH];
  // bool out_alt_pred_latch[FETCH_WIDTH];
  // uint8_t out_pcpn_latch[FETCH_WIDTH];
  // uint8_t out_altpcpn_latch[FETCH_WIDTH];
  // uint8_t out_tage_pred_calc_tags_latch_latch[FETCH_WIDTH][TN_MAX];
  // uint32_t out_tage_pred_calc_idxs_latch_latch[FETCH_WIDTH][TN_MAX];

  // Sub-modules
  TAGE_TOP *tage_inst[BPU_BANK_NUM];
  BTB_TOP *btb_inst[BPU_BANK_NUM];

  // 2-Ahead Predictor Tables
  SimpleAheadEntry simple_ahead_table[BPU_BANK_NUM][TWO_AHEAD_TABLE_SIZE];
  LastBlockEntry last_block_table[BPU_BANK_NUM];
  
  // 2-Ahead Predictor Registers
  // 类似pc_reg的2-ahead reg,跟pc_reg保持同步
  uint32_t saved_2ahead_prediction;
  // 指示的是上一个的2-ahead预测器是否有效
  bool saved_2ahead_pred_valid; // may not used
  // goes to PTAB, 需要跟PTAB相关信号同步
  bool saved_mini_flush_req;
  // goes to fetch_address_FIFO, 需要跟fetch_address_FIFO相关信号同步
  bool saved_mini_flush_correct;
  uint32_t saved_mini_flush_target; // may not used


public:
  // ========================================================================
  // 构造与析构
  // ========================================================================
  BPU_TOP() {
    // 注册全局指针，让TAGE可以访问BPU的GHR/FH
    g_bpu_top = this;
    
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      tage_inst[i] = new TAGE_TOP();
      btb_inst[i] = new BTB_TOP();
    }

    // Initialize Memory
    std::memset(inst_type_mem, 0, sizeof(inst_type_mem));
    reset_internal_all();
  }

  ~BPU_TOP() {
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      delete tage_inst[i];
      delete btb_inst[i];
    }
  }

  void reset_internal_all() {
    DEBUG_LOG_SMALL_4("reset_internal_all\n");
    pc_reg = RESET_PC;
    pc_can_send_to_icache = true;
    pred_base_pc_fired = 0;

    state = S_IDLE;
    do_pred_latch = false;
    for (int i = 0; i < BPU_BANK_NUM; i++)
      do_upd_latch[i] = false;

    // 初始化全局GHR/FH（从TAGE迁移过来）
    std::memset(Arch_GHR, 0, sizeof(Arch_GHR));
    std::memset(Spec_GHR, 0, sizeof(Spec_GHR));
    std::memset(Arch_FH, 0, sizeof(Arch_FH));
    std::memset(Spec_FH, 0, sizeof(Spec_FH));

    for (int i = 0; i < BPU_BANK_NUM; i++) {
      q_wr_ptr[i] = 0;
      q_rd_ptr[i] = 0;
      q_count[i] = 0;
      tage_done[i] = false;
      btb_done[i] = false;
    }

    // 初始化预测结果数组
    for (int i = 0; i < FETCH_WIDTH; i++) {
      tage_calc_pred_dir_latch[i] = false;
      tage_calc_altpred_latch[i] = false;
      tage_calc_pcpn_latch[i] = 0;
      tage_calc_altpcpn_latch[i] = 0;
      tage_result_valid_latch[i] = false;
      btb_pred_target_latch[i] = 0;
      btb_result_valid_latch[i] = false;
      for (int k = 0; k < TN_MAX; k++) {
        tage_pred_calc_tags_latch[i][k] = 0;
        tage_pred_calc_idxs_latch[i][k] = 0;
      }
    }
    
    // 初始化done信号
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      tage_done[i] = false;
      btb_done[i] = false;
    }

    // 初始化2-Ahead预测器
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      for (int j = 0; j < TWO_AHEAD_TABLE_SIZE; j++) {
        simple_ahead_table[i][j].valid = false;
        simple_ahead_table[i][j].taken = false;
        simple_ahead_table[i][j].target = 0;
      }
      last_block_table[i].valid = false;
      last_block_table[i].last_pc = 0;
    }
    // 初始化2-ahead预测器为下一个cache line的地址
    DEBUG_LOG_SMALL_4("reset_internal_all,pc_reg: %x\n", pc_reg);
    saved_2ahead_prediction = pc_reg + (FETCH_WIDTH * 4);
    DEBUG_LOG_SMALL_4("reset_internal_all,saved_2ahead_prediction: %x\n", saved_2ahead_prediction);
    // 初始化2-ahead预测器为有效
    saved_2ahead_pred_valid = true;
    saved_mini_flush_req = false;
    saved_mini_flush_correct = false;
    saved_mini_flush_target = 0;
  }

  // ========================================================================
  // 主 Step 函数
  // ========================================================================
  OutputPayload step(bool clk, bool rst_n, const InputPayload &inp) {
    DEBUG_LOG_SMALL_4("BPU_TOP step,saved_2ahead_prediction: %x\n", saved_2ahead_prediction);
    OutputPayload out_reg;
    std::memset(&out_reg, 0, sizeof(OutputPayload));

    bpu_sim_time++;
    if (rst_n) {
      reset_internal_all();
      DEBUG_LOG("[BPU_TOP] reset\n");
      // out_reg.fetch_address = RESET_PC;
      extern SimCpu cpu;
      out_reg.fetch_address = cpu.back.number_PC;
      // 初始化2-ahead预测器为下一个cache line的地址
      out_reg.two_ahead_target = out_reg.fetch_address + (FETCH_WIDTH * 4);
      // printf("reset to pc: %lx\n", out_reg.fetch_address);
      return out_reg;
      // return out_reg;
    }

    uint32_t pred_base_pc;
    if (state == S_IDLE) {
      pred_base_pc = inp.refetch ? inp.refetch_address : pc_reg;
    } else {
      pred_base_pc = pred_base_pc_fired;
    }

    const uint32_t CACHE_MASK = ~(ICACHE_LINE_SIZE - 1);
    uint32_t pc_plus_width = pred_base_pc + (FETCH_WIDTH * 4); // 32-bit PC
    uint32_t boundary_addr =
        ((pred_base_pc & CACHE_MASK) != (pc_plus_width & CACHE_MASK))
            ? (pc_plus_width & CACHE_MASK)
            : pc_plus_width;

    bool do_pred_on_this_pc[FETCH_WIDTH];
    int this_pc_bank_sel[FETCH_WIDTH];  // Changed from uint32_t to int to properly handle -1
    uint32_t do_pred_for_this_pc[FETCH_WIDTH];
    for (int i = 0; i < FETCH_WIDTH; i++) {
      do_pred_for_this_pc[i] = pred_base_pc + (i * 4);
      if (do_pred_for_this_pc[i] < boundary_addr) {
        this_pc_bank_sel[i] = get_bank_sel(do_pred_for_this_pc[i]);
        do_pred_on_this_pc[i] = true;
      } else {
        do_pred_on_this_pc[i] = false; // not in this cache line
        this_pc_bank_sel[i] = -1; // invalid bank sel
      }
    }

    // Queue status signals
    bool q_full[BPU_BANK_NUM];
    bool q_empty[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      q_full[i] = (q_count[i] == Q_DEPTH);
      q_empty[i] = (q_count[i] == 0);
      DEBUG_LOG_SMALL("[BPU_TOP] q_count[%d] = %d, q_full[%d] = %d, q_empty[%d] = %d\n", i, q_count[i], i, q_full[i], i, q_empty[i]);
    }

    QueueEntry q_data[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      q_data[i] = update_queue[q_rd_ptr[i]][i];
    }
    // Condition Check
    bool going_to_do_pred =
        inp.icache_read_ready || inp.refetch;  // refetch 也允许预测，只是要基于refetch_pc
    bool going_to_do_upd[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      going_to_do_upd[i] = !q_empty[i];
      // printf("%d ", q_count[i]);
    }
    // printf("\n");
    // bool going_to_do_upd_any = going_to_do_upd[0] || going_to_do_upd[1] ||
                              //  going_to_do_upd[2] || going_to_do_upd[3];
    bool going_to_do_upd_any = false;
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      going_to_do_upd_any |= going_to_do_upd[i];
    }
    
    bool trans_ready_to_fire = going_to_do_pred || going_to_do_upd_any; // 允许refetch直接fire

    TAGE_TOP::InputPayload tage_in[BPU_BANK_NUM];
    std::memset(tage_in, 0, sizeof(tage_in)); // Default 0

    // TAGE和BTB并行执行：在S_IDLE或S_WORKING状态同时设置输入
    // bool set_TAGE_input = (state == S_IDLE && trans_ready_to_fire) || 
    //                       (state == S_WORKING) || 
    //                       (state == S_REFEATCH);
    bool set_TAGE_input = state == S_IDLE && trans_ready_to_fire; // only set input in IDLE state
    if (set_TAGE_input) {
      // Prediction Request - 同时对4条PC进行预测
      // 在S_IDLE状态直接设置输入，不检查busy（这是新的请求）
      if (going_to_do_pred) {
        for (int i = 0; i < FETCH_WIDTH; i++) {
          if (do_pred_on_this_pc[i]) {
            int bank_sel = this_pc_bank_sel[i];
            // 添加边界检查，确保bank_sel在有效范围内
            if (bank_sel >= 0 && bank_sel < BPU_BANK_NUM) {
              tage_in[bank_sel].pred_req = true;
              tage_in[bank_sel].pc_pred_in = get_bank_pc(do_pred_for_this_pc[i]);
            }
          }
        }
      }
      // Update Request
      for (int i = 0; i < BPU_BANK_NUM; i++) {
        if (going_to_do_upd[i]) {
          uint32_t u_pc = q_data[i].base_pc;

          bool is_cond_upd = (q_data[i].br_type == BR_DIRECT);

          if (is_cond_upd) {
            tage_in[i].update_en = q_data[i].valid_mask;
            tage_in[i].pc_update_in = get_bank_pc(u_pc);
            tage_in[i].real_dir = q_data[i].actual_dir;
            tage_in[i].pred_in = q_data[i].pred_dir;
            tage_in[i].alt_pred_in = q_data[i].alt_pred;
            tage_in[i].pcpn_in = q_data[i].pcpn;
            tage_in[i].altpcpn_in = q_data[i].altpcpn;

            for (int k = 0; k < 4; k++) { // TN_MAX=4
              tage_in[i].tage_tag_flat_in[k] = q_data[i].tage_tags[k];
              tage_in[i].tage_idx_flat_in[k] = q_data[i].tage_idxs[k];
            }
          }
        }
      }
    }

    // --- Prepare BTB Inputs - 并行执行，不再等待TAGE完成 ---
    BTB_TOP::InputPayload btb_in[BPU_BANK_NUM];
    std::memset(btb_in, 0, sizeof(btb_in));

    // BTB同时对4条PC进行预测
    // bool set_BTB_input = (state == S_IDLE && trans_ready_to_fire) || 
    //                     (state == S_WORKING) || 
    //                     (state == S_REFEATCH);
    bool set_BTB_input = set_TAGE_input;
    if (set_BTB_input) {
      // Prediction Request - 同时对4条PC进行预测
      // 在S_IDLE状态直接设置输入，不检查busy（这是新的请求）
      if (going_to_do_pred) {
        for (int i = 0; i < FETCH_WIDTH; i++) {
          if (do_pred_on_this_pc[i]) {
            int bank_sel = this_pc_bank_sel[i];
            // 添加边界检查，确保bank_sel在有效范围内
            if (bank_sel >= 0 && bank_sel < BPU_BANK_NUM) {
              btb_in[bank_sel].pred_req = true;
              btb_in[bank_sel].pred_pc = get_bank_pc(do_pred_for_this_pc[i]);
            }
          }
        }
      }

      // Update Logic Setup
      for (int i = 0; i < BPU_BANK_NUM; i++) {
        if (going_to_do_upd[i]) {
          btb_in[i].upd_valid = q_data[i].valid_mask;
          btb_in[i].upd_pc = get_bank_pc(q_data[i].base_pc);
          btb_in[i].upd_actual_addr = q_data[i].targets;
          btb_in[i].upd_actual_dir = q_data[i].actual_dir;
          btb_in[i].upd_br_type_in = q_data[i].br_type;
        }
      }
    }

    // i对应第i个bank_sel上的子模块
    // --- Run Submodules (Cycle Step) ---
    TAGE_TOP::OutputPayload tage_out[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      tage_out[i] = tage_inst[i]->step(rst_n, tage_in[i]);
    }
    // --- Run BTB Submodule ---
    BTB_TOP::OutputPayload btb_out[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      btb_out[i] = btb_inst[i]->step(rst_n, btb_in[i]);
    }

    // 在S_WORKING或S_REFEATCH状态，检测busy为低且done为低时，拉高done并缓存预测结果
    if (state == S_WORKING || state == S_REFEATCH) {
      for (int i = 0; i < BPU_BANK_NUM; i++) {
        // TAGE: 如果done为低且busy为低，拉高done并缓存预测结果
        if (!tage_done[i] && !tage_out[i].busy) {
          tage_done[i] = true;
          
          // 缓存TAGE预测结果,检查哪个PC对应到当前banksel
          for (int j = 0; j < FETCH_WIDTH; j++) {
            if (do_pred_on_this_pc[j] && this_pc_bank_sel[j] == i && !tage_result_valid_latch[j]) {
              tage_calc_pred_dir_latch[j] = tage_out[i].pred_out;
              tage_calc_altpred_latch[j] = tage_out[i].alt_pred_out;
              tage_calc_pcpn_latch[j] = tage_out[i].pcpn_out;
              tage_calc_altpcpn_latch[j] = tage_out[i].altpcpn_out;
              for (int k = 0; k < TN_MAX; k++) {
                tage_pred_calc_tags_latch[j][k] = tage_out[i].tage_tag_flat_out[k];
                tage_pred_calc_idxs_latch[j][k] = tage_out[i].tage_idx_flat_out[k];
              }
              tage_result_valid_latch[j] = true;
              break;
            }
          }
        }
        
        // BTB: 如果done为低且busy为低，拉高done并缓存预测结果
        if (!btb_done[i] && !btb_out[i].busy) {
          btb_done[i] = true;
          
          // 缓存BTB预测结果
          for (int j = 0; j < FETCH_WIDTH; j++) {
            if (do_pred_on_this_pc[j] && this_pc_bank_sel[j] == i && !btb_result_valid_latch[j]) {
              btb_pred_target_latch[j] = btb_out[i].pred_target;
              btb_result_valid_latch[j] = true;
              break;
            }
          }
        }
      }
    }

    // 检查所有需要的预测结果是否都已缓存（基于done信号）
    bool all_tage_ready = true;
    bool all_btb_ready = true;
    if (do_pred_latch) {
      for (int i = 0; i < FETCH_WIDTH; i++) {
        if (do_pred_on_this_pc[i]) {
          int bank_sel = this_pc_bank_sel[i];
          // 添加边界检查，确保bank_sel在有效范围内
          if (bank_sel >= 0 && bank_sel < BPU_BANK_NUM) {
            // TAGE完成：结果已缓存且done为高
            if (!tage_result_valid_latch[i] || !tage_done[bank_sel]) {
              all_tage_ready = false;
            }
            // BTB完成：结果已缓存且done为高
            if (!btb_result_valid_latch[i] || !btb_done[bank_sel]) {
              all_btb_ready = false;
            }
          }
        }
      }
    }
    
    // 检查所有更新操作是否完成
    bool all_upd_ready = true;
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      if (do_upd_latch[i]) {
        if (!tage_done[i] || !btb_done[i]) {
          all_upd_ready = false;
        }
      }
    }

    // 状态转换逻辑：基于完成寄存器判断是否所有操作都完成
    State next_state = state;
    bool all_ops_done = (do_pred_latch ? (all_tage_ready && all_btb_ready) : true) && 
                        all_upd_ready;
    
    switch (state) {
      case S_IDLE: // IDLE
        if (trans_ready_to_fire) {
          next_state = S_WORKING;
        } else {
          next_state = S_IDLE;
        }
        break;
      case S_WORKING: // TAGE和BTB并行执行
        if (inp.refetch) {
          // 遇到refetch，转到refetch预测状态
          if (all_ops_done)
            next_state = S_IDLE;
          else
            next_state = S_REFEATCH; // 等待完成
        } else if (all_ops_done) {
          // 所有操作都完成，回到IDLE
          next_state = S_IDLE;
        } else {
          next_state = S_WORKING; // 等待完成
        }
        break;
      case S_REFEATCH: // refetch时的并行预测
        if (all_ops_done) {
          next_state = S_IDLE;
        } else {
          next_state = S_REFEATCH; // 等待完成
        }
        break;
    }
    
    // 预测合并逻辑：根据TAGE预测、BTB预测和指令类型决定最终预测
    // 只有在所有预测结果都已缓存且完成寄存器都标记为完成时才进行合并
    bool final_pred_dir[FETCH_WIDTH];
    uint32_t next_fetch_addr_calc = boundary_addr;
    bool found_taken_branch = false;
    // 最终提供预测的first taken pc以及其bank
    // for 2-Ahead
    int final_bank = -1;
    uint32_t final_pc = 0;
    uint32_t final_2_ahead_address = next_fetch_addr_calc + (FETCH_WIDTH * 4);

    if ((state == S_WORKING) && (next_state == S_IDLE) && !inp.refetch) { // 防止refetch污染推测更新
      for (int i = 0; i < FETCH_WIDTH; i++) {
        if (do_pred_on_this_pc[i]) {
          int bank_sel = this_pc_bank_sel[i];
          // 添加边界检查，确保bank_sel在有效范围内
          if (bank_sel >= 0 && bank_sel < BPU_BANK_NUM) {
            int type_idx = get_bank_pc(do_pred_for_this_pc[i]) & BPU_TYPE_IDX_MASK;
            uint8_t p_type = inst_type_mem[type_idx][bank_sel];

            // 根据指令类型决定最终预测方向
            if (p_type == BR_NONCTL) {
              final_pred_dir[i] = false;
            } else if (p_type == BR_RET || p_type == BR_CALL || 
                       p_type == BR_IDIRECT || p_type == BR_JAL) {
              // 无条件分支，总是taken
              final_pred_dir[i] = true;
            } else {
              // 条件分支，使用TAGE的方向预测
              final_pred_dir[i] = tage_calc_pred_dir_latch[i];
            }

            // 找到第一个taken的分支，使用BTB的target
            if (final_pred_dir[i] && !found_taken_branch && btb_result_valid_latch[i]) {
              found_taken_branch = true;
              next_fetch_addr_calc = btb_pred_target_latch[i];
              final_bank = bank_sel;
              final_pc = get_bank_pc(do_pred_for_this_pc[i]);
              // 此时不能break，继续修正所有final_pred_dir的值
            }
          } else {
            // bank_sel无效，使用默认值
            final_pred_dir[i] = false;
          }
        }
      }

      // ========================================================================
      // 4.1 取指阶段：对FETCH_WIDTH条指令进行Spec GHR/FH更新
      // ========================================================================
      // 本周期开始时的Spec状态（快照）
      bool spec_ghr_tmp[GHR_LENGTH];
      uint32_t spec_fh_tmp[FH_N_MAX][TN_MAX];
      std::memcpy(spec_ghr_tmp, Spec_GHR, sizeof(Spec_GHR));
      std::memcpy(spec_fh_tmp, Spec_FH, sizeof(Spec_FH));

      // 顺序遍历本次fetch的每个slot，相当于执行FETCH_WIDTH次单步更新
      for (int i = 0; i < FETCH_WIDTH; ++i) {
        if (!do_pred_on_this_pc[i]) continue;

        // 只对条件分支更新历史（逻辑参考update阶段的is_cond_upd == BR_DIRECT）
        int bank_sel = this_pc_bank_sel[i];
        if (bank_sel < 0 || bank_sel >= BPU_BANK_NUM) continue;
        
        uint32_t pc = do_pred_for_this_pc[i];
        int type_idx = get_bank_pc(pc) & BPU_TYPE_IDX_MASK;
        uint8_t p_type = inst_type_mem[type_idx][bank_sel];
        bool is_cond = (p_type == BR_DIRECT);

        if (!is_cond) continue; // 只对条件分支更新历史

        bool new_bit = final_pred_dir[i]; // 用预测方向做推测更新
        bool next_ghr[GHR_LENGTH];
        uint32_t next_fh[FH_N_MAX][TN_MAX];
        
        TAGE_GHR_update_comb_1(spec_ghr_tmp, new_bit, next_ghr);
        TAGE_FH_update_comb_1(spec_fh_tmp, spec_ghr_tmp, new_bit,
                              next_fh, fh_length, ghr_length);
        
        std::memcpy(spec_ghr_tmp, next_ghr, sizeof(next_ghr));
        std::memcpy(spec_fh_tmp, next_fh, sizeof(next_fh));
      }

      // 更新全局Spec状态
      std::memcpy(Spec_GHR, spec_ghr_tmp, sizeof(Spec_GHR));
      std::memcpy(Spec_FH, spec_fh_tmp, sizeof(Spec_FH));

      // ========================================================================
      // 2-Ahead预测逻辑
      // ========================================================================
    
      if(found_taken_branch == false) {
        final_2_ahead_address = next_fetch_addr_calc + (4 * FETCH_WIDTH);
      } else {
        uint32_t index = final_pc % TWO_AHEAD_TABLE_SIZE;
        SimpleAheadEntry &entry = simple_ahead_table[final_bank][index];
        if (entry.taken && entry.valid) {
          final_2_ahead_address = entry.target;
        } else {
          final_2_ahead_address = next_fetch_addr_calc + (FETCH_WIDTH * 4);
        }
      }
    } else {
      // 其他情况，使用默认值
      for (int i = 0; i < FETCH_WIDTH; i++) {
        final_pred_dir[i] = false;
      }
    }

    // Queue control signals
    bool q_push_en[BPU_BANK_NUM];
    bool q_pop_en[BPU_BANK_NUM];
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      q_pop_en[i] =
          ((state == S_WORKING || state == S_REFEATCH) && 
           (next_state == S_IDLE) && do_upd_latch[i]); // upd done, able to move ptr
      q_push_en[i] = false; // 给个初始值
    }
    for(int i = 0; i < COMMIT_WIDTH; i++) {
      if(!inp.in_upd_valid[i])
        continue;
      int bank_sel = get_bank_sel(inp.in_update_base_pc[i]);
      q_push_en[bank_sel] = !q_full[bank_sel];
    }

    // output logic
    if ((state == S_WORKING) && next_state == S_IDLE) {
      out_reg.PTAB_write_enable = do_pred_latch && !inp.refetch; // 在预测完成的这周期刚好遇到refetch，无效掉此前预测
    }
    out_reg.icache_read_valid = pc_can_send_to_icache && (state == S_IDLE);
    out_reg.fetch_address =
        inp.refetch ? inp.refetch_address : pc_reg; // 刚好当前拍有refetch,此时pc_reg还没更新，直接从输入拿

    out_reg.predict_next_fetch_address = next_fetch_addr_calc; // 使用合并后的预测结果
    for (int i = 0; i < FETCH_WIDTH; i++) {
      out_reg.out_pred_dir[i] = final_pred_dir[i]; // 使用合并后的预测方向
      out_reg.out_alt_pred[i] = tage_calc_altpred_latch[i];
      out_reg.out_pcpn[i] = tage_calc_pcpn_latch[i];
      out_reg.out_altpcpn[i] = tage_calc_altpcpn_latch[i];
      for (int k = 0; k < TN_MAX; k++) {
        out_reg.out_tage_tags[i][k] = tage_pred_calc_tags_latch[i][k];
        out_reg.out_tage_idxs[i][k] = tage_pred_calc_idxs_latch[i][k];
      }
    }
    out_reg.out_pred_base_pc = pred_base_pc_fired;

    // 2-Ahead Predictor outputs
    uint32_t refetch_2ahead_target = inp.refetch_address + (FETCH_WIDTH * 4);
    out_reg.two_ahead_target = inp.refetch ? refetch_2ahead_target : saved_2ahead_prediction;

    // 对应事件要发生在对应的处理行为上
    bool need_mini_flush = saved_2ahead_prediction != next_fetch_addr_calc;
    out_reg.mini_flush_req = need_mini_flush && out_reg.PTAB_write_enable;

    out_reg.mini_flush_correct = saved_mini_flush_correct && !inp.refetch;
    out_reg.mini_flush_target = saved_mini_flush_target; 

#ifndef ENABLE_2AHEAD
    out_reg.mini_flush_req = false;
    out_reg.mini_flush_correct = false;
#endif

    // DEBUG LOG SMALL 输出所有inp和output，以及sim_time，state和next state
    DEBUG_LOG_SMALL("[BPU_TOP] sim_time: %u, state: %d, next_state: %d\n",
                    bpu_sim_time, state, next_state);
    DEBUG_LOG_SMALL(
        "  inp: refetch=%d, refetch_addr=0x%x, icache_read_ready=%d\n",
        inp.refetch, inp.refetch_address, inp.icache_read_ready);
    for (int i = 0; i < COMMIT_WIDTH; i++) {
      if (inp.in_upd_valid[i]) {
        DEBUG_LOG_SMALL(
            "  inp[%d]: base_pc=0x%x, actual_dir=%d, br_type=%d, "
            "target=0x%x, pred_dir=%d, alt_pred=%d, pcpn=%d, altpcpn=%d\n",
            i, inp.in_update_base_pc[i], inp.in_actual_dir[i],
            inp.in_actual_br_type[i], inp.in_actual_targets[i],
            inp.in_pred_dir[i], inp.in_alt_pred[i], inp.in_pcpn[i],
            inp.in_altpcpn[i]);
      }
    }
    DEBUG_LOG_SMALL("  out: fetch_addr=0x%x, icache_read_valid=%d, "
                    "predict_next_fetch_addr=0x%x, PTAB_write_enable=%d, "
                    "update_queue_full=%d, pred_base_pc=%x\n",
                    out_reg.fetch_address, out_reg.icache_read_valid,
                    out_reg.predict_next_fetch_address,
                    out_reg.PTAB_write_enable, out_reg.update_queue_full,
                    out_reg.out_pred_base_pc);
    for (int i = 0; i < FETCH_WIDTH; i++) {
      if(!out_reg.PTAB_write_enable) 
        continue;
      DEBUG_LOG_SMALL_5(
          "  out[%d]: pred_dir=%d, alt_pred=%d, pcpn=%d, altpcpn=%d\n", i,
          out_reg.out_pred_dir[i], out_reg.out_alt_pred[i], out_reg.out_pcpn[i],
          out_reg.out_altpcpn[i]);
    }
    // --------------------------------------------------------------------
    // 2. 时序逻辑更新 (Sequential Logic Update)
    // --------------------------------------------------------------------

    // TODO：实际这里也是组合，决定latch的next值
    if (out_reg.icache_read_valid && inp.icache_read_ready) { // 握手成功, icache拿走当前pc_reg
      pc_can_send_to_icache = false; // this pc is already sent to icache
    }

    // latch logic
    if (state == S_IDLE && next_state == S_WORKING) {
      do_pred_latch = going_to_do_pred;
      for (int i = 0; i < BPU_BANK_NUM; i++)
        do_upd_latch[i] = going_to_do_upd[i];
      pred_base_pc_fired = pred_base_pc; // 记录当前预测流程中正在处理的pc基地址
      
      // 重置所有缓存标志和done信号
      for (int i = 0; i < FETCH_WIDTH; i++) {
        tage_result_valid_latch[i] = false;
        btb_result_valid_latch[i] = false;
      }
      for (int i = 0; i < BPU_BANK_NUM; i++) {
        tage_done[i] = false;
        btb_done[i] = false;
      }
    }
    
    // 在预测完成时，更新输出latch（使用合并后的预测结果）
    if ((state == S_WORKING) && 
        next_state == S_IDLE) {
      // for (int i = 0; i < FETCH_WIDTH; i++) {
      //   if (do_pred_on_this_pc[i]) {
      //     // 使用合并后的预测方向
      //     out_pred_dir_latch[i] = final_pred_dir[i];
      //     // TAGE的其他输出
      //     out_alt_pred_latch[i] = tage_calc_altpred_latch[i];
      //     out_pcpn_latch[i] = tage_calc_pcpn_latch[i];
      //     out_altpcpn_latch[i] = tage_calc_altpcpn_latch[i];
      //     for (int k = 0; k < TN_MAX; k++) {
      //       out_tage_tags_latch_latch[i][k] = tage_pred_calc_tags_latch[i][k];
      //       out_tage_idxs_latch_latch[i][k] = tage_pred_calc_idxs_latch[i][k];
      //     }
      //   }
      // }
      
      if (do_pred_latch) { // 确保产生了预测
        saved_mini_flush_req = saved_2ahead_prediction != next_fetch_addr_calc;
        saved_mini_flush_correct = saved_2ahead_prediction == next_fetch_addr_calc;
        DEBUG_LOG_SMALL_4("saved_mini_flush_correct: %d from saved_2ahead_prediction: %x and next_fetch_addr_calc: %x\n", saved_mini_flush_correct, saved_2ahead_prediction, next_fetch_addr_calc);
        saved_mini_flush_target = saved_2ahead_prediction; // now not used

        pc_reg = next_fetch_addr_calc; // 使用合并后的预测地址
        pc_can_send_to_icache = true; // 已经填装了一条新的pc_reg
        
        // ========================================================================
        // 2-Ahead时序更新：保存当前预测的C到saved_2ahead_prediction
        // ========================================================================
        saved_2ahead_prediction = final_2_ahead_address;
        saved_2ahead_pred_valid = true;

      }
      
      // 重置缓存标志和done信号
      for (int i = 0; i < FETCH_WIDTH; i++) {
        tage_result_valid_latch[i] = false;
        btb_result_valid_latch[i] = false;
      }
      for (int i = 0; i < BPU_BANK_NUM; i++) {
        tage_done[i] = false;
        btb_done[i] = false;
      }
    }
    
    if (inp.refetch) { // 这会覆盖掉此前的更新，非常合理
      pc_reg = inp.refetch_address; // update pc_reg for refetch
      pc_can_send_to_icache = true; // 已经填装了一条新的pc_reg
      saved_2ahead_prediction = inp.refetch_address + (FETCH_WIDTH * 4); // 初始化2-ahead预测器为下一个cache line的地址
      saved_2ahead_pred_valid = false; // 无效掉2-Ahead缓存
      saved_mini_flush_req = false;
      saved_mini_flush_correct = false;
    }

    state = next_state;

    // COMMIT LOGIC
    // ========================================================================
    // 4.2 提交阶段：更新Arch GHR/FH + 纠正Spec（mispredict恢复）
    // 使用 inp.*（按程序顺序），一组 COMMIT_WIDTH 只写一次 Arch 寄存器
    // ========================================================================
    {
      // 从当前 Arch 状态做一个快照，在上面滚动多条分支
      bool arch_ghr_tmp[GHR_LENGTH];
      uint32_t arch_fh_tmp[FH_N_MAX][TN_MAX];
      std::memcpy(arch_ghr_tmp, Arch_GHR, sizeof(Arch_GHR));
      std::memcpy(arch_fh_tmp,  Arch_FH,  sizeof(Arch_FH));

      bool arch_need_write = false;

      // 按 COMMIT_WIDTH 顺序（即程序顺序）遍历本周期提交的分支
      for (int i = 0; i < COMMIT_WIDTH; ++i) {
        if (!inp.in_upd_valid[i])
          continue;

        // 只对条件分支更新历史
        bool is_cond_upd = (inp.in_actual_br_type[i] == BR_DIRECT);
        if (!is_cond_upd)
          continue;

        bool real_dir = inp.in_actual_dir[i];

        bool next_ghr[GHR_LENGTH];
        uint32_t next_fh[FH_N_MAX][TN_MAX];

        // 在临时 Arch 状态上做单步更新
        TAGE_GHR_update_comb_1(arch_ghr_tmp, real_dir, next_ghr);
        TAGE_FH_update_comb_1(arch_fh_tmp, arch_ghr_tmp, real_dir,
                              next_fh, fh_length, ghr_length);

        // 用提交时带回的预测方向做 mispredict 检测
        bool mispred = (inp.in_pred_dir[i] != real_dir);
        if (mispred) {
          // 发生误预测，将 Spec 历史对齐到“包含 real_dir 的最新 Arch 状态”
          std::memcpy(Spec_GHR, next_ghr, sizeof(Spec_GHR));
          std::memcpy(Spec_FH,  next_fh,  sizeof(Spec_FH));
        }

        // 将本次结果累积到临时 Arch 状态，供后续分支继续更新
        std::memcpy(arch_ghr_tmp, next_ghr, sizeof(arch_ghr_tmp));
        std::memcpy(arch_fh_tmp,  next_fh,  sizeof(arch_fh_tmp));
        arch_need_write = true;
      }

      // 一拍内所有提交的 cond branch 处理完后，只写回一次 Arch GHR/FH
      if (arch_need_write) {
        std::memcpy(Arch_GHR, arch_ghr_tmp, sizeof(Arch_GHR));
        std::memcpy(Arch_FH,  arch_fh_tmp,  sizeof(Arch_FH));
      }
    }
    // Queue Update Logic
    for (int i = 0; i < COMMIT_WIDTH; i++) {
      int bank_sel = get_bank_sel(inp.in_update_base_pc[i]);
      if (q_push_en[bank_sel] && inp.in_upd_valid[i]) { // this can happen every cycle!
        QueueEntry &entry = update_queue[q_wr_ptr[bank_sel]][bank_sel];
        entry.base_pc = inp.in_update_base_pc[i];
        entry.valid_mask = inp.in_upd_valid[i];
        entry.actual_dir = inp.in_actual_dir[i];
        entry.pred_dir = inp.in_pred_dir[i];
        entry.alt_pred = inp.in_alt_pred[i];
        entry.br_type = inp.in_actual_br_type[i];
        entry.targets = inp.in_actual_targets[i];
        entry.pcpn = inp.in_pcpn[i];
        entry.altpcpn = inp.in_altpcpn[i];
        for (int k = 0; k < 4; k++) {
          entry.tage_tags[k] = inp.in_tage_tags[i][k];
          entry.tage_idxs[k] = inp.in_tage_idxs[i][k];
        }
        q_wr_ptr[bank_sel] = (q_wr_ptr[bank_sel] + 1) % Q_DEPTH;
      }
    }

    for (int i = 0; i < BPU_BANK_NUM; i++) {
      int bank_sel = i;
      if (q_pop_en[bank_sel]) {
        QueueEntry &upd_entry = q_data[bank_sel];
        
        // 访问LBE，获取前驱PC_A
        LastBlockEntry &lbe = last_block_table[bank_sel];
        
        if (lbe.valid) {
          // LBE有效，说明存在前驱PC_A
          uint32_t pc_a = lbe.last_pc;
          
          // 计算PC_A在简易预测表中的Index
          uint32_t index = get_bank_pc(pc_a) % TWO_AHEAD_TABLE_SIZE;
          
          // 更新简易预测表
          SimpleAheadEntry &entry = simple_ahead_table[bank_sel][index];
          entry.valid = true;
          
          // PC_B是upd_entry.base_pc，PC_C是upd_entry.targets
          // 判断B到C是否跳转：如果actual_dir为真，说明跳转
          entry.taken = upd_entry.actual_dir;
          entry.target = upd_entry.targets; // PC_C
        }
        
        // 更新LBE：将当前的PC_B写入last_block_table，作为下一个块的前驱
        lbe.valid = true;
        lbe.last_pc = upd_entry.base_pc; // PC_B
        
        q_rd_ptr[bank_sel] = (q_rd_ptr[bank_sel] + 1) % Q_DEPTH;
      }

      if (q_push_en[bank_sel] && !q_pop_en[bank_sel]){
        if(q_count[bank_sel] >= Q_DEPTH) {
          printf("ERROR!!: q_count[%d] >= Q_DEPTH, q_count[%d] = %d\n", bank_sel, bank_sel, q_count[bank_sel]);
          exit(1);
        }
        q_count[bank_sel]++;
      }
      else if (!q_push_en[bank_sel] && q_pop_en[bank_sel]){
        if(q_count[bank_sel] <= 0) {
          printf("ERROR!!: q_count[%d] <= 0, q_count[%d] = %d\n", bank_sel, bank_sel, q_count[bank_sel]);
          exit(1);
        }
        q_count[bank_sel]--;
      }
    }
      
    
    // bool q_full_any = (q_count[0] == Q_DEPTH) || (q_count[1] == Q_DEPTH) ||
                      // (q_count[2] == Q_DEPTH) || (q_count[3] == Q_DEPTH);
    bool q_full_any = false;
    for (int i = 0; i < BPU_BANK_NUM; i++) {
      q_full_any |= (q_count[i] == Q_DEPTH);
    }
    out_reg.update_queue_full = q_full_any;

    // Inst Type Memory Update
    for (int i = 0; i < COMMIT_WIDTH;
         i++) { // this can happen as early as possible
      if (inp.in_upd_valid[i]) {
        uint32_t addr = inp.in_update_base_pc[i];
        int bank_sel = get_bank_sel(addr);
        int type_idx = get_bank_pc(addr) & BPU_TYPE_IDX_MASK;
        inst_type_mem[type_idx][bank_sel] =
            inp.in_actual_br_type[i];
      }
    }

    // DEBUG LOG SMALL 输出pc_reg
    DEBUG_LOG_SMALL("[BPU_TOP] sim_time: %u, refetch: %d, refetch_addr: 0x%x, pc_reg: 0x%x\n", 
      bpu_sim_time, inp.refetch, inp.refetch_address, pc_reg);

    return out_reg;
  }
};

#endif // BPU_TOP_H
