#ifndef FRONT_MODULE_H
#define FRONT_MODULE_H

#include "front_IO.h"
#include <cstdint>

class PtwMemPort;
class PtwWalkPort;
class SimContext;
extern PtwMemPort *icache_ptw_mem_port;
extern PtwWalkPort *icache_ptw_walk_port;

void BPU_top(struct BPU_in *in, struct BPU_out *out);
void BPU_change_pc_reg(uint32_t new_pc);

void icache_top(struct icache_in *in, struct icache_out *out);
void icache_set_context(SimContext *ctx);
void icache_set_ptw_mem_port(PtwMemPort *port);
void icache_set_ptw_walk_port(PtwWalkPort *port);

void instruction_FIFO_top(struct instruction_FIFO_in *in,
                          struct instruction_FIFO_out *out);

void PTAB_top(struct PTAB_in *in, struct PTAB_out *out);

void front_top(struct front_top_in *in, struct front_top_out *out);
void front_top_set_context(SimContext *ctx);

void front2back_FIFO_top(struct front2back_FIFO_in *in, struct front2back_FIFO_out *out);
void front2back_fifo_set_context(SimContext *ctx);


void fetch_address_FIFO_top(struct fetch_address_FIFO_in *in,
                            struct fetch_address_FIFO_out *out);


bool ptab_peek_mini_flush();


#endif // FRONT_MODULE_H
