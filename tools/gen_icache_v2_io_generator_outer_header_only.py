#!/usr/bin/env python3
from __future__ import annotations

import os
import sys

# Import generator helpers from the same folder.
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_io_generator_outer_header_only import generate_outer_header_only  # noqa: E402


def main() -> int:
    repo_root = os.path.normpath(os.path.join(THIS_DIR, os.pardir))

    out_path = os.path.join(repo_root, "io_generator_outer.h")
    impl_cpp_path = os.path.join(
        repo_root, "front-end", "icache", "icache_module_v2.cpp"
    )

    text = generate_outer_header_only(
        include_header="front-end/icache/include/icache_v2_pi_po.h",
        module_type="icache_module_v2_n::ICacheV2",
        pi_po_namespace="icache_module_v2_n::icache_v2_pi_po",
        impl_cpp_path=impl_cpp_path,
        header_guard="IO_GENERATOR_OUTER_H",
        width_type="int",
        function_name="io_generator_outer",
        strip_include_substrings=["include/icache_module_v2.h"],
        wrap_namespace="icache_module_v2_n",
        use_c_linkage=False,
    )

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
