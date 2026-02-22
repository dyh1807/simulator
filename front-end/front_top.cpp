#include "front_IO.h"
#include "front_module.h"
#include "predecode.h"
#include "predecode_checker.h"
#include "BPU/BPU.h"
#include "RISCV.h"
#include "types.h"
#include <cstdint>
#include <cstdio>

// ============================================================================
// 全局状态（寄存器锁存值）
// ============================================================================
static bool predecode_refetch = false;
static uint32_t predecode_refetch_address = 0;
static uint32_t front_sim_time = 0;

// FIFO 状态锁存
static bool fetch_addr_fifo_full_latch = false;
static bool fetch_addr_fifo_empty_latch = true;
static bool fifo_full_latch = false;
static bool fifo_empty_latch = true;
static bool ptab_full_latch = false;
static bool ptab_empty_latch = true;
static bool front2back_fifo_full_latch = false;
static bool front2back_fifo_empty_latch = true;
static SimContext *front_ctx = nullptr;

static BPU_TOP bpu_instance;

// 定义全局指针，供TAGE访问BPU的GHR/FH
BPU_TOP *g_bpu_top = &bpu_instance;
const bool* BPU_get_Arch_GHR() {
  assert(g_bpu_top && "g_bpu_top must be initialized before BPU_get_Arch_GHR");
  return g_bpu_top->Arch_GHR;
}

const bool* BPU_get_Spec_GHR() {
  assert(g_bpu_top && "g_bpu_top must be initialized before BPU_get_Spec_GHR");
  return g_bpu_top->Spec_GHR;
}

const uint32_t (*BPU_get_Arch_FH())[TN_MAX] {
  assert(g_bpu_top && "g_bpu_top must be initialized before BPU_get_Arch_FH");
  return g_bpu_top->Arch_FH;
}

const uint32_t (*BPU_get_Spec_FH())[TN_MAX] {
  assert(g_bpu_top && "g_bpu_top must be initialized before BPU_get_Spec_FH");
  return g_bpu_top->Spec_FH;
}

void front_top_set_context(SimContext *ctx) { front_ctx = ctx; }

// ============================================================================
// 辅助函数
// ============================================================================

// 准备 BPU 输入
static void prepare_bpu_input(struct front_top_in *in, 
                               struct BPU_in *bpu_in,
                               bool do_refetch,
                               uint32_t refetch_addr,
                               bool icache_ready) {
    bpu_in->reset = in->reset;
    bpu_in->refetch = do_refetch;
    bpu_in->refetch_address = refetch_addr;
    bpu_in->icache_read_ready = icache_ready;
    
    for (int i = 0; i < COMMIT_WIDTH; i++) {
        bpu_in->back2front_valid[i] = in->back2front_valid[i];
        bpu_in->predict_base_pc[i] = in->predict_base_pc[i];
        bpu_in->actual_dir[i] = in->actual_dir[i];
        bpu_in->actual_br_type[i] = in->actual_br_type[i];
        bpu_in->actual_target[i] = in->actual_target[i];
        bpu_in->predict_dir[i] = in->predict_dir[i];
        bpu_in->alt_pred[i] = in->alt_pred[i];
        bpu_in->altpcpn[i] = in->altpcpn[i];
        bpu_in->pcpn[i] = in->pcpn[i];
        for (int j = 0; j < 4; j++) {
            bpu_in->tage_idx[i][j] = in->tage_idx[i][j];
            bpu_in->tage_tag[i][j] = in->tage_tag[i][j];
        }
    }
}

// 运行 BPU 并转换输出
static void run_bpu(struct BPU_in *bpu_in, struct BPU_out *bpu_out) {
    BPU_TOP::InputPayload bpu_input;
    bpu_input.refetch = bpu_in->refetch;
    bpu_input.refetch_address = bpu_in->refetch_address;
    bpu_input.icache_read_ready = bpu_in->icache_read_ready;
    
    for (int i = 0; i < COMMIT_WIDTH; i++) {
        bpu_input.in_update_base_pc[i] = bpu_in->predict_base_pc[i];
        bpu_input.in_upd_valid[i] = bpu_in->back2front_valid[i];
        bpu_input.in_actual_dir[i] = bpu_in->actual_dir[i];
        bpu_input.in_actual_br_type[i] = bpu_in->actual_br_type[i];
        bpu_input.in_actual_targets[i] = bpu_in->actual_target[i];
        bpu_input.in_pred_dir[i] = bpu_in->predict_dir[i];
        bpu_input.in_alt_pred[i] = bpu_in->alt_pred[i];
        bpu_input.in_pcpn[i] = bpu_in->pcpn[i];
        bpu_input.in_altpcpn[i] = bpu_in->altpcpn[i];
        for (int j = 0; j < 4; j++) {
            bpu_input.in_tage_tags[i][j] = bpu_in->tage_tag[i][j];
            bpu_input.in_tage_idxs[i][j] = bpu_in->tage_idx[i][j];
        }
    }
    
    BPU_TOP::OutputPayload bpu_output = 
        bpu_instance.step(true, bpu_in->reset, bpu_input);
    
    bpu_out->icache_read_valid = bpu_output.icache_read_valid;
    bpu_out->fetch_address = bpu_output.fetch_address;
    bpu_out->PTAB_write_enable = bpu_output.PTAB_write_enable;
    bpu_out->predict_next_fetch_address = bpu_output.predict_next_fetch_address;
    
    // 2-Ahead Predictor outputs
    bpu_out->two_ahead_target = bpu_output.two_ahead_target;
    bpu_out->mini_flush_req = bpu_output.mini_flush_req;
    bpu_out->mini_flush_target = bpu_output.mini_flush_target;
    bpu_out->mini_flush_correct = bpu_output.mini_flush_correct;
    
    for (int i = 0; i < FETCH_WIDTH; i++) {
        bpu_out->predict_dir[i] = bpu_output.out_pred_dir[i];
        bpu_out->predict_base_pc[i] = bpu_output.out_pred_base_pc + (i * 4);
        bpu_out->alt_pred[i] = bpu_output.out_alt_pred[i];
        bpu_out->altpcpn[i] = bpu_output.out_altpcpn[i];
        bpu_out->pcpn[i] = bpu_output.out_pcpn[i];
        for (int j = 0; j < 4; j++) {
            bpu_out->tage_idx[i][j] = bpu_output.out_tage_idxs[i][j];
            bpu_out->tage_tag[i][j] = bpu_output.out_tage_tags[i][j];
        }
    }
}

// ============================================================================
// 主函数
// ============================================================================
void front_top(struct front_top_in *in, struct front_top_out *out) {
    DEBUG_LOG_SMALL("--------front_top sim_time: %d----------------\n", front_sim_time);
    front_sim_time++;
    
    // ========================================================================
    // 阶段 0: 初始化所有模块的输入输出结构
    // ========================================================================
    struct BPU_in bpu_in;
    struct BPU_out bpu_out;
    struct fetch_address_FIFO_in fetch_addr_fifo_in;
    struct fetch_address_FIFO_out fetch_addr_fifo_out;
    struct icache_in icache_in;
    struct icache_out icache_out;
    struct instruction_FIFO_in fifo_in;
    struct instruction_FIFO_out fifo_out;
    struct PTAB_in ptab_in;
    struct PTAB_out ptab_out;
    struct front2back_FIFO_in front2back_fifo_in;
    struct front2back_FIFO_out front2back_fifo_out;
    
    // 性能优化（临时）：按需字段赋值，先禁用全量 memset。
    // memset(&bpu_in, 0, sizeof(bpu_in));
    // memset(&bpu_out, 0, sizeof(bpu_out));
    // memset(&fetch_addr_fifo_in, 0, sizeof(fetch_addr_fifo_in));
    // memset(&fetch_addr_fifo_out, 0, sizeof(fetch_addr_fifo_out));
    // memset(&icache_in, 0, sizeof(icache_in));
    // memset(&icache_out, 0, sizeof(icache_out));
    // memset(&fifo_in, 0, sizeof(fifo_in));
    // memset(&fifo_out, 0, sizeof(fifo_out));
    // memset(&ptab_in, 0, sizeof(ptab_in));
    // memset(&ptab_out, 0, sizeof(ptab_out));
    // memset(&front2back_fifo_in, 0, sizeof(front2back_fifo_in));
    // memset(&front2back_fifo_out, 0, sizeof(front2back_fifo_out));
    
    // ========================================================================
    // 阶段 1: 计算全局 flush/refetch 信号
    // ========================================================================
    bool global_reset = in->reset;
    // predecode refetch is delayed for 1 cycle
    bool global_refetch = in->refetch || predecode_refetch;
    // only BPU use this address
    uint32_t refetch_address = in->refetch ? in->refetch_address : predecode_refetch_address;
    
    // ========================================================================
    // 阶段 2: 确定各 FIFO 的读使能（在实际读取前先决策）
    // ========================================================================
    
    // fetch_address_FIFO 读使能：icache 准备好接收 且 FIFO 非空
    // 需要先获取 icache 的 ready 状态
    icache_in.reset = global_reset;
    icache_in.refetch = global_refetch;
    icache_in.csr_status = in->csr_status;
    icache_in.run_comb_only = true;
    icache_in.icache_read_valid = false;
    icache_in.fetch_address = 0;
    icache_top(&icache_in, &icache_out);
    bool icache_ready = icache_out.icache_read_ready;
#ifdef USE_IDEAL_ICACHE
    icache_ready = true;
#endif
    DEBUG_LOG_SMALL_4("icache_ready: %d\n", icache_ready);
    bool fetch_addr_fifo_read_enable = icache_ready && !fetch_addr_fifo_empty_latch && !global_reset && !global_refetch;
    if (!global_reset && !global_refetch) {
        if (!icache_ready && fetch_addr_fifo_empty_latch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_fetch_addr_block_ready0_empty1_cycle_total++;
            }
        }
    }
    
    // instruction_FIFO 和 PTAB 读使能：predecode checker 可以工作
    // 条件：两个 FIFO 都非空 且 front2back_fifo 未满
    bool predecode_can_run_old = !fifo_empty_latch && !ptab_empty_latch && 
                              !front2back_fifo_full_latch && !global_reset && !global_refetch;
    if (!predecode_can_run_old) {
        if (fifo_empty_latch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_predecode_gate_block_fifo_empty_cycle_total++;
            }
        }
        if (ptab_empty_latch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_predecode_gate_block_ptab_empty_cycle_total++;
            }
        }
        if (global_reset || global_refetch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_predecode_gate_block_reset_refetch_cycle_total++;
            }
        }
    }
    bool inst_fifo_read_enable = predecode_can_run_old;
    // bool ptab_read_enable = predecode_can_run;
    bool ptab_read_enable = predecode_can_run_old;
    
    // front2back_FIFO 读使能：后端请求读取
    // refetch and reset deal when running
    bool front2back_read_enable = in->FIFO_read_enable;
    
    // ========================================================================
    // 阶段 3: 执行所有 FIFO 的读操作（获取输出数据）
    // read last cycle's data
    // ========================================================================
    
    // 3.1 读取 fetch_address_FIFO
    fetch_addr_fifo_in.reset = global_reset;
    fetch_addr_fifo_in.refetch = global_refetch;
    fetch_addr_fifo_in.read_enable = fetch_addr_fifo_read_enable;
    fetch_addr_fifo_in.write_enable = false;  // 写操作稍后处理
    fetch_addr_fifo_in.fetch_address = 0;
    fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    
    // 3.2 读取 instruction_FIFO
    fifo_in.reset = global_reset;
    fifo_in.refetch = global_refetch;
    fifo_in.read_enable = inst_fifo_read_enable;
    fifo_in.write_enable = false;  // 写操作稍后处理
    instruction_FIFO_top(&fifo_in, &fifo_out);
    
    // 3.3 读取 PTAB
    ptab_in.reset = global_reset;
    ptab_in.refetch = global_refetch;
    ptab_in.read_enable = ptab_read_enable;
    ptab_in.write_enable = false;  // 写操作稍后处理
    PTAB_top(&ptab_in, &ptab_out);
    
    // 3.4 读取 front2back_FIFO
    front2back_fifo_in.reset = global_reset;
    // f2b不用关心预解码flush
    front2back_fifo_in.refetch = in->refetch;
    front2back_fifo_in.read_enable = front2back_read_enable;
    front2back_fifo_in.write_enable = false;  // 写操作稍后处理
    front2back_FIFO_top(&front2back_fifo_in, &front2back_fifo_out);
    
    // 保存读出的数据用于后续处理
    struct fetch_address_FIFO_out saved_fetch_addr_fifo_out = fetch_addr_fifo_out;
    struct instruction_FIFO_out saved_fifo_out = fifo_out;
    struct PTAB_out saved_ptab_out = ptab_out;
    struct front2back_FIFO_out saved_front2back_fifo_out = front2back_fifo_out;
    // 只有两路FIFO都实际读到有效项，且非dummy时才运行预解码
    bool predecode_can_run = saved_fifo_out.FIFO_valid &&
                             saved_ptab_out.read_valid &&
                             !saved_ptab_out.dummy_entry;
    
    // ========================================================================
    // 阶段 4: BPU 控制逻辑
    // ========================================================================
    // BPU 阻塞条件：fetch_address_FIFO 满 或 PTAB 满
    bool bpu_stall = fetch_addr_fifo_full_latch || ptab_full_latch;
    bool bpu_can_run = !bpu_stall || global_reset || global_refetch;
    if (!global_reset && !global_refetch && bpu_can_run) {
        if (front_ctx != nullptr) {
            front_ctx->perf.front_bpu_can_run_cycle_total++;
        }
    }
    
    // BPU 看到的 icache_ready 是 fetch_address_FIFO 是否有空位
    bool bpu_icache_ready = !fetch_addr_fifo_full_latch;
    
    prepare_bpu_input(in, &bpu_in, global_refetch, refetch_address, bpu_icache_ready);
    if (!bpu_can_run) {
        bpu_in.icache_read_ready = false;
    }
    run_bpu(&bpu_in, &bpu_out);
    
    if (bpu_out.icache_read_valid && bpu_can_run) {
        if (!global_reset && !global_refetch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_bpu_issue_cycle_total++;
            }
        }
        DEBUG_LOG_SMALL("[front_top] sim_time: %d, bpu_out.fetch_address: %x\n",
                        front_sim_time, bpu_out.fetch_address);
    } else if (!global_reset && !global_refetch && bpu_can_run) {
        if (front_ctx != nullptr) {
            front_ctx->perf.front_bpu_no_issue_when_can_run_cycle_total++;
        }
    }
    
    // ========================================================================
    // 阶段 5: fetch_address_FIFO 写控制（支持双写和Mini Flush）
    // ========================================================================
    fetch_addr_fifo_in.reset = false;
    fetch_addr_fifo_in.refetch = false;
    fetch_addr_fifo_in.read_enable = false;  // 读已经做过了
    // 相对于当周期写入新值，但最早下周期才会消费
    bool normal_write_enable = bpu_out.icache_read_valid && bpu_can_run && !global_reset;
    // 1. 刚好在BPUfire的当拍来了一个refetch，需要写，不然会掉指令
    // 2. cause BPU takes at least 1 cycle to finish refetch, no 2-write problem
    // fetch_addr_fifo_in.write_enable = normal_write_enable || refetch_write_enable;
    // fetch_addr_fifo_in.fetch_address = normal_write_enable ? bpu_out.fetch_address : refetch_address;
    
    // if (fetch_addr_fifo_in.write_enable) {
    //     // 读的时候也会处理一波reset和refetch
    //     fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    // }
    // if (refetch_write_enable) {
    //     fetch_addr_fifo_in.write_enable = true;
    //     fetch_addr_fifo_in.fetch_address = refetch_address;
    //     DEBUG_LOG_SMALL_4("refetch write enable: %x\n", refetch_address);
    //     fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    // } else 
    DEBUG_LOG_SMALL_4("normal_write_enable: %d, bpu_out.mini_flush_correct: %d\n", normal_write_enable, bpu_out.mini_flush_correct);
    // 首先看能不能节省一个写
    // 未开启2-Ahead模式下correct永远为0，保证正常写入
    if (normal_write_enable && !bpu_out.mini_flush_correct) {
        fetch_addr_fifo_in.write_enable = true;
        fetch_addr_fifo_in.fetch_address = bpu_out.fetch_address;
        DEBUG_LOG_SMALL_4("normal write enable: %x\n", bpu_out.fetch_address);
        fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    }
#ifdef ENABLE_2AHEAD
    // refetch的时候也要写2ahead target
    if (normal_write_enable) {
        fetch_addr_fifo_in.write_enable = true;
        fetch_addr_fifo_in.fetch_address = bpu_out.two_ahead_target;
        DEBUG_LOG_SMALL_4("2ahead write enable: %x\n", bpu_out.two_ahead_target);
        fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    }
#endif

    // ========================================================================
    // 阶段 6: icache 控制逻辑
    // ========================================================================
    icache_in.reset = global_reset;
    icache_in.refetch = global_refetch;
    icache_in.csr_status = in->csr_status;
    icache_in.run_comb_only = false;
    
    // icache 从 fetch_address_FIFO 获取地址
    // 注意用的是旧值
    if (saved_fetch_addr_fifo_out.read_valid) {
        icache_in.icache_read_valid = true;
        icache_in.fetch_address = saved_fetch_addr_fifo_out.fetch_address;
        if (!global_reset && !global_refetch) {
            if (front_ctx != nullptr) {
                front_ctx->perf.front_icache_req_cycle_total++;
            }
        }
        DEBUG_LOG_SMALL_4("icache_in.fetch_address: %x\n", icache_in.fetch_address);
    } else {
        icache_in.icache_read_valid = false;
        icache_in.fetch_address = 0;
    }
    
    icache_top(&icache_in, &icache_out);
    
    // ========================================================================
    // 阶段 7: instruction_FIFO 写控制（写入 icache 返回的数据）
    // ========================================================================
    bool icache_data_valid = icache_out.icache_read_complete;
    if (!global_reset && !global_refetch && icache_data_valid) {
        if (front_ctx != nullptr) {
            front_ctx->perf.front_icache_complete_cycle_total++;
        }
    }
    
    // 检查是否可以写入（不能满）
    bool inst_fifo_can_write = !fifo_full_latch && icache_data_valid && !global_reset && !global_refetch;
    
    fifo_in.reset = global_reset;
    fifo_in.refetch = global_refetch;
    fifo_in.read_enable = false;  // 读已经做过了
    fifo_in.write_enable = inst_fifo_can_write;
    
    if (inst_fifo_can_write) {
        for (int i = 0; i < FETCH_WIDTH; i++) {
            fifo_in.fetch_group[i] = icache_out.fetch_group[i];
            fifo_in.pc[i] = icache_out.fetch_pc + (i * 4);
            fifo_in.page_fault_inst[i] = icache_out.page_fault_inst[i];
            fifo_in.inst_valid[i] = icache_out.inst_valid[i];
            
            // 预译码
            if (icache_out.inst_valid[i]) {
                uint32_t current_pc = icache_out.fetch_pc + (i * 4);
                PredecodeResult result = predecode_instruction(icache_out.fetch_group[i], current_pc);
                fifo_in.predecode_type[i] = result.type;
                fifo_in.predecode_target_address[i] = result.target_address;
                
                DEBUG_LOG_SMALL_4("[icache_out] sim_time: %d, pc: %x, inst: %x\n",
                                  front_sim_time, current_pc, icache_out.fetch_group[i]);
            } else {
                fifo_in.predecode_type[i] = PREDECODE_NON_BRANCH;
                fifo_in.predecode_target_address[i] = 0;
            }
        }
        
        uint32_t mask = ~(ICACHE_LINE_SIZE - 1);
        fifo_in.seq_next_pc = icache_out.fetch_pc + (FETCH_WIDTH * 4);
        if ((fifo_in.seq_next_pc & mask) != (icache_out.fetch_pc & mask)) {
            fifo_in.seq_next_pc &= mask;
        }
        
        instruction_FIFO_top(&fifo_in, &fifo_out);
    }
    
    // ========================================================================
    // 阶段 8: PTAB 写控制
    // ========================================================================
    bool ptab_can_write = bpu_out.PTAB_write_enable && !ptab_full_latch && !global_reset && !global_refetch;
    
    ptab_in.reset = global_reset;
    ptab_in.refetch = global_refetch;
    ptab_in.read_enable = false;  // 读已经做过了
    ptab_in.write_enable = ptab_can_write;
    
    if (ptab_can_write) {
        for (int i = 0; i < FETCH_WIDTH; i++) {
            ptab_in.predict_dir[i] = bpu_out.predict_dir[i];
            ptab_in.predict_base_pc[i] = bpu_out.predict_base_pc[i];
            ptab_in.alt_pred[i] = bpu_out.alt_pred[i];
            ptab_in.altpcpn[i] = bpu_out.altpcpn[i];
            ptab_in.pcpn[i] = bpu_out.pcpn[i];
            for (int j = 0; j < 4; j++) {
                ptab_in.tage_idx[i][j] = bpu_out.tage_idx[i][j];
                ptab_in.tage_tag[i][j] = bpu_out.tage_tag[i][j];
            }
        }
        ptab_in.predict_next_fetch_address = bpu_out.predict_next_fetch_address;
        // 未开启2-Ahead模式下mini_flush_req永远为0，保证正常写入
        ptab_in.need_mini_flush = bpu_out.mini_flush_req;
        
        DEBUG_LOG_SMALL_3("bpu_out.predict_next_fetch_address: %x\n",
                          bpu_out.predict_next_fetch_address);
        
        PTAB_top(&ptab_in, &ptab_out);
    }
    
    // ========================================================================
    // 阶段 9: Predecode Checker 逻辑
    // ========================================================================
    bool do_predecode_flush = false;
    uint32_t predecode_flush_address = 0;
    
    struct predecode_checker_out checker_out;
    memset(&checker_out, 0, sizeof(checker_out));
    
    if (predecode_can_run) {
        // 验证 PC 匹配
        for (int i = 0; i < FETCH_WIDTH; i++) {
            if (saved_fifo_out.pc[i] != saved_ptab_out.predict_base_pc[i]) {
                printf("ERROR: fifo pc[%d]: %x != ptab pc[%d]: %x\n",
                       i, saved_fifo_out.pc[i], i, saved_ptab_out.predict_base_pc[i]);
                exit(1);
            }
        }
        
        // 运行 predecode checker
        struct predecode_checker_in checker_in;
        memset(&checker_in, 0, sizeof(checker_in));
        for (int i = 0; i < FETCH_WIDTH; i++) {
            checker_in.predict_dir[i] = saved_ptab_out.predict_dir[i];
            checker_in.predecode_type[i] = saved_fifo_out.predecode_type[i];
            checker_in.predecode_target_address[i] = saved_fifo_out.predecode_target_address[i];
        }
        checker_in.seq_next_pc = saved_fifo_out.seq_next_pc;
        checker_in.predict_next_fetch_address = saved_ptab_out.predict_next_fetch_address;
        
        predecode_checker_top(&checker_in, &checker_out);
        
        DEBUG_LOG_SMALL_4("[predecode on] seq_next_pc: %x, predict_next: %x\n",
                          saved_fifo_out.seq_next_pc, saved_ptab_out.predict_next_fetch_address);
        
        if (checker_out.predecode_flush_enable) {
            do_predecode_flush = true;
            predecode_flush_address = checker_out.predict_next_fetch_address_corrected;
        }
    }
    
    // ========================================================================
    // 阶段 10: front2back_FIFO 写控制
    // ========================================================================
    bool front2back_can_write = predecode_can_run  
                                && !front2back_fifo_full_latch 
                                && !global_reset; 
    
    front2back_fifo_in.reset = global_reset;
    front2back_fifo_in.refetch = in->refetch;
    front2back_fifo_in.read_enable = false;  // 读已经做过了
    front2back_fifo_in.write_enable = front2back_can_write;
    
    if (front2back_can_write) {
        for (int i = 0; i < FETCH_WIDTH; i++) {
            front2back_fifo_in.fetch_group[i] = saved_fifo_out.instructions[i];
            front2back_fifo_in.page_fault_inst[i] = saved_fifo_out.page_fault_inst[i];
            front2back_fifo_in.inst_valid[i] = saved_fifo_out.inst_valid[i];
            front2back_fifo_in.predict_dir_corrected[i] = checker_out.predict_dir_corrected[i];
            front2back_fifo_in.predict_base_pc[i] = saved_ptab_out.predict_base_pc[i];
            front2back_fifo_in.alt_pred[i] = saved_ptab_out.alt_pred[i];
            front2back_fifo_in.altpcpn[i] = saved_ptab_out.altpcpn[i];
            front2back_fifo_in.pcpn[i] = saved_ptab_out.pcpn[i];
            for (int j = 0; j < 4; j++) {
                front2back_fifo_in.tage_idx[i][j] = saved_ptab_out.tage_idx[i][j];
                front2back_fifo_in.tage_tag[i][j] = saved_ptab_out.tage_tag[i][j];
            }
        }
        front2back_fifo_in.predict_next_fetch_address_corrected = 
            checker_out.predict_next_fetch_address_corrected;
        
        front2back_FIFO_top(&front2back_fifo_in, &front2back_fifo_out);
    }
    
    // ========================================================================
    // 阶段 11: 处理 predecode flush
    // ========================================================================
    if (do_predecode_flush) {
        // // flush 所有中间 FIFO
        // fetch_addr_fifo_in.reset = true;
        // fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
        
        // fifo_in.reset = true;
        // instruction_FIFO_top(&fifo_in, &fifo_out);
        
        // ptab_in.reset = true;
        // PTAB_top(&ptab_in, &ptab_out);
        
        icache_in.reset = true;
        icache_top(&icache_in, &icache_out);
        
        predecode_refetch = true;
        predecode_refetch_address = predecode_flush_address;
        
        DEBUG_LOG_SMALL("[front_top] predecode flush to: %x\n", predecode_flush_address);
    } else {
        predecode_refetch = false;
        predecode_refetch_address = 0;
    }
    
    // ========================================================================
    // 阶段 12: 获取最终 FIFO 状态并更新锁存值
    // ========================================================================
    // 重新获取各 FIFO 的状态（不进行读写，只获取状态）
    fetch_addr_fifo_in.reset = false;
    fetch_addr_fifo_in.refetch = false;
    fetch_addr_fifo_in.read_enable = false;
    fetch_addr_fifo_in.write_enable = false;
    fetch_address_FIFO_top(&fetch_addr_fifo_in, &fetch_addr_fifo_out);
    
    fifo_in.reset = false;
    fifo_in.refetch = false;
    fifo_in.read_enable = false;
    fifo_in.write_enable = false;
    instruction_FIFO_top(&fifo_in, &fifo_out);
    
    ptab_in.reset = false;
    ptab_in.refetch = false;
    ptab_in.read_enable = false;
    ptab_in.write_enable = false;
    PTAB_top(&ptab_in, &ptab_out);
    
    front2back_fifo_in.reset = false;
    front2back_fifo_in.refetch = false;
    front2back_fifo_in.read_enable = false;
    front2back_fifo_in.write_enable = false;
    front2back_FIFO_top(&front2back_fifo_in, &front2back_fifo_out);
    
    // 更新锁存值
    fetch_addr_fifo_full_latch = fetch_addr_fifo_out.full;
    fetch_addr_fifo_empty_latch = fetch_addr_fifo_out.empty;
    fifo_full_latch = fifo_out.full;
    fifo_empty_latch = fifo_out.empty;
    ptab_full_latch = ptab_out.full;
    ptab_empty_latch = ptab_out.empty;
    front2back_fifo_full_latch = front2back_fifo_out.full;
    front2back_fifo_empty_latch = front2back_fifo_out.empty;
    
    // ========================================================================
    // 阶段 13: 设置输出
    // ========================================================================
    // 注意：输出来自 front2back_FIFO 的读结果（阶段3.4的结果）
    // front2back_fifo_in.read_enable = front2back_read_enable;
    // front2back_FIFO_top(&front2back_fifo_in, &front2back_fifo_out);
    
    out->FIFO_valid = saved_front2back_fifo_out.front2back_FIFO_valid;
    for (int i = 0; i < FETCH_WIDTH; i++) {
        out->instructions[i] = saved_front2back_fifo_out.fetch_group[i];
        out->page_fault_inst[i] = saved_front2back_fifo_out.page_fault_inst[i];
        out->predict_dir[i] = saved_front2back_fifo_out.predict_dir_corrected[i];
        out->pc[i] = saved_front2back_fifo_out.predict_base_pc[i];
        out->alt_pred[i] = saved_front2back_fifo_out.alt_pred[i];
        out->altpcpn[i] = saved_front2back_fifo_out.altpcpn[i];
        out->pcpn[i] = saved_front2back_fifo_out.pcpn[i];
        for (int j = 0; j < 4; j++) {
            out->tage_idx[i][j] = saved_front2back_fifo_out.tage_idx[i][j];
            out->tage_tag[i][j] = saved_front2back_fifo_out.tage_tag[i][j];
        }
        out->inst_valid[i] = saved_front2back_fifo_out.inst_valid[i];
        
        if (out->inst_valid[i]) {
            DEBUG_LOG_SMALL_4("[front_top] sim_time: %d, out->pc[%d]: %x, inst: %x\n",
                              front_sim_time, i, out->pc[i], out->instructions[i]);
        }
    }
    out->predict_next_fetch_address = saved_front2back_fifo_out.predict_next_fetch_address_corrected;
    
    if (out->FIFO_valid) {
        DEBUG_LOG_SMALL("[front_top] sim_time: %d, out->pc[0]: %x\n",
                        front_sim_time, out->pc[0]);
    }

}
