#!/usr/bin/env python3
from __future__ import annotations

import os
import sys

# Import the generic generator from the same folder.
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_pi_po import generate_header  # noqa: E402
from gen_pi_po_bit_map import generate_bit_maps  # noqa: E402


def main() -> int:
    repo_root = os.path.normpath(os.path.join(THIS_DIR, os.pardir))

    header_path = os.path.join(
        repo_root, "front-end", "icache", "include", "icache_module_v2.h"
    )
    out_path = os.path.join(
        repo_root, "front-end", "icache", "include", "icache_v2_pi_po.h"
    )
    map_prefix = os.path.join(repo_root, "front-end", "icache", "include", "icache_v2")

    pi_fields = [
        ("in", "ICacheV2_in_t"),
        ("regs", "ICacheV2_regs_t"),
        ("lookup_in", "ICacheV2_lookup_in_t"),
    ]
    po_fields = [
        ("out", "ICacheV2_out_t"),
        ("reg_write", "ICacheV2_regs_t"),
        ("table_write", "ICacheV2_table_write_t"),
    ]

    text = generate_header(
        header_path=header_path,
        include_path="icache_module_v2.h",
        ns="icache_module_v2_n",
        out_ns="icache_v2_pi_po",
        wrapper_type="ICacheV2_IO_t",
        pi_fields=pi_fields,
        po_fields=po_fields,
        type_overrides={},
    )

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    bit_map_result = generate_bit_maps(
        header_path=header_path,
        namespace="icache_module_v2_n",
        pi_fields=pi_fields,
        po_fields=po_fields,
        type_overrides={},
        out_prefix=map_prefix,
        include_dirs=[
            os.path.join(repo_root, "front-end"),
            os.path.join(repo_root, "back-end", "include"),
            os.path.join(repo_root, "include"),
        ],
    )

    print(out_path)
    print(bit_map_result["pi_map_path"])
    print(bit_map_result["po_map_path"])
    print(f"PI_WIDTH={bit_map_result['pi_width']}")
    print(f"PO_WIDTH={bit_map_result['po_width']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
