#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_io_generator_outer_header_only import generate_outer_header_only  # noqa: E402


def main() -> int:
    repo_root = os.path.normpath(os.path.join(THIS_DIR, os.pardir))

    subprocess.run(
        [sys.executable, os.path.join(THIS_DIR, "gen_mmu_ptw_pi_po.py")],
        check=True,
    )

    out_path = os.path.join(repo_root, "mmu_ptw_io_generator_outer.h")
    impl_cpp_path = os.path.join(repo_root, "mmu", "ptw_module.cpp")

    text = generate_outer_header_only(
        include_header="mmu/include/ptw_pi_po.h",
        module_type="ptw_module_n::PTWModule",
        pi_po_namespace="ptw_module_n::ptw_pi_po",
        impl_cpp_path=impl_cpp_path,
        header_guard="MMU_PTW_IO_GENERATOR_OUTER_H",
        width_type="int",
        function_name="io_generator_outer",
        strip_include_substrings=["include/ptw_module.h"],
        wrap_namespace="ptw_module_n",
        use_c_linkage=False,
    )

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

