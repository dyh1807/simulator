#!/usr/bin/env python3
import argparse
from pathlib import Path


def load_lines(path):
    out = []
    for raw in Path(path).read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        out.append(line)
    return out


def parse_event(line):
    parts = line.split()
    cycle = int(parts[0])
    etype = parts[1]
    fields = {}
    for token in parts[2:]:
        key, value = token.split("=", 1)
        fields[key] = value
    return {"cycle": cycle, "type": etype, "fields": fields, "raw": line}


def canonicalize(event):
    etype = event["type"]
    f = event["fields"]
    if etype == "MODE_ACTIVE":
        return (etype, f["mode"], f["offset"])
    if etype == "READ_ACCEPT":
        return (etype, f["m"], f["id"], f["addr"], f["size"], f["bypass"])
    if etype == "WRITE_ACCEPT":
        return (etype, f["m"], f["id"], f["addr"], f["size"], f["bypass"],
                f["data0"])
    if etype == "READ_RESP":
        return (etype, f["m"], f["id"], f["d0"])
    if etype == "WRITE_RESP":
        return (etype, f["m"], f["id"], f["code"])
    if etype == "MAINT_ACCEPT":
        return (etype, f.get("op"), f.get("addr"))
    if etype == "AXI_AR_HS":
        return (etype, f["addr"])
    if etype == "AXI_AW_HS":
        return (etype, f["addr"])
    if etype == "AXI_W_HS":
        return (etype, f["d0"], f["last"])
    return (etype, tuple(sorted(f.items())))


def load_events(path):
    return [parse_event(line) for line in load_lines(path)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cpp_trace")
    ap.add_argument("rtl_trace")
    ap.add_argument(
        "--ignore-maint-op",
        action="append",
        default=[],
        help="ignore MAINT_ACCEPT events for the given maintenance op",
    )
    args = ap.parse_args()

    cpp = load_events(args.cpp_trace)
    rtl = load_events(args.rtl_trace)
    ignore_maint_ops = set(args.ignore_maint_op)

    def normalize(events):
        out = []
        for ev in events:
            if ev["type"] == "MAINT_ACCEPT" and ev["fields"].get("op") in ignore_maint_ops:
                continue
            out.append(canonicalize(ev))
        return out

    cpp_norm = normalize(cpp)
    rtl_norm = normalize(rtl)

    if cpp_norm == rtl_norm:
        print("TRACE_COMPARE_PASS")
        return 0

    print("TRACE_COMPARE_FAIL")
    limit = min(len(cpp_norm), len(rtl_norm))
    for idx in range(limit):
        if cpp_norm[idx] != rtl_norm[idx]:
            print(f"first_diff_index={idx}")
            print(f"cpp_norm: {cpp_norm[idx]}")
            print(f"rtl_norm: {rtl_norm[idx]}")
            print(f"cpp_raw: {cpp[idx]['raw']}")
            print(f"rtl_raw: {rtl[idx]['raw']}")
            return 1
    print(f"length_mismatch cpp={len(cpp_norm)} rtl={len(rtl_norm)}")
    if len(cpp_norm) > limit:
        print(f"cpp_extra_norm: {cpp_norm[limit]}")
        print(f"cpp_extra_raw: {cpp[limit]['raw']}")
    if len(rtl_norm) > limit:
        print(f"rtl_extra_norm: {rtl_norm[limit]}")
        print(f"rtl_extra_raw: {rtl[limit]['raw']}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
