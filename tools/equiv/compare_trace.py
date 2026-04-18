#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys


def load_lines(path):
    out = []
    for raw in Path(path).read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        out.append(line)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cpp_trace")
    ap.add_argument("rtl_trace")
    args = ap.parse_args()

    cpp = load_lines(args.cpp_trace)
    rtl = load_lines(args.rtl_trace)

    if cpp == rtl:
        print("TRACE_COMPARE_PASS")
        return 0

    print("TRACE_COMPARE_FAIL")
    limit = min(len(cpp), len(rtl))
    for idx in range(limit):
      if cpp[idx] != rtl[idx]:
        print(f"first_diff_index={idx}")
        print(f"cpp: {cpp[idx]}")
        print(f"rtl: {rtl[idx]}")
        return 1
    print(f"length_mismatch cpp={len(cpp)} rtl={len(rtl)}")
    if len(cpp) > limit:
        print(f"cpp_extra: {cpp[limit]}")
    if len(rtl) > limit:
        print(f"rtl_extra: {rtl[limit]}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
