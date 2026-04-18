# Shared Stimulus Schema

当前 seed 使用 JSON。

顶层字段：

- `name`
- `tail_cycles`
- `defaults`
- `events`

`defaults` 里允许：

- `mode_req`
- `llc_mapped_offset_req`
- `invalidate_all_valid`
- `invalidate_line_valid`
- `invalidate_line_addr`
- `read_resp_ready_mask`
- `write_resp_ready_mask`
- `axi_arready`
- `axi_awready`
- `axi_wready`
- `axi_bvalid`
- `axi_bid`
- `axi_bresp`
- `axi_rvalid`
- `axi_rid`
- `axi_rresp`
- `axi_rlast`
- `axi_rdata_words`

`events` 里当前支持：

- `set_mode`
- `set_offset`
- `invalidate_all`
- `invalidate_line`
- `read_req`
- `write_req`
- `read_resp_ready_mask`
- `write_resp_ready_mask`
- `axi_arready`
- `axi_awready`
- `axi_wready`
- `axi_b`
- `axi_r`

## 说明

当前 MVP 约定：

- `read_req` / `write_req` 默认只持续一个 cycle
- 如需持续多个 cycle，就在多个 cycle 放事件
- `axi_rdata_words` 当前按 256b beat 传 8 个 32-bit word
- `write_req.wdata_words` 传 16 个 32-bit word
- `write_req.wstrb_bytes` 传 64 个 byte enable

