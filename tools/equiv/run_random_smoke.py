#!/usr/bin/env python3
import argparse
import json
import random
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools" / "equiv"
OUT_ROOT = TOOLS_DIR / "out" / "random_smoke"

DDR_READ_ADDRS = [
    0x80001000,
    0x80002000,
    0x80003000,
    0x80004000,
    0x80005000,
    0x80006000,
]
MMIO_ADDRS = [
    0x10000000,
    0x10000004,
    0x10000008,
    0x1000000C,
]
MAPPED_ADDRS = [
    0x40001000,
    0x40001040,
    0x40001100,
    0x40001140,
]
MODE2_DDR_ADDRS = [
    0x80004000,
    0x80004040,
    0x80004100,
    0x80004140,
]


def run(cmd, **kwargs):
    print("+", " ".join(str(x) for x in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def word_list_from_base(base, count):
    return [f"0x{(base + idx * 0x01010101) & 0xffffffff:08x}" for idx in range(count)]


def make_write_event(cycle, addr, req_id, data0):
    return {
        "cycle": cycle,
        "type": "write_req",
        "master": 0,
        "addr": f"0x{addr:08x}",
        "size": 3,
        "id": req_id,
        "bypass": 0,
        "hold_until_accept": 1,
        "wdata_words": [f"0x{data0 & 0xffffffff:08x}"] + [0] * 15,
        "wstrb_bytes": [1, 1, 1, 1] + [0] * 60,
    }


def make_b_event(cycle):
    return {
        "cycle": cycle,
        "type": "axi_b",
        "valid": 1,
        "id": 0,
        "resp": 0,
    }


def make_bypass_read_events(cycle, addr, req_id, base_word):
    return [
        {
            "cycle": cycle,
            "type": "read_req",
            "master": 0,
            "addr": f"0x{addr:08x}",
            "size": 3,
            "id": req_id,
            "bypass": 1,
            "hold_until_accept": 1,
        },
        {
            "cycle": cycle + 12,
            "type": "axi_r",
            "valid": 1,
            "id": 0,
            "resp": 0,
            "last": 1,
            "data_words": word_list_from_base(base_word, 8),
        },
    ]


def make_mmio_write_events(cycle, addr, req_id, data0):
    return [make_write_event(cycle, addr, req_id, data0), make_b_event(cycle + 12)]


def make_invalidate_line_event(cycle, addr):
    return {
        "cycle": cycle,
        "type": "invalidate_line",
        "addr": f"0x{addr:08x}",
    }


def make_mode2_ddr_write_events(cycle, addr, data0):
    return [
        {"cycle": cycle, "type": "set_mode", "mode": 2},
        make_write_event(cycle + 4, addr, 5, data0),
        make_b_event(cycle + 16),
        {"cycle": cycle + 20, "type": "set_mode", "mode": 1},
    ]


def make_mode2_mapped_write_events(cycle, addr, data0):
    return [
        {"cycle": cycle, "type": "set_mode", "mode": 2},
        make_write_event(cycle + 4, addr, 6, data0),
        {"cycle": cycle + 20, "type": "set_mode", "mode": 1},
    ]


def build_seed(rng, case_idx):
    prefix_op_count = rng.randint(2, 4)
    cycle = 4
    events = []
    final_mem_samples = []
    final_mmio_samples = []
    final_mapped_samples = []
    retired_read_ids = [1, 2, 3, 4]
    retired_write_ids = [5, 6, 7]

    for _ in range(prefix_op_count):
        op = rng.choice([
            "bypass_read",
            "mmio_write",
            "invalidate_line",
        ])
        if op == "bypass_read":
            req_id = rng.choice(retired_read_ids)
            addr = rng.choice(DDR_READ_ADDRS)
            base_word = rng.getrandbits(32) & 0xF0F0F0F0
            events.extend(make_bypass_read_events(cycle, addr, req_id, base_word))
            cycle += 24
        elif op == "mmio_write":
            req_id = rng.choice(retired_write_ids)
            addr = rng.choice(MMIO_ADDRS)
            data0 = rng.getrandbits(32)
            events.extend(make_mmio_write_events(cycle, addr, req_id, data0))
            if addr not in final_mmio_samples:
                final_mmio_samples.append(addr)
            cycle += 24
        elif op == "invalidate_line":
            addr = rng.choice(DDR_READ_ADDRS)
            events.append(make_invalidate_line_event(cycle, addr))
            cycle += 12

    # Keep mode2 activity, if any, as a terminal template. This avoids relying on
    # unfinished mode2 write retirement semantics before another op is launched.
    tail_mode2 = rng.choice(["none", "mode2_ddr_write", "mode2_mapped_write"])
    if tail_mode2 == "mode2_ddr_write":
        addr = rng.choice(MODE2_DDR_ADDRS)
        data0 = rng.getrandbits(32)
        events.extend(make_mode2_ddr_write_events(cycle, addr, data0))
        if addr not in final_mem_samples:
            final_mem_samples.append(addr)
        cycle += 28
    elif tail_mode2 == "mode2_mapped_write":
        addr = rng.choice(MAPPED_ADDRS)
        data0 = rng.getrandbits(32)
        events.extend(make_mode2_mapped_write_events(cycle, addr, data0))
        if addr not in final_mapped_samples:
            final_mapped_samples.append(addr)
        cycle += 28

    seed = {
        "name": f"random_smoke_{case_idx:02d}",
        "warmup_cycles": 8300,
        "tail_cycles": 24,
        "defaults": {
            "mode_req": 1,
            "llc_mapped_offset_req": "0x40000000",
            "read_resp_ready_mask": 15,
            "write_resp_ready_mask": 3,
            "axi_arready": 1,
            "axi_awready": 1,
            "axi_wready": 1,
            "axi_bvalid": 0,
            "axi_rvalid": 0,
        },
        "events": events,
    }
    if final_mem_samples:
        seed["final_mem_samples"] = [f"0x{x:08x}" for x in final_mem_samples]
    if final_mmio_samples:
        seed["final_mmio_samples"] = [f"0x{x:08x}" for x in final_mmio_samples]
    if final_mapped_samples:
        seed["final_mapped_samples"] = [f"0x{x:08x}" for x in final_mapped_samples]
    return seed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=8)
    ap.add_argument("--root-seed", type=int, default=20260420)
    ap.add_argument("--rtl-node", default="eda-03")
    ap.add_argument(
        "--submodule-root",
        default=str(REPO_ROOT / "axi-interconnect-kit"),
        help="path to the submodule worktree used for both C++ and RTL builds",
    )
    args = ap.parse_args()

    seed_dir = OUT_ROOT / f"seedset_{args.root_seed}_{args.count}"
    seed_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "count": args.count,
        "root_seed": args.root_seed,
        "seeds": [],
    }

    seed_paths = []
    rng = random.Random(args.root_seed)
    for idx in range(args.count):
        seed = build_seed(rng, idx)
        path = seed_dir / f"{seed['name']}.json"
        path.write_text(json.dumps(seed, indent=2) + "\n")
        manifest["seeds"].append(path.name)
        seed_paths.append(path)
    (seed_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    cmd = [
        "python3",
        str(TOOLS_DIR / "run_mvp.py"),
        "--rtl-node",
        args.rtl_node,
        "--submodule-root",
        str(Path(args.submodule_root).resolve()),
    ]
    for seed_path in seed_paths:
        cmd.extend(["--seed", str(seed_path)])
    run(cmd)


if __name__ == "__main__":
    main()
