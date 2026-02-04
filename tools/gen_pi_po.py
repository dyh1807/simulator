#!/usr/bin/env python3
from __future__ import annotations

import argparse
import dataclasses
import os
import re
import sys
from typing import Dict, List, Sequence, Tuple


TYPE_WIDTH_DEFAULT: Dict[str, int] = {
    "bool": 1,
    # NOTE: For pi/po flattening we use *logical* widths, not storage widths.
    # The mapping below follows the repo's current bitvector conventions.
    "uint8": 8,
    "uint8_t": 8,
    "uint16": 6,
    "uint16_t": 6,
    "uint32": 32,
    "uint32_t": 32,
    "uint64": 64,
    "uint64_t": 64,
}


def strip_comments(text: str) -> str:
    # Remove /* ... */ then // ...
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def find_struct_body(text: str, struct_name: str) -> str:
    # Match `struct Name {` and extract the balanced body.
    m = re.search(rf"\bstruct\s+{re.escape(struct_name)}\s*\{{", text)
    if not m:
        raise ValueError(f"struct not found: {struct_name}")
    brace_start = m.end() - 1  # points at '{'
    depth = 0
    for i in range(brace_start, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace_start + 1 : i]
    raise ValueError(f"unterminated struct body: {struct_name}")


@dataclasses.dataclass(frozen=True)
class Field:
    type_name: str
    name: str
    dims: Tuple[str, ...]  # array dimensions as expressions


def parse_fields(struct_body: str) -> List[Field]:
    fields: List[Field] = []
    # Split by ';' as a coarse statement delimiter.
    for stmt in struct_body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        # Skip function declarations or nested type declarations.
        if "(" in s or ")" in s:
            continue
        if s.startswith("struct ") or s.startswith("enum ") or s.startswith("using "):
            continue
        # Drop default initializer.
        if "=" in s:
            s = s.split("=", 1)[0].strip()

        # Example: `uint32_t cache_data[ICACHE_V1_SET_NUM][ICACHE_V1_WAYS]`
        m = re.match(
            r"^(?P<type>[A-Za-z_]\w*(?:::\w+)*)\s+(?P<name>[A-Za-z_]\w*)\s*(?P<arrays>(?:\[[^\]]+\])*)$",
            s,
        )
        if not m:
            raise ValueError(f"cannot parse field declaration: `{stmt.strip()};`")

        type_name = m.group("type")
        name = m.group("name")
        arrays = m.group("arrays")
        dims = tuple(d.strip() for d in re.findall(r"\[([^\]]+)\]", arrays))
        fields.append(Field(type_name=type_name, name=name, dims=dims))
    return fields


def type_width_bits(type_name: str, overrides: Dict[str, int]) -> int:
    if type_name in overrides:
        return overrides[type_name]
    if type_name in TYPE_WIDTH_DEFAULT:
        return TYPE_WIDTH_DEFAULT[type_name]
    m = re.match(r"^(?:wire|reg)(\d+)_t$", type_name)
    if m:
        return int(m.group(1))
    raise ValueError(f"unsupported type for bitvector packing: {type_name}")


def cxx_qualified(ns: str, type_name: str) -> str:
    if not ns:
        return type_name
    if "::" in type_name:
        return type_name
    return f"{ns}::{type_name}"


def dim_product_expr(dims: Sequence[str]) -> str:
    if not dims:
        return "1"
    return " * ".join(f"({d})" for d in dims)


def gen_pack_scalar(type_name: str, width: int, value_expr: str, bits_name: str, idx_name: str) -> str:
    if width == 1 and type_name == "bool":
        return f"{bits_name}[{idx_name}++] = {value_expr};\n"
    # Use uint64_t as the working type.
    return (
        f"for (size_t b = 0; b < {width}; ++b) {{ {bits_name}[{idx_name}++] = ((static_cast<uint64_t>({value_expr}) >> b) & 1u) != 0; }}\n"
    )


def gen_unpack_scalar(type_name: str, width: int, dst_expr: str, bits_name: str, idx_name: str) -> str:
    if width == 1 and type_name == "bool":
        return f"{dst_expr} = {bits_name}[{idx_name}++];\n"
    return (
        f"{{ uint64_t tmp = 0; for (size_t b = 0; b < {width}; ++b) {{ if ({bits_name}[{idx_name}++]) tmp |= (uint64_t(1) << b); }} {dst_expr} = static_cast<{type_name}>(tmp); }}\n"
    )


def gen_pack_field(field: Field, width: int, obj_expr: str, bits_name: str, idx_name: str) -> str:
    access = f"{obj_expr}.{field.name}"
    if not field.dims:
        return gen_pack_scalar(field.type_name, width, access, bits_name, idx_name)

    # Multi-d array: generate nested loops.
    loop_vars = [f"i{d}" for d in range(len(field.dims))]
    code = ""
    for lv, dim in zip(loop_vars, field.dims):
        code += f"for (size_t {lv} = 0; {lv} < ({dim}); ++{lv}) {{\n"
    index_expr = "".join(f"[{lv}]" for lv in loop_vars)
    code += gen_pack_scalar(field.type_name, width, f"{access}{index_expr}", bits_name, idx_name)
    code += "}\n" * len(loop_vars)
    return code


def gen_unpack_field(field: Field, width: int, obj_expr: str, bits_name: str, idx_name: str) -> str:
    access = f"{obj_expr}.{field.name}"
    if not field.dims:
        return gen_unpack_scalar(field.type_name, width, access, bits_name, idx_name)

    loop_vars = [f"i{d}" for d in range(len(field.dims))]
    code = ""
    for lv, dim in zip(loop_vars, field.dims):
        code += f"for (size_t {lv} = 0; {lv} < ({dim}); ++{lv}) {{\n"
    index_expr = "".join(f"[{lv}]" for lv in loop_vars)
    code += gen_unpack_scalar(field.type_name, width, f"{access}{index_expr}", bits_name, idx_name)
    code += "}\n" * len(loop_vars)
    return code


def struct_bits_expr(fields: Sequence[Field], overrides: Dict[str, int]) -> str:
    parts: List[str] = []
    for f in fields:
        w = type_width_bits(f.type_name, overrides)
        parts.append(f"({w} * {dim_product_expr(f.dims)})")
    if not parts:
        return "0"
    return " + ".join(parts)


def generate_header(
    header_path: str,
    include_path: str,
    ns: str,
    out_ns: str,
    wrapper_type: str,
    pi_fields: Sequence[Tuple[str, str]],
    po_fields: Sequence[Tuple[str, str]],
    type_overrides: Dict[str, int],
) -> str:
    with open(header_path, "r", encoding="utf-8") as f:
        text = strip_comments(f.read())

    struct_fields: Dict[str, List[Field]] = {}
    needed_structs = {s for _, s in list(pi_fields) + list(po_fields)}
    for struct_name in needed_structs:
        body = find_struct_body(text, struct_name)
        struct_fields[struct_name] = parse_fields(body)

    def q(t: str) -> str:
        return cxx_qualified(ns, t)

    lines: List[str] = []
    lines.append("#pragma once\n")
    lines.append("#include <cstddef>\n")
    lines.append("#include <cstdint>\n")
    lines.append(f'#include "{include_path}"\n\n')

    # Open namespace(s)
    if ns:
        lines.append(f"namespace {ns} {{\n")
    lines.append(f"namespace {out_ns} {{\n\n")

    # Per-struct width constants and pack/unpack functions
    for struct_name, fields in struct_fields.items():
        bits_expr = struct_bits_expr(fields, type_overrides)
        lines.append(f"static constexpr size_t {struct_name}_BITS = {bits_expr};\n")
        lines.append(
            f"inline void pack_{struct_name}(const {q(struct_name)} &v, bool *bits, size_t &idx) {{\n"
        )
        for f in fields:
            w = type_width_bits(f.type_name, type_overrides)
            lines.append(gen_pack_field(f, w, "v", "bits", "idx"))
        lines.append("}\n\n")

        lines.append(
            f"inline void unpack_{struct_name}(const bool *bits, size_t &idx, {q(struct_name)} &v) {{\n"
        )
        for f in fields:
            w = type_width_bits(f.type_name, type_overrides)
            lines.append(gen_unpack_field(f, w, "v", "bits", "idx"))
        lines.append("}\n\n")

    # PI/PO widths
    pi_bits_terms = " + ".join(f"{s}_BITS" for _, s in pi_fields) if pi_fields else "0"
    po_bits_terms = " + ".join(f"{s}_BITS" for _, s in po_fields) if po_fields else "0"
    lines.append(f"static constexpr size_t PI_WIDTH = {pi_bits_terms};\n")
    lines.append(f"static constexpr size_t PO_WIDTH = {po_bits_terms};\n\n")

    # Wrapper helpers
    wrapper_q = q(wrapper_type)
    lines.append(f"inline void pack_pi(const {wrapper_q} &io, bool *pi) {{\n")
    lines.append("  size_t idx = 0;\n")
    for field_name, struct_name in pi_fields:
        lines.append(f"  pack_{struct_name}(io.{field_name}, pi, idx);\n")
    lines.append("}\n\n")

    lines.append(f"inline void unpack_pi(const bool *pi, {wrapper_q} &io) {{\n")
    lines.append("  size_t idx = 0;\n")
    for field_name, struct_name in pi_fields:
        lines.append(f"  unpack_{struct_name}(pi, idx, io.{field_name});\n")
    lines.append("}\n\n")

    lines.append(f"inline void pack_po(const {wrapper_q} &io, bool *po) {{\n")
    lines.append("  size_t idx = 0;\n")
    for field_name, struct_name in po_fields:
        lines.append(f"  pack_{struct_name}(io.{field_name}, po, idx);\n")
    lines.append("}\n\n")

    lines.append(f"inline void unpack_po(const bool *po, {wrapper_q} &io) {{\n")
    lines.append("  size_t idx = 0;\n")
    for field_name, struct_name in po_fields:
        lines.append(f"  unpack_{struct_name}(po, idx, io.{field_name});\n")
    lines.append("}\n\n")

    # Directional helpers for common simulation usage:
    # - drive generalized inputs via `unpack_pi(pi, io)` before `comb()`
    # - capture generalized outputs via `pack_po(io, po)` after `comb()`
    lines.append(f"inline void pi_to_inputs(const bool *pi, {wrapper_q} &io) {{\n")
    lines.append("  unpack_pi(pi, io);\n")
    lines.append("}\n\n")

    lines.append(f"inline void outputs_to_po(const {wrapper_q} &io, bool *po) {{\n")
    lines.append("  pack_po(io, po);\n")
    lines.append("}\n\n")

    lines.append("template <typename ModuleT>\n")
    lines.append("inline void eval_comb(ModuleT &module, const bool *pi, bool *po) {\n")
    lines.append("  unpack_pi(pi, module.io);\n")
    lines.append("  module.comb();\n")
    lines.append("  pack_po(module.io, po);\n")
    lines.append("}\n\n")

    # Close namespace(s)
    lines.append(f"}} // namespace {out_ns}\n")
    if ns:
        lines.append(f"}} // namespace {ns}\n")

    return "".join(lines)


def parse_field_spec(specs: Sequence[str]) -> List[Tuple[str, str]]:
    out: List[Tuple[str, str]] = []
    for s in specs:
        if ":" not in s:
            raise ValueError(f"invalid field spec (expected field:Struct): {s}")
        field, struct_name = s.split(":", 1)
        field = field.strip()
        struct_name = struct_name.strip()
        if not field or not struct_name:
            raise ValueError(f"invalid field spec: {s}")
        out.append((field, struct_name))
    return out


def parse_overrides(specs: Sequence[str]) -> Dict[str, int]:
    out: Dict[str, int] = {}
    for s in specs:
        if "=" not in s:
            raise ValueError(f"invalid override (expected Type=Width): {s}")
        t, w = s.split("=", 1)
        t = t.strip()
        w = w.strip()
        out[t] = int(w, 10)
    return out


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(description="Generate bool* pi/po bitvector pack/unpack helpers from C++ structs.")
    ap.add_argument("--header", required=True, help="Path to the C++ header containing the structs.")
    ap.add_argument("--include", required=True, dest="include_path", help="Include path to use in the generated header.")
    ap.add_argument("--namespace", default="", help="C++ namespace that owns the structs (e.g., icache_module_n).")
    ap.add_argument("--out-namespace", default="pi_po", help="Namespace for generated helpers (inside --namespace if provided).")
    ap.add_argument("--wrapper-type", required=True, help="Wrapper struct type that owns the fields (e.g., ICache_IO_t).")
    ap.add_argument("--pi-field", action="append", default=[], help="PI field spec: field:StructName (repeatable).")
    ap.add_argument("--po-field", action="append", default=[], help="PO field spec: field:StructName (repeatable).")
    ap.add_argument("--override", action="append", default=[], help="Type width override: Type=Width (repeatable).")
    ap.add_argument("--output", required=True, help="Output header path.")
    args = ap.parse_args(list(argv))

    pi_fields = parse_field_spec(args.pi_field)
    po_fields = parse_field_spec(args.po_field)
    overrides = parse_overrides(args.override)

    out_text = generate_header(
        header_path=args.header,
        include_path=args.include_path,
        ns=args.namespace,
        out_ns=args.out_namespace,
        wrapper_type=args.wrapper_type,
        pi_fields=pi_fields,
        po_fields=po_fields,
        type_overrides=overrides,
    )

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(out_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
