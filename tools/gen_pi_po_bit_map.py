#!/usr/bin/env python3
from __future__ import annotations

import argparse
import bisect
import csv
import dataclasses
import itertools
import os
import subprocess
import sys
import tempfile
from typing import Dict, Iterable, List, Sequence, Tuple


THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, THIS_DIR)

from gen_pi_po import (  # noqa: E402
    Field,
    find_struct_body,
    parse_field_spec,
    parse_fields,
    parse_overrides,
    strip_comments,
    type_width_bits,
)


@dataclasses.dataclass(frozen=True)
class SignalRange:
    vector_name: str  # "pi" or "po"
    bit_lsb: int
    bit_msb: int
    width: int
    wrapper_field: str
    struct_name: str
    signal: str
    type_name: str


def _ordered_unique_structs(
    pi_fields: Sequence[Tuple[str, str]], po_fields: Sequence[Tuple[str, str]]
) -> List[str]:
    ordered: List[str] = []
    seen = set()
    for _, struct_name in list(pi_fields) + list(po_fields):
        if struct_name in seen:
            continue
        seen.add(struct_name)
        ordered.append(struct_name)
    return ordered


def _load_struct_fields(header_path: str, struct_names: Sequence[str]) -> Dict[str, List[Field]]:
    with open(header_path, "r", encoding="utf-8") as f:
        text = strip_comments(f.read())
    out: Dict[str, List[Field]] = {}
    for struct_name in struct_names:
        body = find_struct_body(text, struct_name)
        out[struct_name] = parse_fields(body)
    return out


def _collect_dim_exprs(struct_fields: Dict[str, List[Field]]) -> List[str]:
    exprs: List[str] = []
    seen = set()
    for fields in struct_fields.values():
        for field in fields:
            for d in field.dims:
                d = d.strip()
                if not d:
                    continue
                if d in seen:
                    continue
                seen.add(d)
                exprs.append(d)
    return exprs


def _cpp_escape(path: str) -> str:
    return path.replace("\\", "\\\\").replace('"', '\\"')


def _eval_exprs_with_cpp(
    exprs: Sequence[str],
    header_path: str,
    namespace: str,
    include_dirs: Sequence[str],
    cxx: str,
    cxxflags: Sequence[str],
) -> Dict[str, int]:
    if not exprs:
        return {}

    with tempfile.TemporaryDirectory(prefix="gen_pi_po_map_") as td:
        src_path = os.path.join(td, "eval_exprs.cpp")
        exe_path = os.path.join(td, "eval_exprs.out")

        lines: List[str] = []
        lines.append("#include <cstdint>\n")
        lines.append("#include <iostream>\n")
        lines.append(f'#include "{_cpp_escape(os.path.abspath(header_path))}"\n')
        lines.append("int main() {\n")
        if namespace:
            lines.append(f"  using namespace {namespace};\n")
        for i, expr in enumerate(exprs):
            lines.append(
                f"  std::cout << {i} << ' ' << static_cast<unsigned long long>(({expr})) << '\\n';\n"
            )
        lines.append("  return 0;\n")
        lines.append("}\n")

        with open(src_path, "w", encoding="utf-8") as f:
            f.write("".join(lines))

        compile_cmd = [cxx, "-std=c++17", "-O0", src_path, "-o", exe_path]
        for inc in include_dirs:
            compile_cmd.extend(["-I", inc])
        compile_cmd.extend(cxxflags)

        comp = subprocess.run(
            compile_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if comp.returncode != 0:
            raise RuntimeError(
                "failed to evaluate array dimensions with C++ compiler:\n"
                + f"command: {' '.join(compile_cmd)}\n"
                + comp.stderr
            )

        run = subprocess.run(
            [exe_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if run.returncode != 0:
            raise RuntimeError(
                "failed to run dimension evaluator binary:\n" + run.stderr
            )

        out: Dict[str, int] = {}
        for line in run.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            idx_str, val_str = line.split(maxsplit=1)
            idx = int(idx_str, 10)
            val = int(val_str, 10)
            if idx < 0 or idx >= len(exprs):
                raise RuntimeError(f"invalid evaluator output index: {idx}")
            out[exprs[idx]] = val
        if len(out) != len(exprs):
            missing = [e for e in exprs if e not in out]
            raise RuntimeError(
                "dimension evaluator produced incomplete output; missing: "
                + ", ".join(missing)
            )
        return out


def _iter_indices(dims: Sequence[int]) -> Iterable[Tuple[int, ...]]:
    if not dims:
        yield ()
        return
    ranges = [range(d) for d in dims]
    for idxs in itertools.product(*ranges):
        yield idxs


def _build_ranges(
    vector_name: str,
    bindings: Sequence[Tuple[str, str]],
    struct_fields: Dict[str, List[Field]],
    type_overrides: Dict[str, int],
    dim_values: Dict[str, int],
) -> Tuple[List[SignalRange], int]:
    ranges: List[SignalRange] = []
    bit_cursor = 0
    for wrapper_field, struct_name in bindings:
        fields = struct_fields[struct_name]
        for field in fields:
            width = type_width_bits(field.type_name, type_overrides)
            dims = [dim_values[d] for d in field.dims]
            for idxs in _iter_indices(dims):
                suffix = "".join(f"[{i}]" for i in idxs)
                signal_name = f"{wrapper_field}.{field.name}{suffix}"
                lsb = bit_cursor
                msb = bit_cursor + width - 1
                ranges.append(
                    SignalRange(
                        vector_name=vector_name,
                        bit_lsb=lsb,
                        bit_msb=msb,
                        width=width,
                        wrapper_field=wrapper_field,
                        struct_name=struct_name,
                        signal=signal_name,
                        type_name=field.type_name,
                    )
                )
                bit_cursor += width
    return ranges, bit_cursor


def _write_range_csv(path: str, rows: Sequence[SignalRange]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "vector",
                "bit_lsb",
                "bit_msb",
                "width",
                "wrapper_field",
                "struct_name",
                "signal",
                "type_name",
            ]
        )
        for row in rows:
            w.writerow(
                [
                    row.vector_name,
                    row.bit_lsb,
                    row.bit_msb,
                    row.width,
                    row.wrapper_field,
                    row.struct_name,
                    row.signal,
                    row.type_name,
                ]
            )


def _write_bit_csv(path: str, rows: Sequence[SignalRange]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "vector",
                "bit",
                "signal",
                "signal_bit",
                "width",
                "wrapper_field",
                "struct_name",
                "type_name",
            ]
        )
        for row in rows:
            for bit in range(row.bit_lsb, row.bit_msb + 1):
                w.writerow(
                    [
                        row.vector_name,
                        bit,
                        row.signal,
                        bit - row.bit_lsb,
                        row.width,
                        row.wrapper_field,
                        row.struct_name,
                        row.type_name,
                    ]
                )


def _find_range(rows: Sequence[SignalRange], bit: int) -> SignalRange | None:
    if bit < 0:
        return None
    starts = [r.bit_lsb for r in rows]
    pos = bisect.bisect_right(starts, bit) - 1
    if pos < 0:
        return None
    candidate = rows[pos]
    if bit > candidate.bit_msb:
        return None
    return candidate


def generate_bit_maps(
    header_path: str,
    namespace: str,
    pi_fields: Sequence[Tuple[str, str]],
    po_fields: Sequence[Tuple[str, str]],
    type_overrides: Dict[str, int],
    out_prefix: str,
    include_dirs: Sequence[str] = (),
    cxx: str = "g++",
    cxxflags: Sequence[str] = (),
    emit_per_bit_csv: bool = False,
) -> Dict[str, object]:
    structs = _ordered_unique_structs(pi_fields, po_fields)
    struct_fields = _load_struct_fields(header_path, structs)
    dim_exprs = _collect_dim_exprs(struct_fields)
    dim_values = _eval_exprs_with_cpp(
        exprs=dim_exprs,
        header_path=header_path,
        namespace=namespace,
        include_dirs=include_dirs,
        cxx=cxx,
        cxxflags=cxxflags,
    )

    pi_ranges, pi_width = _build_ranges(
        vector_name="pi",
        bindings=pi_fields,
        struct_fields=struct_fields,
        type_overrides=type_overrides,
        dim_values=dim_values,
    )
    po_ranges, po_width = _build_ranges(
        vector_name="po",
        bindings=po_fields,
        struct_fields=struct_fields,
        type_overrides=type_overrides,
        dim_values=dim_values,
    )

    pi_map_path = f"{out_prefix}_pi_map.csv"
    po_map_path = f"{out_prefix}_po_map.csv"
    _write_range_csv(pi_map_path, pi_ranges)
    _write_range_csv(po_map_path, po_ranges)

    pi_bits_path = None
    po_bits_path = None
    if emit_per_bit_csv:
        pi_bits_path = f"{out_prefix}_pi_bits.csv"
        po_bits_path = f"{out_prefix}_po_bits.csv"
        _write_bit_csv(pi_bits_path, pi_ranges)
        _write_bit_csv(po_bits_path, po_ranges)

    return {
        "pi_width": pi_width,
        "po_width": po_width,
        "pi_ranges": pi_ranges,
        "po_ranges": po_ranges,
        "pi_map_path": pi_map_path,
        "po_map_path": po_map_path,
        "pi_bits_path": pi_bits_path,
        "po_bits_path": po_bits_path,
    }


def _parse_query(query: str) -> Tuple[str, int]:
    if ":" not in query:
        raise ValueError(f"invalid query `{query}` (expected pi:K or po:K)")
    vec, bit_s = query.split(":", 1)
    vec = vec.strip().lower()
    if vec not in ("pi", "po"):
        raise ValueError(f"invalid query vector `{vec}` in `{query}`")
    bit = int(bit_s.strip(), 10)
    return vec, bit


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(
        description=(
            "Generate generalized-IO PI/PO bit mapping CSV files and support "
            "bit-index to signal lookup."
        )
    )
    ap.add_argument("--header", required=True, help="Path to module header (contains structs).")
    ap.add_argument("--namespace", default="", help="Namespace owning the structs (e.g., icache_module_n).")
    ap.add_argument("--pi-field", action="append", default=[], help="PI field spec: field:StructName (repeatable).")
    ap.add_argument("--po-field", action="append", default=[], help="PO field spec: field:StructName (repeatable).")
    ap.add_argument("--override", action="append", default=[], help="Type width override: Type=Width (repeatable).")
    ap.add_argument("--include-dir", action="append", default=[], help="Additional include dir for expression evaluation.")
    ap.add_argument("--cxx", default="g++", help="C++ compiler used to evaluate array dimensions.")
    ap.add_argument("--cxxflag", action="append", default=[], help="Extra C++ flags for evaluator compile.")
    ap.add_argument(
        "--out-prefix",
        required=True,
        help="Output file prefix. Script writes <prefix>_pi_map.csv and <prefix>_po_map.csv.",
    )
    ap.add_argument(
        "--emit-per-bit-csv",
        action="store_true",
        help="Also emit per-bit CSV: <prefix>_pi_bits.csv and <prefix>_po_bits.csv.",
    )
    ap.add_argument(
        "--query",
        action="append",
        default=[],
        help="Bit query in the form pi:K or po:K (repeatable).",
    )
    args = ap.parse_args(list(argv))

    pi_fields = parse_field_spec(args.pi_field)
    po_fields = parse_field_spec(args.po_field)
    overrides = parse_overrides(args.override)

    result = generate_bit_maps(
        header_path=args.header,
        namespace=args.namespace,
        pi_fields=pi_fields,
        po_fields=po_fields,
        type_overrides=overrides,
        out_prefix=args.out_prefix,
        include_dirs=args.include_dir,
        cxx=args.cxx,
        cxxflags=args.cxxflag,
        emit_per_bit_csv=args.emit_per_bit_csv,
    )

    print(f"PI_WIDTH={result['pi_width']}")
    print(f"PO_WIDTH={result['po_width']}")
    print(result["pi_map_path"])
    print(result["po_map_path"])
    if result["pi_bits_path"]:
        print(result["pi_bits_path"])
    if result["po_bits_path"]:
        print(result["po_bits_path"])

    pi_ranges: Sequence[SignalRange] = result["pi_ranges"]  # type: ignore[assignment]
    po_ranges: Sequence[SignalRange] = result["po_ranges"]  # type: ignore[assignment]
    for q in args.query:
        vec, bit = _parse_query(q)
        ranges = pi_ranges if vec == "pi" else po_ranges
        width = int(result["pi_width"] if vec == "pi" else result["po_width"])
        hit = _find_range(ranges, bit)
        if hit is None:
            print(f"QUERY {vec}:{bit} -> out_of_range (width={width})")
            continue
        print(
            f"QUERY {vec}:{bit} -> {hit.signal}[{bit - hit.bit_lsb}] "
            f"(range={hit.bit_lsb}:{hit.bit_msb}, type={hit.type_name}, width={hit.width})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
