#ifndef BTB_TOP_H
#define BTB_TOP_H

#include "../../frontend.h"
#include "../BPU_configs.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>

struct BtbSetData {
  uint32_t tag[BTB_WAY_NUM];
  uint32_t bta[BTB_WAY_NUM];
  bool valid[BTB_WAY_NUM];     // the valid bit in BTB_ENTRY
  uint8_t useful[BTB_WAY_NUM]; // 3-bit
};

struct HitCheckOut {
  int hit_way; // 0123
  bool hit;
};

class BTB_TOP {
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

  struct InputPayload {
    uint32_t pred_pc;
    bool pred_req;
    bool upd_valid;
    uint32_t upd_pc;
    uint32_t upd_actual_addr;
    uint8_t upd_br_type_in;
    bool upd_actual_dir;
  };

  struct OutputPayload {
    uint32_t pred_target;
    bool btb_pred_out_valid;
    bool btb_update_done;
    bool busy;
  };

  // 状态输入结构体（包含所有寄存器）
  struct StateInput {
    State state;
    // input latches
    bool do_pred_latch;
    bool do_upd_latch;
    uint32_t upd_pc_latch;
    uint32_t upd_actual_addr_latch;
    uint8_t upd_br_type_latch;
    bool upd_actual_dir_latch;
    // pipeline latches
    uint32_t pred_calc_pc_latch;
    uint32_t pred_calc_btb_tag_latch;
    uint32_t pred_calc_btb_idx_latch;
    uint32_t pred_calc_type_idx_latch;
    uint32_t pred_calc_bht_idx_latch;
    // uint32_t pred_calc_tc_idx_latch;
  };

  // Index生成结果
  struct IndexResult {
    uint32_t btb_idx;
    uint32_t type_idx;
    uint32_t bht_idx;
    uint32_t tc_idx;
    uint32_t tag;
    bool read_address_valid;
  };

  // 内存读取结果
  struct MemReadResult {
    BtbSetData r_btb_set;
    uint8_t r_type;
    uint32_t r_bht;
    uint32_t r_tc;
    bool read_data_valid;
  };

  // 组合逻辑计算结果结构体
  struct CombResult {
    State next_state;
    uint32_t btb_idx;
    uint32_t type_idx;
    uint32_t bht_idx;
    uint32_t tc_idx;
    uint32_t tag;
    BtbSetData r_btb_set;
    uint8_t r_type;
    uint32_t r_bht;
    uint32_t r_tc;
    HitCheckOut hit_info;
    uint32_t pred_target;

    // Update Path Calculation
    uint32_t next_bht_val;
    HitCheckOut upd_hit_info;
    int victim_way;
    int w_target_way;
    uint8_t next_useful_val;
    bool upd_writes_btb;

    // Stage 1 calculation results (for pipeline)
    uint32_t s1_pred_tag;
    uint32_t s1_pred_btb_idx;
    uint32_t s1_pred_type_idx;
    uint32_t s1_pred_bht_idx;
    uint32_t s1_pred_tc_idx;

    OutputPayload out_regs;
  };

private:
  // 内存存储
  uint32_t mem_btb_tag[BTB_WAY_NUM][BTB_ENTRY_NUM];
  uint32_t mem_btb_bta[BTB_WAY_NUM][BTB_ENTRY_NUM];
  bool mem_btb_valid[BTB_WAY_NUM][BTB_ENTRY_NUM];
  uint8_t mem_btb_useful[BTB_WAY_NUM][BTB_ENTRY_NUM];

  uint8_t mem_type[BTB_TYPE_ENTRY_NUM];
  uint32_t mem_bht[BHT_ENTRY_NUM];
  uint32_t mem_tc[TC_ENTRY_NUM];

  // Pipeline Registers
  State state;
  bool do_pred_latch;
  bool do_upd_latch;
  uint32_t upd_pc_latch;
  uint32_t upd_actual_addr_latch;
  uint8_t upd_br_type_latch;
  bool upd_actual_dir_latch;

  // Pipeline Regs (S1 to S2)
  uint32_t pred_calc_pc_latch;
  uint32_t pred_calc_btb_tag_latch;
  uint32_t pred_calc_btb_idx_latch;
  uint32_t pred_calc_type_idx_latch;
  uint32_t pred_calc_bht_idx_latch;
  // uint32_t pred_calc_tc_idx_latch;

  // For Update Writeback (S1 calc result):
  uint32_t upd_calc_next_bht_val_latch;
  HitCheckOut upd_calc_hit_info_latch;
  int upd_calc_victim_way_latch;
  int upd_calc_w_target_way_latch;
  uint8_t upd_calc_next_useful_val_latch;
  bool upd_calc_writes_btb_latch;

  // Outputs Registers
  OutputPayload out_regs;

  // SRAM延迟模拟相关变量（用于BTB和TC表项）
  bool sram_delay_active;           // 是否正在进行延迟
  int sram_delay_counter;            // 剩余延迟周期数
  MemReadResult sram_delayed_data;  // 延迟期间保存的数据（包含BTB和TC）
  bool sram_new_req_this_cycle;      // 本周期是否有新的读请求（在step_comb中设置，step_seq中使用）
  std::mt19937 rng;                  // 随机数生成器
  std::uniform_int_distribution<int> delay_dist; // 延迟分布（1-5周期）

public:
  BTB_TOP() : rng(std::random_device{}()), delay_dist(SRAM_DELAY_MIN, SRAM_DELAY_MAX) { reset(); }

  void reset() {
    std::memset(mem_btb_tag, 0, sizeof(mem_btb_tag));
    std::memset(mem_btb_bta, 0, sizeof(mem_btb_bta));
    std::memset(mem_btb_valid, 0, sizeof(mem_btb_valid));
    std::memset(mem_btb_useful, 0, sizeof(mem_btb_useful));
    std::memset(mem_type, 0, sizeof(mem_type));
    std::memset(mem_bht, 0, sizeof(mem_bht));
    std::memset(mem_tc, 0, sizeof(mem_tc));

    state = S_IDLE;
    do_pred_latch = false;
    do_upd_latch = false;
    upd_pc_latch = 0;
    upd_actual_addr_latch = 0;
    upd_br_type_latch = 0;
    upd_actual_dir_latch = false;

    pred_calc_pc_latch = 0;
    pred_calc_btb_tag_latch = 0;
    pred_calc_btb_idx_latch = 0;
    pred_calc_type_idx_latch = 0;
    pred_calc_bht_idx_latch = 0;
    // pred_calc_tc_idx_latch = 0;

    upd_calc_next_bht_val_latch = 0;
    memset(&upd_calc_hit_info_latch, 0, sizeof(HitCheckOut));
    upd_calc_victim_way_latch = 0;
    upd_calc_w_target_way_latch = 0;
    upd_calc_next_useful_val_latch = 0;
    upd_calc_writes_btb_latch = false;

    memset(&out_regs, 0, sizeof(OutputPayload));
    
    // Init SRAM delay simulation
    sram_delay_active = false;
    sram_delay_counter = 0;
    sram_new_req_this_cycle = false;
    memset(&sram_delayed_data, 0, sizeof(MemReadResult));
  }

  // ------------------------------------------------------------------------
  // 组合逻辑函数 - 第一次生成Index（不包含 tc_idx）
  // ------------------------------------------------------------------------
  IndexResult step_comb_gen_index_1(const InputPayload &inp,
                                    const StateInput &state_in) {
    IndexResult idx;
    memset(&idx, 0, sizeof(IndexResult));

    bool read_upd_in = (state_in.state == S_IDLE && inp.upd_valid);
    bool read_upd_wait = (state_in.state == S_IDLE_WAIT_DATA && state_in.do_upd_latch);
    bool read_pred = (state_in.state == S_STAGE2 && state_in.do_pred_latch) || (state_in.state == S_STAGE2_WAIT_DATA && state_in.do_pred_latch);

    // Table Address Mux Logic (类似 TAGE)
    if (read_pred) {
      // Stage 2: 使用 pipeline registers 中的地址
      idx.btb_idx = state_in.pred_calc_btb_idx_latch;
      idx.type_idx = state_in.pred_calc_type_idx_latch;
      idx.bht_idx = state_in.pred_calc_bht_idx_latch;
      // gen_index_1 not set tc index
      idx.tag = state_in.pred_calc_btb_tag_latch;
      idx.read_address_valid = true;

    } else if (read_upd_in || read_upd_wait) {
      // Update path: 使用 update PC
      uint32_t upd_pc = read_upd_in ? inp.upd_pc : state_in.upd_pc_latch;
      idx.btb_idx = btb_get_idx_comb(upd_pc);
      idx.type_idx = btb_get_type_idx_comb(upd_pc);
      idx.bht_idx = bht_get_idx_comb(upd_pc);
      idx.tag = btb_get_tag_comb(upd_pc);
      // gen_index_1 not set tc index
      idx.read_address_valid = true;
    } else {
      idx.read_address_valid = false;
    }

    return idx;
  }

  // ------------------------------------------------------------------------
  // 第一次内存读取 (TABLE READ) - 读取 BHT、Type
  // ------------------------------------------------------------------------
  MemReadResult step_comb_mem_read_1(const IndexResult &idx_1,
                                     const StateInput &state_in) {
    MemReadResult mem;
    memset(&mem, 0, sizeof(MemReadResult));

    // Memory Read (第一次，读取 BHT、Type)
    if (idx_1.read_address_valid) {
      // Type, BHT Read
      mem.r_type = mem_type[idx_1.type_idx];
      mem.r_bht = mem_bht[idx_1.bht_idx];
      // BTB 和 TC 不在这里读取，需要在第二次 mem_read 中读取
      mem.read_data_valid = true; // reg file read, always valid
    } else {
      mem.read_data_valid = false;
    }
    return mem;
  }

  // ------------------------------------------------------------------------
  // 组合逻辑函数 - 第二次生成Index（使用第一次读取的 BHT 值计算 tc_idx）
  // ------------------------------------------------------------------------
  IndexResult step_comb_gen_index_2(const InputPayload &inp,
                                    const StateInput &state_in,
                                    const IndexResult &idx_1,
                                    const MemReadResult &mem_1) {
    IndexResult idx_2;
    memset(&idx_2, 0, sizeof(IndexResult));

    // 复制第一次的索引结果
    idx_2.btb_idx = idx_1.btb_idx;
    idx_2.type_idx = idx_1.type_idx;
    idx_2.bht_idx = idx_1.bht_idx;
    idx_2.tag = idx_1.tag;

    // 计算 tc_idx（使用第一次读取的 BHT 值）
    if (state_in.state == S_STAGE2 && state_in.do_pred_latch) {
      // Stage 2: 使用本周期刚刚从 mem_1 读出的有效 BHT，和 latch 下来的 PC 进行计算
      // mem_1.r_bht 在 Stage 2 是有效的，因为 gen_index_1 在 Stage 2 开启了读取
      idx_2.tc_idx = tc_get_idx_comb(state_in.pred_calc_pc_latch, mem_1.r_bht);
      idx_2.read_address_valid = true;
    } else if (state_in.state == S_IDLE && inp.upd_valid) {
      // Update path: 使用第一次读取的 BHT 值计算 tc_idx
      idx_2.tc_idx = tc_get_idx_comb(inp.upd_pc, mem_1.r_bht);
      idx_2.read_address_valid = true;
    } else {
      idx_2.read_address_valid = false;
    }

    return idx_2;
  }

  // ------------------------------------------------------------------------
  // 第二次内存读取 (TABLE READ) - 读取 TC、BTB，带SRAM随机延迟模拟
  // ------------------------------------------------------------------------
  MemReadResult step_comb_mem_read_2(const IndexResult &idx_2,
                                     const StateInput &state_in,
                                     const MemReadResult &mem_1) {
    MemReadResult mem_2;
    memset(&mem_2, 0, sizeof(MemReadResult));

    mem_2.r_type = mem_1.r_type;
    mem_2.r_bht = mem_1.r_bht;

    // 如果正在进行延迟，返回保存的数据
#ifdef SRAM_DELAY_ENABLE
    if (sram_delay_active) {
      if(idx_2.read_address_valid) {
        printf("[BTB_TOP] SRAM delay active, read address valid\n");
        exit(1);
      }
      mem_2.r_btb_set = sram_delayed_data.r_btb_set;
      mem_2.r_tc = sram_delayed_data.r_tc; // from SRAM
      // 延迟计数器在时序逻辑中更新，这里只返回数据
      // read_data_valid将在延迟计数器为0时由时序逻辑设置
      if(sram_delay_counter == 0) {
        mem_2.read_data_valid = true;
        sram_delay_active = false;
      }
      else {
        mem_2.read_data_valid = false;
      }
      sram_new_req_this_cycle = false; // 延迟期间不接受新请求
      return mem_2;
    }
#endif
    // Memory Read (第二次，读取 TC、BTB) - 新的读请求
    if (idx_2.read_address_valid) {
      // BTB Set Read
      for (int w = 0; w < BTB_WAY_NUM; w++) {
        mem_2.r_btb_set.tag[w] = mem_btb_tag[w][idx_2.btb_idx];
        mem_2.r_btb_set.bta[w] = mem_btb_bta[w][idx_2.btb_idx];
        mem_2.r_btb_set.valid[w] = mem_btb_valid[w][idx_2.btb_idx];
        mem_2.r_btb_set.useful[w] = mem_btb_useful[w][idx_2.btb_idx];
      }

      // 读取 TC
      mem_2.r_tc = mem_tc[idx_2.tc_idx];
      // 保存数据，标记本周期有新请求
      sram_delayed_data = mem_2;
      sram_new_req_this_cycle = true;
      // 初始时数据无效，等待延迟完成
#ifdef SRAM_DELAY_ENABLE
      mem_2.read_data_valid = false;
#else
      mem_2.read_data_valid = true;
#endif
    } else {
      mem_2.read_data_valid = false;
      sram_new_req_this_cycle = false;
    }
    return mem_2;
  }

  // ------------------------------------------------------------------------
  // 组合逻辑函数 - 计算部分
  // ------------------------------------------------------------------------
  CombResult step_comb_calc(const InputPayload &inp, const StateInput &state_in,
                            const IndexResult &idx_2,
                            const MemReadResult &mem_2) {
    CombResult comb;
    memset(&comb, 0, sizeof(CombResult));

    // 复制index结果
    // comb.btb_idx = idx_2.btb_idx;
    // comb.type_idx = idx_2.type_idx;
    // comb.bht_idx = idx_2.bht_idx;
    // comb.tc_idx = idx_2.tc_idx;
    // comb.tag = idx_2.tag;

    DEBUG_LOG_SMALL("[BTB_TOP] state=%d\n", state_in.state);

    // 1.1 Next State Logic
    switch (state_in.state) {
      case S_IDLE:
        if (inp.pred_req || inp.upd_valid) {
          if (!inp.upd_valid)
            comb.next_state = S_STAGE2; // no update req, go straight to stage 2
          else if (mem_2.read_data_valid)
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
        else if (mem_2.read_data_valid)
          comb.next_state = S_IDLE; // data is ready, go straight to idle
        else
          comb.next_state =
              S_STAGE2_WAIT_DATA; // data is not ready, wait for data
        break;
      case S_IDLE_WAIT_DATA:
        if (mem_2.read_data_valid)
          comb.next_state = S_STAGE2;
        else
          comb.next_state = S_IDLE_WAIT_DATA;
        break;
      case S_STAGE2_WAIT_DATA:
        if (mem_2.read_data_valid)
          comb.next_state = S_IDLE;
        else
          comb.next_state = S_STAGE2_WAIT_DATA;
        break;
      default:
        comb.next_state = state_in.state;
        break;
    }

    // 1.2 Stage 1 Calculation (预测路径的 index 计算)
    if (state_in.state == S_IDLE && inp.pred_req) {
      comb.s1_pred_tag = btb_get_tag_comb(inp.pred_pc);
      comb.s1_pred_btb_idx = btb_get_idx_comb(inp.pred_pc);
      comb.s1_pred_type_idx = btb_get_type_idx_comb(inp.pred_pc);
      comb.s1_pred_bht_idx = bht_get_idx_comb(inp.pred_pc);
      // comb.s1_pred_tc_idx = tc_get_idx_comb(
          // inp.pred_pc, mem_2.r_bht); // this not depends on mem_valid
    }

    // 1.3 Stage 2 Calculation (预测输出逻辑)
    if ((state_in.state == S_STAGE2 || state_in.state == S_STAGE2_WAIT_DATA) && state_in.do_pred_latch && mem_2.read_data_valid) {
      comb.hit_info =
          btb_hit_check_comb(&mem_2.r_btb_set, state_in.pred_calc_btb_tag_latch);
      comb.pred_target =
          btb_pred_output_comb(state_in.pred_calc_pc_latch, mem_2.r_type,
                               comb.hit_info, &mem_2.r_btb_set, mem_2.r_tc);
    }

    // 1.4 Update Path Calculation
    if (comb.next_state == S_STAGE2) {
      bool upd_actual_dir;
      uint32_t upd_actual_addr;
      uint8_t upd_br_type_in;
      upd_actual_dir = state_in.state == S_IDLE ? inp.upd_actual_dir : state_in.upd_actual_dir_latch;
      upd_actual_addr = state_in.state == S_IDLE ? inp.upd_actual_addr : state_in.upd_actual_addr_latch;
      upd_br_type_in = state_in.state == S_IDLE ? inp.upd_br_type_in : state_in.upd_br_type_latch;

      comb.next_bht_val = bht_next_state_comb(mem_2.r_bht, upd_actual_dir);

      comb.upd_hit_info = btb_hit_check_comb(&mem_2.r_btb_set, idx_2.tag);
      comb.victim_way = btb_victim_select_comb(&mem_2.r_btb_set);
      comb.w_target_way =
          comb.upd_hit_info.hit ? comb.upd_hit_info.hit_way : comb.victim_way;

      uint32_t current_target_bta = mem_2.r_btb_set.bta[comb.w_target_way];
      uint8_t current_useful = mem_2.r_btb_set.useful[comb.w_target_way];
      bool correct_pred = (current_target_bta == upd_actual_addr); 

      uint8_t calc_useful_val =
          useful_next_state_comb(current_useful, correct_pred);
      comb.next_useful_val = comb.upd_hit_info.hit ? calc_useful_val : 1;

      comb.upd_writes_btb =
          (upd_br_type_in == BR_DIRECT || upd_br_type_in == BR_CALL ||
           upd_br_type_in == BR_RET || upd_br_type_in == BR_JAL);
    }

    // 1.5 Output Logic
    // comb.out_regs.busy = (state_in.state != S_IDLE);

    if (state_in.state != S_IDLE && comb.next_state == S_IDLE) { // moving to idle
      if (state_in.do_upd_latch) {
        comb.out_regs.btb_update_done = true;
      }
      if (state_in.do_pred_latch) {
        comb.out_regs.pred_target = comb.pred_target;
        comb.out_regs.btb_pred_out_valid = true;
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
    // input latches
    state_in.do_pred_latch = do_pred_latch;
    state_in.do_upd_latch = do_upd_latch;
    state_in.upd_pc_latch = upd_pc_latch;
    state_in.upd_actual_addr_latch = upd_actual_addr_latch;
    state_in.upd_br_type_latch = upd_br_type_latch;
    state_in.upd_actual_dir_latch = upd_actual_dir_latch;
    // pipeline latches
    state_in.pred_calc_pc_latch = pred_calc_pc_latch;
    state_in.pred_calc_btb_tag_latch = pred_calc_btb_tag_latch;
    state_in.pred_calc_btb_idx_latch = pred_calc_btb_idx_latch;
    state_in.pred_calc_type_idx_latch = pred_calc_type_idx_latch;
    state_in.pred_calc_bht_idx_latch = pred_calc_bht_idx_latch;
    // state_in.pred_calc_tc_idx_latch = pred_calc_tc_idx_latch;

    // 第一次：生成索引（不包含 tc_idx）
    IndexResult idx_1 = step_comb_gen_index_1(inp, state_in);
    // 第一次：读取 BHT、Type
    MemReadResult mem_1 = step_comb_mem_read_1(idx_1, state_in);
    // 第二次：生成索引（使用第一次读取的 BHT 值计算 tc_idx）
    IndexResult idx_2 = step_comb_gen_index_2(inp, state_in, idx_1, mem_1);
    // 第二次：读取 TC、BTB
    MemReadResult mem_2 = step_comb_mem_read_2(idx_2, state_in, mem_1);
    // 计算
    return step_comb_calc(inp, state_in, idx_2, mem_2);
  }

  // ------------------------------------------------------------------------
  // 时序逻辑函数 (Sequential Logic)
  // ------------------------------------------------------------------------
  void step_seq(bool rst_n, const InputPayload &inp, const CombResult &comb) {
#ifdef SRAM_DELAY_ENABLE
    // SRAM延迟模拟的时序逻辑
    if (sram_new_req_this_cycle && !sram_delay_active) {
      // 新的读请求：启动延迟
      sram_delay_active = true;
      sram_delay_counter = delay_dist(rng); // 生成1-5周期的随机延迟
    }
    if (sram_delay_active) {
      // 延迟进行中：递减计数器
      if (sram_delay_counter > 0) {
        sram_delay_counter--;
      } else {
        // 延迟完成：重置标志（数据已在当前周期返回）
        // sram_delay_active = false;
      }
    }
#endif
    // Latch Requests
    if (state == S_IDLE && comb.next_state != S_IDLE) { // moving out from IDLE
      do_pred_latch =
          inp.pred_req; // this will only be changed here, latched!
      do_upd_latch = inp.upd_valid;
      upd_pc_latch = inp.upd_pc;
      upd_actual_addr_latch = inp.upd_actual_addr;
      upd_br_type_latch = inp.upd_br_type_in;
      upd_actual_dir_latch = inp.upd_actual_dir;

      pred_calc_pc_latch = inp.pred_pc;
      pred_calc_btb_tag_latch = comb.s1_pred_tag;
      pred_calc_btb_idx_latch = comb.s1_pred_btb_idx;
      pred_calc_type_idx_latch = comb.s1_pred_type_idx;
      pred_calc_bht_idx_latch = comb.s1_pred_bht_idx;
      // pred_calc_tc_idx_latch = comb.s1_pred_tc_idx; // 已在 step_comb_calc 中计算
    }

    // Latch Update Calculation Results
    if (comb.next_state == S_STAGE2) {
      upd_calc_next_bht_val_latch = comb.next_bht_val;
      upd_calc_hit_info_latch = comb.upd_hit_info;
      upd_calc_victim_way_latch = comb.victim_way;
      upd_calc_w_target_way_latch = comb.w_target_way;
      upd_calc_next_useful_val_latch = comb.next_useful_val;
      upd_calc_writes_btb_latch = comb.upd_writes_btb;
    }

    // Global Registers Update & Memory Write
    if (state != S_IDLE && comb.next_state == S_IDLE && do_upd_latch) {
      

      // Conditional Updates
      if (upd_actual_dir_latch == true) {
        // 1. Update Type
        uint32_t upd_type_idx = btb_get_type_idx_comb(upd_pc_latch);
        mem_type[upd_type_idx] = upd_br_type_latch;

        // Update BHT (Always valid)
        if(upd_br_type_latch != BR_NONCTL) {
          uint32_t upd_bht_idx = bht_get_idx_comb(upd_pc_latch);
          mem_bht[upd_bht_idx] = upd_calc_next_bht_val_latch;
        }

        // 2. Update TC
        if (upd_br_type_latch == BR_IDIRECT) {
          uint32_t upd_tc_idx =
              tc_get_idx_comb(upd_pc_latch, upd_calc_next_bht_val_latch);
              // tc_get_idx_comb(upd_pc_latch, mem_bht[bht_get_idx_comb(upd_pc_latch)]);
          mem_tc[upd_tc_idx] = upd_actual_addr_latch;
        }
        // 3. Update BTB
        else if (upd_calc_writes_btb_latch) {
          uint32_t upd_btb_idx = btb_get_idx_comb(upd_pc_latch);
          uint32_t upd_tag = btb_get_tag_comb(upd_pc_latch);
          mem_btb_tag[upd_calc_w_target_way_latch][upd_btb_idx] = upd_tag;
          mem_btb_bta[upd_calc_w_target_way_latch][upd_btb_idx] = upd_actual_addr_latch;
          mem_btb_valid[upd_calc_w_target_way_latch][upd_btb_idx] = true;
          mem_btb_useful[upd_calc_w_target_way_latch][upd_btb_idx] =
              upd_calc_next_useful_val_latch;
        }
      }

      
    }

    // State Transition
    state = comb.next_state;
    return;
  }

  // ------------------------------------------------------------------------
  // 周期步进函数 (Cycle Step) - 保留作为兼容接口
  // ------------------------------------------------------------------------
  OutputPayload step(bool rst_n, const InputPayload &inp) {
    if (rst_n) {
      reset();
      DEBUG_LOG("[BTB_TOP] reset\n");
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

  // [Comb] Index Calculation
  static uint32_t btb_get_tag_comb(uint32_t pc) {
    return ((pc >> 2) >> BTB_IDX_LEN) & BTB_TAG_MASK;
  }
  static uint32_t btb_get_idx_comb(uint32_t pc) {
    return (pc >> 2) & BTB_IDX_MASK;
  }
  static uint32_t btb_get_type_idx_comb(uint32_t pc) {
    return (pc >> 2) & BTB_TYPE_IDX_MASK;
  }
  static uint32_t bht_get_idx_comb(uint32_t pc) {
    return (pc >> 2) & BHT_IDX_MASK;
  }
  static uint32_t tc_get_idx_comb(uint32_t pc, uint32_t bht_value) {
    return (bht_value ^ pc) & TC_ENTRY_MASK;
  }

  // [Comb] Logic
  static uint32_t bht_next_state_comb(uint32_t current_bht, bool pc_dir) {
    return (current_bht << 1) | (pc_dir ? 1 : 0);
  }

  static uint8_t useful_next_state_comb(uint8_t current_val, bool correct) {
    if (correct) {
      if (current_val < 7)
        return current_val + 1;
    } else {
      if (current_val > 0)
        return current_val - 1;
    }
    return current_val;
  }

  static HitCheckOut btb_hit_check_comb(const BtbSetData *set_data,
                                        uint32_t tag) {
    HitCheckOut out;
    memset(&out, 0, sizeof(HitCheckOut));

    out.hit = false;
    out.hit_way = 0;
    for (int way = 0; way < BTB_WAY_NUM; way++) {
      if (set_data->valid[way] && set_data->tag[way] == tag) {
        out.hit_way = way;
        out.hit = true;
        return out;
      }
    }
    return out;
  }

  static uint32_t btb_pred_output_comb(uint32_t pc, uint8_t br_type,
                                       const HitCheckOut &hit_info,
                                       const BtbSetData *set_data,
                                       uint32_t tc_target) {
    uint8_t type = br_type;
    // printf("type=%d\n", type);

    if (type == BR_IDIRECT) {
      // printf("TC=%x\n", tc_taget);
      return tc_target;
    } else if (type == BR_DIRECT || type == BR_CALL || type == BR_JAL ||
               type == BR_RET) {
      if (hit_info.hit) {
        return set_data->bta[hit_info.hit_way];
      }
      return pc + 4; // Miss
    }
    return pc + 4; // Other types
  }

  static int btb_victim_select_comb(const BtbSetData *set_data) {
    // 1. Empty Way
    for (int way = 0; way < BTB_WAY_NUM; way++) {
      if (!set_data->valid[way])
        return way;
    }
    // 2. Min Useful
    int min_useful = 255;
    int min_useful_way = 0;
    for (int way = 0; way < BTB_WAY_NUM; way++) {
      if (set_data->useful[way] < min_useful) {
        min_useful = set_data->useful[way];
        min_useful_way = way;
      }
    }
    return min_useful_way;
  }
};

#endif // BTB_TOP_H