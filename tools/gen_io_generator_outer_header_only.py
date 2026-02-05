#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class ModuleName:
    namespace: str
    class_name: str


def parse_module_type(module_type: str) -> ModuleName:
    parts = [p for p in module_type.split("::") if p]
    if not parts:
        raise ValueError(f"invalid module type: `{module_type}`")
    if len(parts) == 1:
        return ModuleName(namespace="", class_name=parts[0])
    return ModuleName(namespace="::".join(parts[:-1]), class_name=parts[-1])


def open_namespace(ns: str) -> Tuple[str, str]:
    if not ns:
        return "", ""
    parts = [p for p in ns.split("::") if p]
    if not parts:
        return "", ""
    open_lines = " ".join(f"namespace {p} {{" for p in parts) + "\n"
    close_lines = "}" * len(parts) + f" // namespace {parts[-1]}\n"
    return open_lines, close_lines


def _is_prefix_line(line: str) -> bool:
    s = line.lstrip()
    if s == "":
        return True
    if s.startswith("#include"):
        return True
    if s.startswith("//"):
        return True
    if s.startswith("/*") or s.startswith("*/") or s.startswith("*"):
        return True
    return False


def split_prefix_block(lines: Sequence[str]) -> Tuple[List[str], List[str]]:
    i = 0
    while i < len(lines) and _is_prefix_line(lines[i]):
        i += 1
    return list(lines[:i]), list(lines[i:])


def strip_cpp_includes(prefix_lines: List[str], strip_include_substrings: Sequence[str]) -> List[str]:
    out: List[str] = []
    for line in prefix_lines:
        if line.lstrip().startswith("#include"):
            if any(s in line for s in strip_include_substrings):
                continue
        out.append(line)
    return out


def inline_class_method_defs(
    code_lines: Sequence[str],
    *,
    class_name: str,
    drop_using_namespace: Optional[str],
) -> List[str]:
    # Prefix `inline` to out-of-class member function definitions to make
    # the implementation header-only friendly.
    #
    # Heuristic: match a line that begins a definition and contains `Class::`.
    # This is tuned for this repo's module .cpp style (brace on same line).
    #
    # Examples:
    #   ICache::ICache() {
    #   void ICache::reset() {
    #   void ICache::export_lookup_set_for_pc(...) const {
    #
    # We intentionally do NOT try to parse full C++.
    class_pat = re.escape(class_name)
    fn_re = re.compile(
        rf"^(?P<indent>\s*)(?!inline\b)(?P<prefix>(?:[A-Za-z_][\w:<>\s\*&]*\s+)?)"
        rf"{class_pat}::(?P<fn>[A-Za-z_]\w*)\s*\("
    )

    out: List[str] = []
    using_line = None
    if drop_using_namespace:
        using_line = f"using namespace {drop_using_namespace};"

    for line in code_lines:
        if using_line and line.strip() == using_line:
            continue
        m = fn_re.match(line)
        if m:
            indent = m.group("indent")
            out.append(indent + "inline " + line[len(indent) :])
        else:
            out.append(line)
    return out


def maybe_wrap_namespace(code: str, wrap_namespace: str) -> str:
    if not wrap_namespace:
        return code
    # Avoid double wrapping if the file already contains a matching namespace
    # declaration at top level (best-effort).
    ns_last = wrap_namespace.split("::")[-1]
    if re.search(rf"^\s*namespace\s+{re.escape(ns_last)}\b", code, flags=re.MULTILINE):
        return code
    ns_open, ns_close = open_namespace(wrap_namespace)
    return ns_open + code + ns_close


def generate_outer_header_only(
    *,
    include_header: str,
    module_type: str,
    pi_po_namespace: str,
    impl_cpp_path: str,
    header_guard: str,
    width_type: str,
    function_name: str,
    strip_include_substrings: Sequence[str],
    wrap_namespace: Optional[str],
    use_c_linkage: bool,
) -> str:
    module = parse_module_type(module_type)

    with open(impl_cpp_path, "r", encoding="utf-8") as f:
        cpp_text = f.read()

    cpp_lines = cpp_text.splitlines()
    prefix_lines, code_lines = split_prefix_block(cpp_lines)
    prefix_lines = strip_cpp_includes(prefix_lines, strip_include_substrings)

    drop_using = wrap_namespace if wrap_namespace else None
    code_lines = inline_class_method_defs(
        code_lines,
        class_name=module.class_name,
        drop_using_namespace=drop_using,
    )
    impl_code = "\n".join(code_lines).rstrip() + "\n"
    if wrap_namespace:
        impl_code = maybe_wrap_namespace(impl_code, wrap_namespace)

    impl_text = "\n".join(prefix_lines).rstrip() + "\n" + impl_code

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
        f"{impl_text}\n"
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
        description="Generate a demo.h-style io_generator_outer header with embedded module implementation (.cpp) so it is header-only."
    )
    ap.add_argument("--out", required=True, help="Output header path to write.")
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
        "--impl-cpp",
        required=True,
        help="Path to the module implementation .cpp to embed (e.g., front-end/icache/icache_module.cpp)",
    )
    ap.add_argument(
        "--strip-include",
        action="append",
        default=[],
        help="Substring match: drop matching #include lines from the embedded .cpp (repeatable).",
    )
    ap.add_argument(
        "--wrap-namespace",
        default=None,
        help="Wrap embedded implementation in this namespace and drop `using namespace <ns>;` (best-effort).",
    )
    ap.add_argument("--header-guard", default="IO_GENERATOR_OUTER_H")
    ap.add_argument("--width-type", default="int")
    ap.add_argument("--function-name", default="io_generator_outer")
    ap.add_argument("--c-linkage", action="store_true")
    args = ap.parse_args()

    repo_root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir))
    out_path = args.out
    if not os.path.isabs(out_path):
        out_path = os.path.join(repo_root, out_path)

    impl_cpp_path = args.impl_cpp
    if not os.path.isabs(impl_cpp_path):
        impl_cpp_path = os.path.join(repo_root, impl_cpp_path)

    wrap_namespace = args.wrap_namespace
    if wrap_namespace is None:
        wrap_namespace = parse_module_type(args.module_type).namespace or None

    text = generate_outer_header_only(
        include_header=args.include_header,
        module_type=args.module_type,
        pi_po_namespace=args.pi_po_namespace,
        impl_cpp_path=impl_cpp_path,
        header_guard=args.header_guard,
        width_type=args.width_type,
        function_name=args.function_name,
        strip_include_substrings=args.strip_include,
        wrap_namespace=wrap_namespace,
        use_c_linkage=args.c_linkage,
    )

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

