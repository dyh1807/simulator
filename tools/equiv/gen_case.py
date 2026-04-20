#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

NUM_READ_MASTERS = 4
NUM_WRITE_MASTERS = 2
READ_BEAT_WORDS = 8
LINE_WORDS = 16
WRITE_WORDS = 16
WRITE_STRB_BYTES = 64


def parse_int(v):
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        return int(v, 0)
    raise TypeError(f"bad int value: {v!r}")


def parse_final_mem_samples(seed):
    out = []
    for item in seed.get("final_mem_samples", []):
        if isinstance(item, dict):
            out.append(parse_int(item["addr"]))
        else:
            out.append(parse_int(item))
    return out


def parse_final_mmio_samples(seed):
    out = []
    for item in seed.get("final_mmio_samples", []):
        if isinstance(item, dict):
            out.append(parse_int(item["addr"]))
        else:
            out.append(parse_int(item))
    return out


def parse_final_mapped_samples(seed):
    out = []
    for item in seed.get("final_mapped_samples", []):
        if isinstance(item, dict):
            out.append(parse_int(item["addr"]))
        else:
            out.append(parse_int(item))
    return out


def zero_frame(defaults):
    return {
        "mode_req": parse_int(defaults.get("mode_req", 1)),
        "llc_mapped_offset_req": parse_int(defaults.get("llc_mapped_offset_req", 0)),
        "invalidate_all_valid": parse_int(defaults.get("invalidate_all_valid", 0)),
        "invalidate_line_valid": parse_int(defaults.get("invalidate_line_valid", 0)),
        "invalidate_line_addr": parse_int(defaults.get("invalidate_line_addr", 0)),
        "read_resp_ready_mask": parse_int(defaults.get("read_resp_ready_mask", (1 << NUM_READ_MASTERS) - 1)),
        "write_resp_ready_mask": parse_int(defaults.get("write_resp_ready_mask", (1 << NUM_WRITE_MASTERS) - 1)),
        "axi_arready": parse_int(defaults.get("axi_arready", 1)),
        "axi_awready": parse_int(defaults.get("axi_awready", 1)),
        "axi_wready": parse_int(defaults.get("axi_wready", 1)),
        "axi_bvalid": parse_int(defaults.get("axi_bvalid", 0)),
        "axi_bid": parse_int(defaults.get("axi_bid", 0)),
        "axi_bresp": parse_int(defaults.get("axi_bresp", 0)),
        "axi_rvalid": parse_int(defaults.get("axi_rvalid", 0)),
        "axi_rid": parse_int(defaults.get("axi_rid", 0)),
        "axi_rresp": parse_int(defaults.get("axi_rresp", 0)),
        "axi_rlast": parse_int(defaults.get("axi_rlast", 0)),
        "axi_rdata_words": [parse_int(x) for x in defaults.get("axi_rdata_words", [0] * READ_BEAT_WORDS)] + [0] * READ_BEAT_WORDS,
        "mem_read_line_resp_valid": parse_int(defaults.get("mem_read_line_resp_valid", 0)),
        "mem_read_line_resp_words": [parse_int(x) for x in defaults.get("mem_read_line_resp_words", [0] * LINE_WORDS)] + [0] * LINE_WORDS,
        "read_req": [
            {"valid": 0, "addr": 0, "size": 0, "id": 0, "bypass": 0, "hold_until_accept": 0}
            for _ in range(NUM_READ_MASTERS)
        ],
        "write_req": [
            {
                "valid": 0,
                "addr": 0,
                "size": 0,
                "id": 0,
                "bypass": 0,
                "hold_until_accept": 0,
                "wdata_words": [0] * WRITE_WORDS,
                "wstrb_bytes": [0] * WRITE_STRB_BYTES,
            }
            for _ in range(NUM_WRITE_MASTERS)
        ],
    }


def apply_event(frame, event):
    t = event["type"]
    if t == "set_mode":
        frame["mode_req"] = parse_int(event["mode"])
    elif t == "set_offset":
        frame["llc_mapped_offset_req"] = parse_int(event["offset"])
    elif t == "invalidate_all":
        frame["invalidate_all_valid"] = parse_int(event.get("valid", 1))
    elif t == "invalidate_line":
        frame["invalidate_line_valid"] = parse_int(event.get("valid", 1))
        frame["invalidate_line_addr"] = parse_int(event["addr"])
    elif t == "read_req":
        m = parse_int(event["master"])
        rq = frame["read_req"][m]
        rq["valid"] = parse_int(event.get("valid", 1))
        rq["addr"] = parse_int(event["addr"])
        rq["size"] = parse_int(event["size"])
        rq["id"] = parse_int(event["id"])
        rq["bypass"] = parse_int(event.get("bypass", 0))
        rq["hold_until_accept"] = parse_int(event.get("hold_until_accept", 0))
    elif t == "write_req":
        m = parse_int(event["master"])
        rq = frame["write_req"][m]
        rq["valid"] = parse_int(event.get("valid", 1))
        rq["addr"] = parse_int(event["addr"])
        rq["size"] = parse_int(event["size"])
        rq["id"] = parse_int(event["id"])
        rq["bypass"] = parse_int(event.get("bypass", 0))
        rq["hold_until_accept"] = parse_int(event.get("hold_until_accept", 0))
        words = [parse_int(x) for x in event.get("wdata_words", [])]
        bytes_ = [parse_int(x) for x in event.get("wstrb_bytes", [])]
        rq["wdata_words"] = (words + [0] * WRITE_WORDS)[:WRITE_WORDS]
        rq["wstrb_bytes"] = (bytes_ + [0] * WRITE_STRB_BYTES)[:WRITE_STRB_BYTES]
    elif t == "read_resp_ready_mask":
        frame["read_resp_ready_mask"] = parse_int(event["value"])
    elif t == "write_resp_ready_mask":
        frame["write_resp_ready_mask"] = parse_int(event["value"])
    elif t == "axi_arready":
        frame["axi_arready"] = parse_int(event["value"])
    elif t == "axi_awready":
        frame["axi_awready"] = parse_int(event["value"])
    elif t == "axi_wready":
        frame["axi_wready"] = parse_int(event["value"])
    elif t == "axi_b":
        frame["axi_bvalid"] = parse_int(event.get("valid", 1))
        frame["axi_bid"] = parse_int(event.get("id", 0))
        frame["axi_bresp"] = parse_int(event.get("resp", 0))
    elif t == "axi_r":
        frame["axi_rvalid"] = parse_int(event.get("valid", 1))
        frame["axi_rid"] = parse_int(event.get("id", 0))
        frame["axi_rresp"] = parse_int(event.get("resp", 0))
        frame["axi_rlast"] = parse_int(event.get("last", 1))
        words = [parse_int(x) for x in event.get("data_words", [])]
        frame["axi_rdata_words"] = (words + [0] * READ_BEAT_WORDS)[:READ_BEAT_WORDS]
    elif t == "mem_read_line_resp":
        frame["mem_read_line_resp_valid"] = parse_int(event.get("valid", 1))
        words = [parse_int(x) for x in event.get("data_words", [])]
        frame["mem_read_line_resp_words"] = (words + [0] * LINE_WORDS)[:LINE_WORDS]
    else:
        raise ValueError(f"unsupported event type: {t}")


def flatten_words(words, total_bits):
    value = 0
    for idx, word in enumerate(words):
        value |= (word & 0xFFFFFFFF) << (idx * 32)
    width_hex = (total_bits + 3) // 4
    return f"{total_bits}'h{value:0{width_hex}x}"


def flatten_bytes(bytes_, total_bits):
    value = 0
    for idx, b in enumerate(bytes_):
        value |= (b & 0x1) << idx
    width_hex = (total_bits + 3) // 4
    return f"{total_bits}'h{value:0{width_hex}x}"


def emit_header(frames, final_mem_samples, final_mmio_samples,
                final_mapped_samples, out_path):
    lines = []
    lines.append("#pragma once")
    lines.append("#include <array>")
    lines.append("#include <cstdint>")
    lines.append("namespace equiv_case {")
    lines.append(f"constexpr int kNumCycles = {len(frames)};")
    lines.append(f"constexpr int kNumFinalMemSamples = {len(final_mem_samples)};")
    lines.append(f"constexpr int kNumFinalMmioSamples = {len(final_mmio_samples)};")
    lines.append(f"constexpr int kNumFinalMappedSamples = {len(final_mapped_samples)};")
    if final_mem_samples:
        addrs = ", ".join(f"0x{x:08x}u" for x in final_mem_samples)
        lines.append(
            f"inline constexpr std::array<uint32_t, kNumFinalMemSamples> "
            f"kFinalMemSampleAddrs = {{{addrs}}};"
        )
    else:
        lines.append(
            "inline constexpr std::array<uint32_t, kNumFinalMemSamples> "
            "kFinalMemSampleAddrs = {};"
        )
    if final_mmio_samples:
        addrs = ", ".join(f"0x{x:08x}u" for x in final_mmio_samples)
        lines.append(
            f"inline constexpr std::array<uint32_t, kNumFinalMmioSamples> "
            f"kFinalMmioSampleAddrs = {{{addrs}}};"
        )
    else:
        lines.append(
            "inline constexpr std::array<uint32_t, kNumFinalMmioSamples> "
            "kFinalMmioSampleAddrs = {};"
        )
    if final_mapped_samples:
        addrs = ", ".join(f"0x{x:08x}u" for x in final_mapped_samples)
        lines.append(
            f"inline constexpr std::array<uint32_t, kNumFinalMappedSamples> "
            f"kFinalMappedSampleAddrs = {{{addrs}}};"
        )
    else:
        lines.append(
            "inline constexpr std::array<uint32_t, kNumFinalMappedSamples> "
            "kFinalMappedSampleAddrs = {};"
        )
    lines.append("struct ReadReq { bool valid; uint32_t addr; uint8_t size; uint8_t id; bool bypass; bool hold_until_accept; };")
    lines.append("struct WriteReq { bool valid; uint32_t addr; uint8_t size; uint8_t id; bool bypass; bool hold_until_accept; std::array<uint32_t, 16> wdata_words; std::array<uint8_t, 64> wstrb_bytes; };")
    lines.append("struct Frame {")
    lines.append("  uint8_t mode_req;")
    lines.append("  uint32_t llc_mapped_offset_req;")
    lines.append("  bool invalidate_all_valid;")
    lines.append("  bool invalidate_line_valid;")
    lines.append("  uint32_t invalidate_line_addr;")
    lines.append("  uint8_t read_resp_ready_mask;")
    lines.append("  uint8_t write_resp_ready_mask;")
    lines.append("  bool axi_arready; bool axi_awready; bool axi_wready;")
    lines.append("  bool axi_bvalid; uint8_t axi_bid; uint8_t axi_bresp;")
    lines.append("  bool axi_rvalid; uint8_t axi_rid; uint8_t axi_rresp; bool axi_rlast;")
    lines.append("  std::array<uint32_t, 8> axi_rdata_words;")
    lines.append("  bool mem_read_line_resp_valid;")
    lines.append("  std::array<uint32_t, 16> mem_read_line_resp_words;")
    lines.append("  std::array<ReadReq, 4> read_req;")
    lines.append("  std::array<WriteReq, 2> write_req;")
    lines.append("};")
    lines.append("inline constexpr Frame kFrames[kNumCycles] = {")
    for frame in frames:
        rr = []
        for r in frame["read_req"]:
            rr.append(f"ReadReq{{{int(r['valid'])}, 0x{r['addr']:08x}u, {r['size']}, {r['id']}, {int(r['bypass'])}, {int(r['hold_until_accept'])}}}")
        wr = []
        for w in frame["write_req"]:
            words = ", ".join(f"0x{x:08x}u" for x in w["wdata_words"])
            strb = ", ".join(str(int(x)) for x in w["wstrb_bytes"])
            wr.append(
                "WriteReq{" +
                f"{int(w['valid'])}, 0x{w['addr']:08x}u, {w['size']}, {w['id']}, {int(w['bypass'])}, {int(w['hold_until_accept'])}, " +
                f"{{{words}}}, {{{strb}}}" +
                "}"
            )
        rbeat = ", ".join(f"0x{x:08x}u" for x in frame["axi_rdata_words"][:READ_BEAT_WORDS])
        line_words = ", ".join(
            f"0x{x:08x}u" for x in frame["mem_read_line_resp_words"][:LINE_WORDS]
        )
        lines.append(
            "  Frame{" +
            f"{frame['mode_req']}, 0x{frame['llc_mapped_offset_req']:08x}u, {int(frame['invalidate_all_valid'])}, "
            f"{int(frame['invalidate_line_valid'])}, 0x{frame['invalidate_line_addr']:08x}u, "
            f"{frame['read_resp_ready_mask']}, {frame['write_resp_ready_mask']}, "
            f"{int(frame['axi_arready'])}, {int(frame['axi_awready'])}, {int(frame['axi_wready'])}, "
            f"{int(frame['axi_bvalid'])}, {frame['axi_bid']}, {frame['axi_bresp']}, "
            f"{int(frame['axi_rvalid'])}, {frame['axi_rid']}, {frame['axi_rresp']}, {int(frame['axi_rlast'])}, "
            f"{{{rbeat}}}, {int(frame['mem_read_line_resp_valid'])}, {{{line_words}}}, "
            f"{{{', '.join(rr)}}}, {{{', '.join(wr)}}}" +
            "},"
        )
    lines.append("};")
    lines.append("}")
    out_path.write_text("\n".join(lines) + "\n")


def emit_verilog(frames, final_mem_samples, final_mmio_samples,
                 final_mapped_samples, out_path):
    lines = []
    lines.append("localparam integer EQUIV_NUM_CYCLES = %d;" % len(frames))
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MEM_SAMPLES = %d;" %
        len(final_mem_samples)
    )
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MEM_STORAGE_SAMPLES = "
        "(EQUIV_NUM_FINAL_MEM_SAMPLES > 0) ? EQUIV_NUM_FINAL_MEM_SAMPLES : 1;"
    )
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MMIO_SAMPLES = %d;" %
        len(final_mmio_samples)
    )
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MMIO_STORAGE_SAMPLES = "
        "(EQUIV_NUM_FINAL_MMIO_SAMPLES > 0) ? EQUIV_NUM_FINAL_MMIO_SAMPLES : 1;"
    )
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MAPPED_SAMPLES = %d;" %
        len(final_mapped_samples)
    )
    lines.append(
        "localparam integer EQUIV_NUM_FINAL_MAPPED_STORAGE_SAMPLES = "
        "(EQUIV_NUM_FINAL_MAPPED_SAMPLES > 0) ? EQUIV_NUM_FINAL_MAPPED_SAMPLES : 1;"
    )
    lines.append(
        "reg [31:0] stim_final_mem_sample_addr "
        "[0:EQUIV_NUM_FINAL_MEM_STORAGE_SAMPLES-1];"
    )
    lines.append(
        "reg [31:0] stim_final_mmio_sample_addr "
        "[0:EQUIV_NUM_FINAL_MMIO_STORAGE_SAMPLES-1];"
    )
    lines.append(
        "reg [31:0] stim_final_mapped_sample_addr "
        "[0:EQUIV_NUM_FINAL_MAPPED_STORAGE_SAMPLES-1];"
    )
    lines.append("reg [1:0] stim_mode_req [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [31:0] stim_offset_req [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_invalidate_all [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_invalidate_line_valid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [31:0] stim_invalidate_line_addr [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [3:0] stim_read_resp_ready_mask [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_write_resp_ready_mask [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_arready [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_awready [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_wready [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_bvalid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [5:0] stim_axi_bid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_axi_bresp [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_rvalid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [5:0] stim_axi_rid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_axi_rresp [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_axi_rlast [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [255:0] stim_axi_rdata [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg stim_mem_read_line_resp_valid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [511:0] stim_mem_read_line_resp_data [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [3:0] stim_read_req_valid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [127:0] stim_read_req_addr [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [31:0] stim_read_req_size [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [15:0] stim_read_req_id [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [3:0] stim_read_req_bypass [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [3:0] stim_read_req_hold [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_write_req_valid [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [63:0] stim_write_req_addr [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1023:0] stim_write_req_wdata [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [127:0] stim_write_req_wstrb [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [15:0] stim_write_req_size [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [7:0] stim_write_req_id [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_write_req_bypass [0:EQUIV_NUM_CYCLES-1];")
    lines.append("reg [1:0] stim_write_req_hold [0:EQUIV_NUM_CYCLES-1];")
    lines.append("integer equiv_init_idx;")
    lines.append("initial begin")
    lines.append("  for (equiv_init_idx = 0; equiv_init_idx < EQUIV_NUM_CYCLES; equiv_init_idx = equiv_init_idx + 1) begin")
    lines.append("    stim_mode_req[equiv_init_idx] = 2'd0;")
    lines.append("    stim_offset_req[equiv_init_idx] = 32'd0;")
    lines.append("    stim_invalidate_all[equiv_init_idx] = 1'b0;")
    lines.append("    stim_invalidate_line_valid[equiv_init_idx] = 1'b0;")
    lines.append("    stim_invalidate_line_addr[equiv_init_idx] = 32'd0;")
    lines.append("    stim_read_resp_ready_mask[equiv_init_idx] = 4'd0;")
    lines.append("    stim_write_resp_ready_mask[equiv_init_idx] = 2'd0;")
    lines.append("    stim_axi_arready[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_awready[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_wready[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_bvalid[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_bid[equiv_init_idx] = 6'd0;")
    lines.append("    stim_axi_bresp[equiv_init_idx] = 2'd0;")
    lines.append("    stim_axi_rvalid[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_rid[equiv_init_idx] = 6'd0;")
    lines.append("    stim_axi_rresp[equiv_init_idx] = 2'd0;")
    lines.append("    stim_axi_rlast[equiv_init_idx] = 1'b0;")
    lines.append("    stim_axi_rdata[equiv_init_idx] = 256'd0;")
    lines.append("    stim_mem_read_line_resp_valid[equiv_init_idx] = 1'b0;")
    lines.append("    stim_mem_read_line_resp_data[equiv_init_idx] = 512'd0;")
    lines.append("    stim_read_req_valid[equiv_init_idx] = 4'd0;")
    lines.append("    stim_read_req_addr[equiv_init_idx] = 128'd0;")
    lines.append("    stim_read_req_size[equiv_init_idx] = 32'd0;")
    lines.append("    stim_read_req_id[equiv_init_idx] = 16'd0;")
    lines.append("    stim_read_req_bypass[equiv_init_idx] = 4'd0;")
    lines.append("    stim_read_req_hold[equiv_init_idx] = 4'd0;")
    lines.append("    stim_write_req_valid[equiv_init_idx] = 2'd0;")
    lines.append("    stim_write_req_addr[equiv_init_idx] = 64'd0;")
    lines.append("    stim_write_req_wdata[equiv_init_idx] = 1024'd0;")
    lines.append("    stim_write_req_wstrb[equiv_init_idx] = 128'd0;")
    lines.append("    stim_write_req_size[equiv_init_idx] = 16'd0;")
    lines.append("    stim_write_req_id[equiv_init_idx] = 8'd0;")
    lines.append("    stim_write_req_bypass[equiv_init_idx] = 2'd0;")
    lines.append("    stim_write_req_hold[equiv_init_idx] = 2'd0;")
    lines.append("  end")
    lines.append("  for (equiv_init_idx = 0; equiv_init_idx < EQUIV_NUM_FINAL_MEM_STORAGE_SAMPLES; equiv_init_idx = equiv_init_idx + 1) begin")
    lines.append("    stim_final_mem_sample_addr[equiv_init_idx] = 32'd0;")
    lines.append("  end")
    lines.append("  for (equiv_init_idx = 0; equiv_init_idx < EQUIV_NUM_FINAL_MMIO_STORAGE_SAMPLES; equiv_init_idx = equiv_init_idx + 1) begin")
    lines.append("    stim_final_mmio_sample_addr[equiv_init_idx] = 32'd0;")
    lines.append("  end")
    lines.append("  for (equiv_init_idx = 0; equiv_init_idx < EQUIV_NUM_FINAL_MAPPED_STORAGE_SAMPLES; equiv_init_idx = equiv_init_idx + 1) begin")
    lines.append("    stim_final_mapped_sample_addr[equiv_init_idx] = 32'd0;")
    lines.append("  end")
    for i, addr in enumerate(final_mem_samples):
        lines.append(f"  stim_final_mem_sample_addr[{i}] = 32'h{addr:08x};")
    for i, addr in enumerate(final_mmio_samples):
        lines.append(f"  stim_final_mmio_sample_addr[{i}] = 32'h{addr:08x};")
    for i, addr in enumerate(final_mapped_samples):
        lines.append(f"  stim_final_mapped_sample_addr[{i}] = 32'h{addr:08x};")
    for i, frame in enumerate(frames):
        lines.append(f"  stim_mode_req[{i}] = 2'd{frame['mode_req']};")
        lines.append(f"  stim_offset_req[{i}] = 32'h{frame['llc_mapped_offset_req']:08x};")
        lines.append(f"  stim_invalidate_all[{i}] = 1'b{int(frame['invalidate_all_valid'])};")
        lines.append(f"  stim_invalidate_line_valid[{i}] = 1'b{int(frame['invalidate_line_valid'])};")
        lines.append(f"  stim_invalidate_line_addr[{i}] = 32'h{frame['invalidate_line_addr']:08x};")
        lines.append(f"  stim_read_resp_ready_mask[{i}] = 4'h{frame['read_resp_ready_mask']:x};")
        lines.append(f"  stim_write_resp_ready_mask[{i}] = 2'h{frame['write_resp_ready_mask']:x};")
        lines.append(f"  stim_axi_arready[{i}] = 1'b{int(frame['axi_arready'])};")
        lines.append(f"  stim_axi_awready[{i}] = 1'b{int(frame['axi_awready'])};")
        lines.append(f"  stim_axi_wready[{i}] = 1'b{int(frame['axi_wready'])};")
        lines.append(f"  stim_axi_bvalid[{i}] = 1'b{int(frame['axi_bvalid'])};")
        lines.append(f"  stim_axi_bid[{i}] = 6'h{frame['axi_bid']:x};")
        lines.append(f"  stim_axi_bresp[{i}] = 2'h{frame['axi_bresp']:x};")
        lines.append(f"  stim_axi_rvalid[{i}] = 1'b{int(frame['axi_rvalid'])};")
        lines.append(f"  stim_axi_rid[{i}] = 6'h{frame['axi_rid']:x};")
        lines.append(f"  stim_axi_rresp[{i}] = 2'h{frame['axi_rresp']:x};")
        lines.append(f"  stim_axi_rlast[{i}] = 1'b{int(frame['axi_rlast'])};")
        lines.append(f"  stim_axi_rdata[{i}] = {flatten_words(frame['axi_rdata_words'][:READ_BEAT_WORDS], 256)};")
        lines.append(f"  stim_mem_read_line_resp_valid[{i}] = 1'b{int(frame['mem_read_line_resp_valid'])};")
        lines.append(f"  stim_mem_read_line_resp_data[{i}] = {flatten_words(frame['mem_read_line_resp_words'][:LINE_WORDS], 512)};")
        read_valid = 0
        read_addr_words = []
        read_size = 0
        read_id = 0
        read_bypass = 0
        read_hold = 0
        for m, req in enumerate(frame["read_req"]):
            read_valid |= (req["valid"] & 1) << m
            read_bypass |= (req["bypass"] & 1) << m
            read_hold |= (req["hold_until_accept"] & 1) << m
            read_addr_words.append(req["addr"])
            read_size |= (req["size"] & 0xFF) << (m * 8)
            read_id |= (req["id"] & 0xF) << (m * 4)
        lines.append(f"  stim_read_req_valid[{i}] = 4'h{read_valid:x};")
        lines.append(f"  stim_read_req_addr[{i}] = {flatten_words(read_addr_words, 128)};")
        lines.append(f"  stim_read_req_size[{i}] = 32'h{read_size:08x};")
        lines.append(f"  stim_read_req_id[{i}] = 16'h{read_id:04x};")
        lines.append(f"  stim_read_req_bypass[{i}] = 4'h{read_bypass:x};")
        lines.append(f"  stim_read_req_hold[{i}] = 4'h{read_hold:x};")
        write_valid = 0
        write_addr_words = []
        write_wdata_words = []
        write_wstrb_bits = []
        write_size = 0
        write_id = 0
        write_bypass = 0
        write_hold = 0
        for m, req in enumerate(frame["write_req"]):
            write_valid |= (req["valid"] & 1) << m
            write_bypass |= (req["bypass"] & 1) << m
            write_hold |= (req["hold_until_accept"] & 1) << m
            write_addr_words.append(req["addr"])
            write_size |= (req["size"] & 0xFF) << (m * 8)
            write_id |= (req["id"] & 0xF) << (m * 4)
            write_wdata_words.extend(req["wdata_words"])
            write_wstrb_bits.extend(req["wstrb_bytes"])
        lines.append(f"  stim_write_req_valid[{i}] = 2'h{write_valid:x};")
        lines.append(f"  stim_write_req_addr[{i}] = {flatten_words(write_addr_words, 64)};")
        lines.append(f"  stim_write_req_wdata[{i}] = {flatten_words(write_wdata_words, 1024)};")
        lines.append(f"  stim_write_req_wstrb[{i}] = {flatten_bytes(write_wstrb_bits, 128)};")
        lines.append(f"  stim_write_req_size[{i}] = 16'h{write_size:04x};")
        lines.append(f"  stim_write_req_id[{i}] = 8'h{write_id:02x};")
        lines.append(f"  stim_write_req_bypass[{i}] = 2'h{write_bypass:x};")
        lines.append(f"  stim_write_req_hold[{i}] = 2'h{write_hold:x};")
    lines.append("end")
    out_path.write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seed")
    ap.add_argument("outdir")
    args = ap.parse_args()
    seed = json.loads(Path(args.seed).read_text())
    warmup_cycles = parse_int(seed.get("warmup_cycles", 0))
    max_cycle = 0
    for ev in seed.get("events", []):
        start_cycle = warmup_cycles + parse_int(ev["cycle"])
        duration = parse_int(ev.get("duration", 1))
        max_cycle = max(max_cycle, start_cycle + max(duration, 1) - 1)
    total_cycles = max_cycle + 1 + parse_int(seed.get("tail_cycles", 0))
    frames = [zero_frame(seed.get("defaults", {})) for _ in range(total_cycles)]
    for ev in seed.get("events", []):
        start_cycle = warmup_cycles + parse_int(ev["cycle"])
        duration = max(parse_int(ev.get("duration", 1)), 1)
        for cycle in range(start_cycle, start_cycle + duration):
            apply_event(frames[cycle], ev)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    final_mem_samples = parse_final_mem_samples(seed)
    final_mmio_samples = parse_final_mmio_samples(seed)
    final_mapped_samples = parse_final_mapped_samples(seed)
    emit_header(frames, final_mem_samples, final_mmio_samples,
                final_mapped_samples, outdir / "case_data.h")
    emit_verilog(frames, final_mem_samples, final_mmio_samples,
                 final_mapped_samples, outdir / "equiv_case.vh")
    meta = {
        "name": seed.get("name", Path(args.seed).stem),
        "num_cycles": total_cycles,
        "warmup_cycles": warmup_cycles,
        "final_mem_samples": final_mem_samples,
        "final_mmio_samples": final_mmio_samples,
        "final_mapped_samples": final_mapped_samples,
    }
    (outdir / "case_meta.json").write_text(json.dumps(meta, indent=2) + "\n")


if __name__ == "__main__":
    main()
