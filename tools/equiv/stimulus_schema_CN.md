# Shared Stimulus Schema

当前 seed 使用 JSON。

顶层字段：

- `name`
- `warmup_cycles`
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

每条 event 还支持可选字段：

- `duration`
- `hold_until_accept`

表示该 event 连续作用多少个 cycle。默认是 `1`。
`hold_until_accept` 目前只对 `read_req` / `write_req` 有意义，表示请求一旦启动，就保持到第一次被 `*_ACCEPT` 为止。

## 说明

当前 MVP 约定：

- `warmup_cycles` 表示在 seed 正式事件开始前，C++ / RTL 都先空跑的 cycle 数
- 这个字段主要用于吸收 RTL 顶层 reset 后的 invalidate sweep / reconfig busy 窗口
- `read_req` / `write_req` 默认只持续一个 cycle
- 推荐对 upstream request 使用 `hold_until_accept: 1`，而不是简单用 `duration` 重复脉冲
- `duration` 更适合给 mode/ready/AXI response 这类纯电平刺激使用
- `axi_rdata_words` 当前按 256b beat 传 8 个 32-bit word
- `write_req.wdata_words` 传 16 个 32-bit word
- `write_req.wstrb_bytes` 传 64 个 byte enable
