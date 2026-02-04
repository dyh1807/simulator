#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import sys


def generate_outer_header(
    *,
    include_header: str,
    module_type: str,
    pi_po_namespace: str,
    header_guard: str,
    width_type: str,
    function_name: str,
    use_c_linkage: bool,
) -> str:
    c_linkage_begin = ""
    c_linkage_end = ""
    if use_c_linkage:
        c_linkage_begin = (
            "#ifdef __cplusplus\n"
            'extern "C" {\n'
            "#endif\n\n"
        )
        c_linkage_end = "\n#ifdef __cplusplus\n}\n#endif\n"

    return (
        f"#ifndef {header_guard}\n"
        f"#define {header_guard}\n"
        f'#include "{include_header}"\n\n'
        f"{c_linkage_begin}"
        f"extern const {width_type} PI_WIDTH = static_cast<{width_type}>({pi_po_namespace}::PI_WIDTH);\n"
        f"extern const {width_type} PO_WIDTH = static_cast<{width_type}>({pi_po_namespace}::PO_WIDTH);\n\n"
        f"void {function_name}(bool *pi, bool *po) {{\n"
        f"  thread_local {module_type} module;\n"
        f"  {pi_po_namespace}::eval_comb(module, pi, po);\n"
        f"}}\n"
        f"{c_linkage_end}"
        f"#endif\n"
    )


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Generate an outer io_generator header (demo.h-like) for a given pi/po module."
    )
    ap.add_argument(
        "--out",
        required=True,
        help="Output header path to write (e.g., io_generator_outer.h)",
    )
    ap.add_argument(
        "--include-header",
        required=True,
        help='Header to #include that provides PI_WIDTH/PO_WIDTH + eval_comb (e.g., front-end/icache/include/icache_v1_pi_po.h)',
    )
    ap.add_argument(
        "--module-type",
        required=True,
        help="C++ module type to instantiate (e.g., icache_module_n::ICache)",
    )
    ap.add_argument(
        "--pi-po-namespace",
        required=True,
        help="Namespace that defines PI_WIDTH/PO_WIDTH and eval_comb (e.g., icache_module_n::icache_v1_pi_po)",
    )
    ap.add_argument(
        "--header-guard",
        default="IO_GENERATOR_OUTER_H",
        help="Header guard macro name (default: IO_GENERATOR_OUTER_H)",
    )
    ap.add_argument(
        "--width-type",
        default="int",
        help="Type of PI_WIDTH/PO_WIDTH symbols (default: int)",
    )
    ap.add_argument(
        "--function-name",
        default="io_generator_outer",
        help="Function name to emit (default: io_generator_outer)",
    )
    ap.add_argument(
        "--c-linkage",
        action="store_true",
        help='Wrap exported symbols in extern "C" for stable linkage.',
    )
    args = ap.parse_args()

    repo_root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))
    out_path = args.out
    if not os.path.isabs(out_path):
        out_path = os.path.join(repo_root, out_path)

    text = generate_outer_header(
        include_header=args.include_header,
        module_type=args.module_type,
        pi_po_namespace=args.pi_po_namespace,
        header_guard=args.header_guard,
        width_type=args.width_type,
        function_name=args.function_name,
        use_c_linkage=args.c_linkage,
    )

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

