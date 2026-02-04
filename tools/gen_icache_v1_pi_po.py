#!/usr/bin/env python3
from __future__ import annotations

import os
import sys

# Import the generic generator from the same folder.
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_pi_po import generate_header  # noqa: E402


def main() -> int:
    repo_root = os.path.normpath(os.path.join(THIS_DIR, os.pardir))

    header_path = os.path.join(repo_root, "front-end", "icache", "include", "icache_module.h")
    out_path = os.path.join(repo_root, "front-end", "icache", "include", "icache_v1_pi_po.h")

    text = generate_header(
        header_path=header_path,
        include_path="icache_module.h",
        ns="icache_module_n",
        out_ns="icache_v1_pi_po",
        wrapper_type="ICache_IO_t",
        pi_fields=[
            ("in", "ICache_in_t"),
            ("regs", "ICache_regs_t"),
            ("lookup_in", "ICache_lookup_in_t"),
        ],
        po_fields=[
            ("out", "ICache_out_t"),
            ("reg_write", "ICache_reg_write_t"),
            ("table_write", "ICache_table_write_t"),
        ],
        type_overrides={},
    )

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

