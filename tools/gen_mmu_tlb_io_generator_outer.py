#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_io_generator_outer import generate_outer_header  # noqa: E402


def main() -> int:
    repo_root = os.path.normpath(os.path.join(THIS_DIR, os.pardir))

    subprocess.run(
        [sys.executable, os.path.join(THIS_DIR, "gen_mmu_tlb_pi_po.py")],
        check=True,
    )

    out_path = os.path.join(repo_root, "mmu_tlb_io_generator_outer.h")

    text = generate_outer_header(
        include_header="mmu/include/tlb_pi_po.h",
        module_type="tlb_module_n::TLBModule",
        pi_po_namespace="tlb_module_n::tlb_pi_po",
        header_guard="MMU_TLB_IO_GENERATOR_OUTER_H",
        width_type="int",
        function_name="io_generator_outer",
        use_c_linkage=False,
    )

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

