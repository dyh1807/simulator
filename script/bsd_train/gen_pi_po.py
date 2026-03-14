#!/usr/bin/env python3
from __future__ import annotations

import argparse
import dataclasses
import os
import re
import sys
from textwrap import indent
from typing import Dict, List, Sequence, Tuple

TYPE_WIDTH_DEFAULT: Dict[str, int] = {
    'bool': 1,
    'uint8': 8,
    'uint8_t': 8,
    'uint16': 16,
    'uint16_t': 16,
    'uint32': 32,
    'uint32_t': 32,
    'uint64': 64,
    'uint64_t': 64,
}


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*?$', '', text, flags=re.MULTILINE)
    return text


def find_struct_body(text: str, struct_name: str) -> str:
    m = re.search(rf'\bstruct\s+{re.escape(struct_name)}\s*\{{', text)
    if not m:
        raise ValueError(f'struct not found: {struct_name}')
    brace_start = m.end() - 1
    depth = 0
    for i in range(brace_start, len(text)):
        ch = text[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return text[brace_start + 1:i]
    raise ValueError(f'unterminated struct body: {struct_name}')


@dataclasses.dataclass(frozen=True)
class Field:
    type_name: str
    name: str
    dims: Tuple[str, ...]


def parse_fields(struct_body: str) -> List[Field]:
    fields: List[Field] = []
    for stmt in struct_body.split(';'):
        s = stmt.strip()
        if not s:
            continue
        if s.startswith(('struct ', 'enum ', 'using ', 'public:', 'private:', 'protected:')):
            continue
        if '=' in s:
            s = s.split('=', 1)[0].strip()
        if not s:
            continue
        if '(' in s or ')' in s:
            continue
        m = re.match(
            r'^(?P<type>[A-Za-z_]\w*(?:<[^>]+>)?(?:::\w+(?:<[^>]+>)?)*)\s+'
            r'(?P<name>[A-Za-z_]\w*)\s*(?P<arrays>(?:\[[^\]]+\])*)$',
            s,
        )
        if not m:
            raise ValueError(f'cannot parse field declaration: `{stmt.strip()};`')
        dims = tuple(d.strip() for d in re.findall(r'\[([^\]]+)\]', m.group('arrays')))
        fields.append(Field(m.group('type'), m.group('name'), dims))
    return fields


def type_width_expr(type_name: str, overrides: Dict[str, int]) -> str:
    if type_name in overrides:
        return str(overrides[type_name])
    if type_name in TYPE_WIDTH_DEFAULT:
        return str(TYPE_WIDTH_DEFAULT[type_name])
    m = re.match(r'^(?:wire|reg)<([^>]+)>$', type_name)
    if m:
        return m.group(1).strip()
    m = re.match(r'^(?:wire|reg)(\d+)_t$', type_name)
    if m:
        return m.group(1)
    raise ValueError(f'unsupported type width for `{type_name}`')


def cxx_qualified(ns: str, type_name: str) -> str:
    if not ns or '::' in type_name:
        return type_name
    return f'{ns}::{type_name}'


def dim_product_expr(dims: Sequence[str]) -> str:
    if not dims:
        return '1'
    return ' * '.join(f'({d})' for d in dims)


def gen_pack_scalar(type_name: str, width: int, value_expr: str, bits_name: str, idx_name: str) -> str:
    if str(width) == '1' and type_name == 'bool':
        return f'{bits_name}[{idx_name}++] = {value_expr};\n'
    return (
        f'for (size_t b = 0; b < {width}; ++b) {{\n'
        f'  {bits_name}[{idx_name}++] = ((static_cast<uint64_t>({value_expr}) >> b) & 1u) != 0;\n'
        f'}}\n'
    )


def gen_unpack_scalar(type_name: str, width: int, dst_expr: str, bits_name: str, idx_name: str) -> str:
    if str(width) == '1' and type_name == 'bool':
        return f'{dst_expr} = {bits_name}[{idx_name}++];\n'
    return (
        '{\n'
        '  uint64_t tmp = 0;\n'
        f'  for (size_t b = 0; b < {width}; ++b) {{\n'
        f'    if ({bits_name}[{idx_name}++]) {{\n'
        '      tmp |= (uint64_t(1) << b);\n'
        '    }\n'
        '  }\n'
        f'  {dst_expr} = static_cast<{type_name}>(tmp);\n'
        '}\n'
    )


def gen_pack_field(field: Field, width: int, obj_expr: str, bits_name: str, idx_name: str) -> str:
    access = f'{obj_expr}.{field.name}'
    if not field.dims:
        return gen_pack_scalar(field.type_name, width, access, bits_name, idx_name)
    def rec(level: int, suffix: str) -> str:
        if level == len(field.dims):
            return gen_pack_scalar(field.type_name, width, f'{access}{suffix}', bits_name, idx_name)
        lv = f'i{level}'
        inner = indent(rec(level + 1, f'{suffix}[{lv}]'), '  ')
        return f'for (size_t {lv} = 0; {lv} < ({field.dims[level]}); ++{lv}) {{\n{inner}}}\n'
    return rec(0, '')


def gen_unpack_field(field: Field, width: int, obj_expr: str, bits_name: str, idx_name: str) -> str:
    access = f'{obj_expr}.{field.name}'
    if not field.dims:
        return gen_unpack_scalar(field.type_name, width, access, bits_name, idx_name)
    def rec(level: int, suffix: str) -> str:
        if level == len(field.dims):
            return gen_unpack_scalar(field.type_name, width, f'{access}{suffix}', bits_name, idx_name)
        lv = f'i{level}'
        inner = indent(rec(level + 1, f'{suffix}[{lv}]'), '  ')
        return f'for (size_t {lv} = 0; {lv} < ({field.dims[level]}); ++{lv}) {{\n{inner}}}\n'
    return rec(0, '')


def struct_bits_expr(fields: Sequence[Field], overrides: Dict[str, int]) -> str:
    parts: List[str] = []
    for f in fields:
        parts.append(f'({type_width_expr(f.type_name, overrides)} * {dim_product_expr(f.dims)})')
    return ' + '.join(parts) if parts else '0'


def parse_field_spec(specs: Sequence[str]) -> List[Tuple[str, str]]:
    out: List[Tuple[str, str]] = []
    for s in specs:
        if ':' not in s:
            raise ValueError(f'invalid field spec: {s}')
        field, struct_name = s.split(':', 1)
        out.append((field.strip(), struct_name.strip()))
    return out


def parse_overrides(specs: Sequence[str]) -> Dict[str, int]:
    out: Dict[str, int] = {}
    for s in specs:
        if '=' not in s:
            raise ValueError(f'invalid override: {s}')
        k, v = s.split('=', 1)
        out[k.strip()] = int(v.strip(), 10)
    return out


def generate_header(header_path: str, include_path: str, ns: str, out_ns: str, wrapper_type: str,
                    pi_fields: Sequence[Tuple[str, str]], po_fields: Sequence[Tuple[str, str]],
                    type_overrides: Dict[str, int]) -> str:
    with open(header_path, 'r', encoding='utf-8') as f:
        text = strip_comments(f.read())

    ordered_structs: List[str] = []
    seen = set()
    for _, struct_name in list(pi_fields) + list(po_fields):
        if struct_name not in seen:
            seen.add(struct_name)
            ordered_structs.append(struct_name)

    struct_fields: Dict[str, List[Field]] = {}
    for struct_name in ordered_structs:
        struct_fields[struct_name] = parse_fields(find_struct_body(text, struct_name))

    def q(t: str) -> str:
        return cxx_qualified(ns, t)

    lines: List[str] = []
    lines.append('#pragma once\n')
    lines.append('#include <cstddef>\n#include <cstdint>\n')
    lines.append(f'#include "{include_path}"\n\n')
    if ns:
        lines.append(f'namespace {ns} {{\n')
    lines.append(f'namespace {out_ns} {{\n\n')

    for struct_name in ordered_structs:
        fields = struct_fields[struct_name]
        lines.append(f'static constexpr size_t {struct_name}_BITS =\n')
        lines.append(f'    {struct_bits_expr(fields, type_overrides)};\n')
        lines.append(f'inline void pack_{struct_name}(const {q(struct_name)} &v, bool *bits, size_t &idx) {{\n')
        for f in fields:
            lines.append(indent(gen_pack_field(f, type_width_expr(f.type_name, type_overrides), 'v', 'bits', 'idx'), '  '))
        lines.append('}\n\n')
        lines.append(f'inline void unpack_{struct_name}(const bool *bits, size_t &idx, {q(struct_name)} &v) {{\n')
        for f in fields:
            lines.append(indent(gen_unpack_field(f, type_width_expr(f.type_name, type_overrides), 'v', 'bits', 'idx'), '  '))
        lines.append('}\n\n')

    pi_bits_terms = ' + '.join(f'{s}_BITS' for _, s in pi_fields) if pi_fields else '0'
    po_bits_terms = ' + '.join(f'{s}_BITS' for _, s in po_fields) if po_fields else '0'
    wrapper_q = q(wrapper_type)
    lines.append(f'static constexpr size_t PI_WIDTH = {pi_bits_terms};\n')
    lines.append(f'static constexpr size_t PO_WIDTH = {po_bits_terms};\n\n')
    lines.append(f'inline void pack_pi(const {wrapper_q} &io, bool *pi) {{\n  size_t idx = 0;\n')
    for field_name, struct_name in pi_fields:
        lines.append(f'  pack_{struct_name}(io.{field_name}, pi, idx);\n')
    lines.append('}\n\n')
    lines.append(f'inline void unpack_pi(const bool *pi, {wrapper_q} &io) {{\n  size_t idx = 0;\n')
    for field_name, struct_name in pi_fields:
        lines.append(f'  unpack_{struct_name}(pi, idx, io.{field_name});\n')
    lines.append('}\n\n')
    lines.append(f'inline void pack_po(const {wrapper_q} &io, bool *po) {{\n  size_t idx = 0;\n')
    for field_name, struct_name in po_fields:
        lines.append(f'  pack_{struct_name}(io.{field_name}, po, idx);\n')
    lines.append('}\n\n')
    lines.append(f'inline void unpack_po(const bool *po, {wrapper_q} &io) {{\n  size_t idx = 0;\n')
    for field_name, struct_name in po_fields:
        lines.append(f'  unpack_{struct_name}(po, idx, io.{field_name});\n')
    lines.append('}\n\n')
    lines.append('template <typename ModuleT>\ninline void eval_comb(ModuleT &module, const bool *pi, bool *po) {\n')
    lines.append('  unpack_pi(pi, module.io);\n  module.comb();\n  pack_po(module.io, po);\n}\n\n')
    lines.append(f'}} // namespace {out_ns}\n')
    if ns:
        lines.append(f'}} // namespace {ns}\n')
    return ''.join(lines)


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(description='Generate bool* pi/po helpers from C++ structs.')
    ap.add_argument('--header', required=True)
    ap.add_argument('--include', required=True, dest='include_path')
    ap.add_argument('--namespace', default='')
    ap.add_argument('--out-namespace', default='pi_po')
    ap.add_argument('--wrapper-type', required=True)
    ap.add_argument('--pi-field', action='append', default=[])
    ap.add_argument('--po-field', action='append', default=[])
    ap.add_argument('--override', action='append', default=[])
    ap.add_argument('--output', required=True)
    args = ap.parse_args(list(argv))
    text = generate_header(
        header_path=args.header,
        include_path=args.include_path,
        ns=args.namespace,
        out_ns=args.out_namespace,
        wrapper_type=args.wrapper_type,
        pi_fields=parse_field_spec(args.pi_field),
        po_fields=parse_field_spec(args.po_field),
        type_overrides=parse_overrides(args.override),
    )
    os.makedirs(os.path.dirname(args.output) or '.', exist_ok=True)
    with open(args.output, 'w', encoding='utf-8') as f:
        f.write(text)
    print(args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
