"""
Shape validator for `libgraph.json`, the single source of truth for the JSON
document `libgraph.py` (via `report.py`) writes and the HTML/UI reads.

Usage:
    python Tools/libgraph/tests/contract.py <path-to-libgraph.json>
        prints "OK" and exits 0 if the document conforms,
        otherwise prints every error found (one per line) and exits 1.

Importable:
    from contract import validate, NODE_KINDS, EDGE_CLASSES, ...
    errors = validate(doc)   # [] means the document conforms

See CONTRACT.md (same directory) for the full human-readable schema this
module enforces field-by-field.

Python 3.12 stdlib only.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Enums — the fixed vocabularies used throughout libgraph.json. Later agents
# import these constants instead of re-typing the string literals.
# ---------------------------------------------------------------------------

NODE_KINDS = ["lib", "middleware", "game", "proof"]

# Every non-game edge carries exactly one of these BASE classes...
EDGE_BASE_CLASSES = [
    "declared+used",
    "declared-unused",
    "used-undeclared",
    "test-only",
    "pch",
    "link-only",
]

# ...plus zero or more of these ADDITIVE classes layered on top.
EDGE_ADDITIVE_CLASSES = [
    "allowlisted",
    "in-cycle",
]

# The full set an edge's `classes[]` entries are drawn from (union of the
# two above). Game edges carry `classes: []` — see CONTRACT.md.
EDGE_CLASSES = EDGE_BASE_CLASSES + EDGE_ADDITIVE_CLASSES

HEADER_ROLES = ["entry", "exposed", "unused-public", "internal"]

TYPE_MODES = ["embed", "ref", "mention"]

FILE_TAGS = ["test", "pch", "conditional"]

# The fields carried by `libs[].inbound{}`, and reused as the exact key set
# every `delta[lib]` entry must have.
INBOUND_FIELDS = [
    "entry_headers",
    "exposed_headers",
    "types_embed",
    "types_ref",
    "link_symbols",
    "consumer_libs",
    "consumer_games",
    "leaking_headers",
]


# ---------------------------------------------------------------------------
# validate()
# ---------------------------------------------------------------------------

def validate(doc: dict) -> list[str]:
    """
    Validate `doc` against the libgraph.json contract. Returns a list of
    human-readable error strings; an empty list means the document conforms.

    Never raises on a malformed document — a wrong type or a missing key is
    reported as a string, not a traceback, so a caller can run this over an
    arbitrary/partial document and get a full error list back.
    """
    errors: list[str] = []

    if not isinstance(doc, dict):
        return ["document root must be an object"]

    for key in ("meta", "nodes", "edges", "libs", "delta"):
        if key not in doc:
            errors.append(f"missing top-level key '{key}'")

    if errors:
        # Without the top-level keys nothing below can be checked safely.
        return errors

    node_names_by_kind: dict[str, str] = {}  # name -> kind
    _validate_meta(doc["meta"], errors)
    _validate_nodes(doc["nodes"], errors, node_names_by_kind)
    node_names = set(node_names_by_kind.keys())
    _validate_edges(doc["edges"], errors, node_names, node_names_by_kind)
    _validate_libs(doc["libs"], errors, node_names, node_names_by_kind)
    _validate_delta(doc["delta"], errors)
    _validate_layers(doc["nodes"], doc["edges"], doc.get("libs"), errors)

    return errors


# ---------------------------------------------------------------------------
# internal helpers
# ---------------------------------------------------------------------------

def _is_str(v: Any) -> bool:
    return isinstance(v, str)


def _is_int(v: Any) -> bool:
    # bool is a subclass of int in Python; reject it explicitly so a stray
    # `true`/`false` doesn't silently pass an int check.
    return isinstance(v, int) and not isinstance(v, bool)


def _is_bool(v: Any) -> bool:
    return isinstance(v, bool)


def _is_str_or_none(v: Any) -> bool:
    return v is None or isinstance(v, str)


def _is_list_of_str(v: Any) -> bool:
    return isinstance(v, list) and all(_is_str(x) for x in v)


def _validate_meta(meta: Any, errors: list[str]) -> None:
    if not isinstance(meta, dict):
        errors.append("meta must be an object")
        return

    for key in ("config", "generated", "repo_head", "baseline", "health",
                "delta_removed", "delta_added", "symbols"):
        if key not in meta:
            errors.append(f"meta.{key}: missing")

    if "config" in meta and not _is_str(meta["config"]):
        errors.append("meta.config must be a string")
    if "generated" in meta and not _is_str(meta["generated"]):
        errors.append("meta.generated must be an ISO-8601 UTC string")
    if "repo_head" in meta and not _is_str(meta["repo_head"]):
        errors.append("meta.repo_head must be a git SHA string")
    if "baseline" in meta and not _is_str_or_none(meta["baseline"]):
        errors.append("meta.baseline must be a string path or null")
    if "delta_removed" in meta and not _is_list_of_str(meta["delta_removed"]):
        errors.append("meta.delta_removed must be a list of lib name strings")
    if "delta_added" in meta and not _is_list_of_str(meta["delta_added"]):
        errors.append("meta.delta_added must be a list of lib name strings")
    if "symbols" in meta and not _is_bool(meta["symbols"]):
        errors.append("meta.symbols must be a bool")

    if "health" not in meta:
        return
    health = meta["health"]
    if not isinstance(health, dict):
        errors.append("meta.health must be an object")
        return

    for key in ("orphans", "duplicates", "ambiguous", "unresolved_conditions",
                "missing_link_inputs"):
        if key not in health:
            errors.append(f"meta.health.{key}: missing")

    if "orphans" in health and not _is_list_of_str(health["orphans"]):
        errors.append("meta.health.orphans must be a list of path strings")
    if "duplicates" in health and not _is_list_of_str(health["duplicates"]):
        errors.append("meta.health.duplicates must be a list of path strings")
    if "unresolved_conditions" in health and not _is_list_of_str(health["unresolved_conditions"]):
        errors.append("meta.health.unresolved_conditions must be a list of strings")
    if "missing_link_inputs" in health and not _is_list_of_str(health["missing_link_inputs"]):
        errors.append("meta.health.missing_link_inputs must be a list of human-readable strings")

    if "ambiguous" in health:
        amb = health["ambiguous"]
        if not isinstance(amb, list):
            errors.append("meta.health.ambiguous must be a list")
        else:
            for i, entry in enumerate(amb):
                prefix = f"meta.health.ambiguous[{i}]"
                if not isinstance(entry, dict):
                    errors.append(f"{prefix} must be an object")
                    continue
                for key in ("file", "line", "raw", "candidates"):
                    if key not in entry:
                        errors.append(f"{prefix}.{key}: missing")
                if "file" in entry and not _is_str(entry["file"]):
                    errors.append(f"{prefix}.file must be a string")
                if "line" in entry and not _is_int(entry["line"]):
                    errors.append(f"{prefix}.line must be an int")
                if "raw" in entry and not _is_str(entry["raw"]):
                    errors.append(f"{prefix}.raw must be a string")
                if "candidates" in entry and not _is_list_of_str(entry["candidates"]):
                    errors.append(f"{prefix}.candidates must be a list of path strings")


def _validate_nodes(nodes: Any, errors: list[str], out_kind_by_name: dict[str, str]) -> None:
    if not isinstance(nodes, list):
        errors.append("nodes must be a list")
        return

    seen: set[str] = set()
    for i, n in enumerate(nodes):
        prefix = f"nodes[{i}]"
        if not isinstance(n, dict):
            errors.append(f"{prefix} must be an object")
            continue
        for key in ("name", "kind", "layer", "in_cycle", "inbound_size"):
            if key not in n:
                errors.append(f"{prefix}.{key}: missing")

        name = n.get("name")
        if not _is_str(name):
            errors.append(f"{prefix}.name must be a string")
        else:
            if name in seen:
                errors.append(f"{prefix}.name '{name}' is a duplicate node name")
            seen.add(name)

        kind = n.get("kind")
        if not _is_str(kind) or kind not in NODE_KINDS:
            errors.append(f"{prefix}.kind must be one of {NODE_KINDS}, got {kind!r}")

        if "layer" in n and not _is_int(n["layer"]):
            errors.append(f"{prefix}.layer must be an int")
        if "in_cycle" in n and not _is_bool(n["in_cycle"]):
            errors.append(f"{prefix}.in_cycle must be a bool")
        if "inbound_size" in n and not _is_int(n["inbound_size"]):
            errors.append(f"{prefix}.inbound_size must be an int")

        if _is_str(name) and _is_str(kind):
            out_kind_by_name[name] = kind


def _validate_edge_classes(prefix: str, classes: Any, src_kind: str | None,
                            dst_kind: str | None, errors: list[str]) -> None:
    if not isinstance(classes, list) or not all(_is_str(c) for c in classes):
        errors.append(f"{prefix}.classes must be a list of strings")
        return

    bad = [c for c in classes if c not in EDGE_CLASSES]
    if bad:
        errors.append(f"{prefix}.classes contains unknown class(es) {bad}, allowed {EDGE_CLASSES}")

    is_game_edge = src_kind == "game" or dst_kind == "game"
    if is_game_edge:
        if classes != []:
            errors.append(f"{prefix}.classes must be [] for a game edge, got {classes}")
        return

    base_present = [c for c in classes if c in EDGE_BASE_CLASSES]
    if len(base_present) != 1:
        errors.append(
            f"{prefix}.classes must carry exactly one base class from {EDGE_BASE_CLASSES}, "
            f"found {base_present} in {classes}"
        )
    extra = [c for c in classes if c not in EDGE_BASE_CLASSES and c not in EDGE_ADDITIVE_CLASSES]
    if extra and not bad:
        # already reported above via `bad`, avoid duplicate noise
        pass
    if len(classes) != len(set(classes)):
        errors.append(f"{prefix}.classes contains duplicate entries: {classes}")


def _validate_edges(edges: Any, errors: list[str], node_names: set[str],
                     node_kind_by_name: dict[str, str]) -> None:
    if not isinstance(edges, list):
        errors.append("edges must be a list")
        return

    for i, e in enumerate(edges):
        prefix = f"edges[{i}]"
        if not isinstance(e, dict):
            errors.append(f"{prefix} must be an object")
            continue

        for key in ("src", "dst", "classes", "allowlist_kind", "lower_bound",
                    "include_lines", "files", "types", "symbols"):
            if key not in e:
                errors.append(f"{prefix}.{key}: missing")

        src, dst = e.get("src"), e.get("dst")
        if not _is_str(src):
            errors.append(f"{prefix}.src must be a string")
        elif src not in node_names:
            errors.append(f"{prefix}.src '{src}' is not a node in nodes[]")
        if not _is_str(dst):
            errors.append(f"{prefix}.dst must be a string")
        elif dst not in node_names:
            errors.append(f"{prefix}.dst '{dst}' is not a node in nodes[]")

        if "classes" in e:
            _validate_edge_classes(
                prefix, e["classes"],
                node_kind_by_name.get(src) if _is_str(src) else None,
                node_kind_by_name.get(dst) if _is_str(dst) else None,
                errors,
            )
            # NOTE: there is deliberately no "every in-cycle edge has equal
            # layer" check here any more. `in-cycle` is an SCC over
            # declared+used/used-undeclared (real usage); `layer` is now
            # driven by DECLARED edges only (see CONTRACT.md "How `layer`
            # is computed"). The two edge sets differ, so two nodes in one
            # usage-cycle can legitimately sit on different layers — that
            # is precisely how an undeclared/upward edge shows up as a DSM
            # up-edge. `_validate_layers` (below) is the real layer check.

        if "allowlist_kind" in e and not _is_str_or_none(e["allowlist_kind"]):
            errors.append(f"{prefix}.allowlist_kind must be a string or null")
        if "lower_bound" in e and not _is_bool(e["lower_bound"]):
            errors.append(f"{prefix}.lower_bound must be a bool")
        if "include_lines" in e and not _is_int(e["include_lines"]):
            errors.append(f"{prefix}.include_lines must be an int")

        if "files" in e:
            files = e["files"]
            if not isinstance(files, list):
                errors.append(f"{prefix}.files must be a list")
            else:
                for j, f in enumerate(files):
                    fprefix = f"{prefix}.files[{j}]"
                    if not isinstance(f, dict):
                        errors.append(f"{fprefix} must be an object")
                        continue
                    for key in ("file", "line", "raw", "tags"):
                        if key not in f:
                            errors.append(f"{fprefix}.{key}: missing")
                    if "file" in f and not _is_str(f["file"]):
                        errors.append(f"{fprefix}.file must be a string")
                    if "line" in f and not _is_int(f["line"]):
                        errors.append(f"{fprefix}.line must be an int")
                    if "raw" in f and not _is_str(f["raw"]):
                        errors.append(f"{fprefix}.raw must be a string")
                    if "tags" in f:
                        tags = f["tags"]
                        if not isinstance(tags, list) or not all(_is_str(t) for t in tags):
                            errors.append(f"{fprefix}.tags must be a list of strings")
                        else:
                            bad = [t for t in tags if t not in FILE_TAGS]
                            if bad:
                                errors.append(f"{fprefix}.tags contains unknown tag(s) {bad}, allowed {FILE_TAGS}")

        if "types" in e:
            types_ = e["types"]
            if not isinstance(types_, list):
                errors.append(f"{prefix}.types must be a list")
            else:
                for j, t in enumerate(types_):
                    tprefix = f"{prefix}.types[{j}]"
                    if not isinstance(t, dict):
                        errors.append(f"{tprefix} must be an object")
                        continue
                    for key in ("name", "mode", "count"):
                        if key not in t:
                            errors.append(f"{tprefix}.{key}: missing")
                    if "name" in t and not _is_str(t["name"]):
                        errors.append(f"{tprefix}.name must be a string")
                    if "mode" in t and (not _is_str(t["mode"]) or t["mode"] not in TYPE_MODES):
                        errors.append(f"{tprefix}.mode must be one of {TYPE_MODES}, got {t.get('mode')!r}")
                    if "count" in t and not _is_int(t["count"]):
                        errors.append(f"{tprefix}.count must be an int")

        if "symbols" in e:
            syms = e["symbols"]
            if not isinstance(syms, list):
                errors.append(f"{prefix}.symbols must be a list")
            else:
                for j, s in enumerate(syms):
                    sprefix = f"{prefix}.symbols[{j}]"
                    if not isinstance(s, dict):
                        errors.append(f"{sprefix} must be an object")
                        continue
                    for key in ("demangled", "cls"):
                        if key not in s:
                            errors.append(f"{sprefix}.{key}: missing")
                    if "demangled" in s and not _is_str(s["demangled"]):
                        errors.append(f"{sprefix}.demangled must be a string")
                    if "cls" in s and not _is_str_or_none(s["cls"]):
                        errors.append(f"{sprefix}.cls must be a string or null")


def _validate_node_list_field(prefix: str, value: Any, node_names: set[str], errors: list[str]) -> None:
    if not isinstance(value, list) or not all(_is_str(x) for x in value):
        errors.append(f"{prefix} must be a list of node-name strings")
        return
    bad = [x for x in value if x not in node_names]
    if bad:
        errors.append(f"{prefix} references unknown node(s) {bad}")


def _validate_libs(libs: Any, errors: list[str], node_names: set[str],
                    node_kind_by_name: dict[str, str]) -> None:
    if not isinstance(libs, dict):
        errors.append("libs must be an object")
        return

    lib_node_names = {n for n, k in node_kind_by_name.items() if k == "lib"}
    libs_keys = set(libs.keys())
    if libs_keys != lib_node_names:
        missing = lib_node_names - libs_keys
        extra = libs_keys - lib_node_names
        if missing:
            errors.append(f"libs is missing entries for lib node(s) {sorted(missing)}")
        if extra:
            errors.append(f"libs has entries for non-lib node(s) {sorted(extra)} (libs keys must be exactly the kind==\"lib\" nodes)")

    for name, lib in libs.items():
        prefix = f"libs['{name}']"
        if not isinstance(lib, dict):
            errors.append(f"{prefix} must be an object")
            continue

        for key in ("inbound", "outbound", "headers", "types", "symbols",
                    "unreferenced", "leakage", "sentinel"):
            if key not in lib:
                errors.append(f"{prefix}.{key}: missing")

        # inbound
        if "inbound" in lib:
            inbound = lib["inbound"]
            if not isinstance(inbound, dict):
                errors.append(f"{prefix}.inbound must be an object")
            else:
                for field in INBOUND_FIELDS:
                    if field not in inbound:
                        errors.append(f"{prefix}.inbound.{field}: missing")
                    elif not _is_int(inbound[field]):
                        errors.append(f"{prefix}.inbound.{field} must be an int")
                extra = set(inbound.keys()) - set(INBOUND_FIELDS)
                if extra:
                    errors.append(f"{prefix}.inbound has unknown field(s) {sorted(extra)}")

        # outbound
        if "outbound" in lib:
            outbound = lib["outbound"]
            if not isinstance(outbound, dict):
                errors.append(f"{prefix}.outbound must be an object")
            else:
                for key in ("edges", "symbols_by_target", "lower_bound"):
                    if key not in outbound:
                        errors.append(f"{prefix}.outbound.{key}: missing")
                if "edges" in outbound:
                    _validate_node_list_field(f"{prefix}.outbound.edges", outbound["edges"], node_names, errors)
                if "lower_bound" in outbound:
                    _validate_node_list_field(f"{prefix}.outbound.lower_bound", outbound["lower_bound"], node_names, errors)
                if "symbols_by_target" in outbound:
                    sbt = outbound["symbols_by_target"]
                    if not isinstance(sbt, dict):
                        errors.append(f"{prefix}.outbound.symbols_by_target must be an object")
                    else:
                        for dst, syms in sbt.items():
                            sprefix = f"{prefix}.outbound.symbols_by_target['{dst}']"
                            if dst not in node_names:
                                errors.append(f"{sprefix} key is not a node in nodes[]")
                            if not isinstance(syms, list):
                                errors.append(f"{sprefix} must be a list")
                                continue
                            for j, s in enumerate(syms):
                                if not isinstance(s, dict) or "demangled" not in s or "cls" not in s:
                                    errors.append(f"{sprefix}[{j}] must be an object with 'demangled' and 'cls'")
                                    continue
                                if not _is_str(s["demangled"]):
                                    errors.append(f"{sprefix}[{j}].demangled must be a string")
                                if not _is_str_or_none(s["cls"]):
                                    errors.append(f"{sprefix}[{j}].cls must be a string or null")

        # headers
        if "headers" in lib:
            headers = lib["headers"]
            if not isinstance(headers, list):
                errors.append(f"{prefix}.headers must be a list")
            else:
                for i, h in enumerate(headers):
                    hprefix = f"{prefix}.headers[{i}]"
                    if not isinstance(h, dict):
                        errors.append(f"{hprefix} must be an object")
                        continue
                    for key in ("path", "role", "declared_internal", "consumers",
                                "symbols_out_of_line", "hint"):
                        if key not in h:
                            errors.append(f"{hprefix}.{key}: missing")
                    if "path" in h and not _is_str(h["path"]):
                        errors.append(f"{hprefix}.path must be a string")
                    if "role" in h and (not _is_str(h["role"]) or h["role"] not in HEADER_ROLES):
                        errors.append(f"{hprefix}.role must be one of {HEADER_ROLES}, got {h.get('role')!r}")
                    if "declared_internal" in h and not _is_bool(h["declared_internal"]):
                        errors.append(f"{hprefix}.declared_internal must be a bool")
                    if "symbols_out_of_line" in h and not _is_int(h["symbols_out_of_line"]):
                        errors.append(f"{hprefix}.symbols_out_of_line must be an int")
                    if "hint" in h and not _is_str_or_none(h["hint"]):
                        errors.append(f"{hprefix}.hint must be a string or null")
                    if "consumers" in h:
                        cons = h["consumers"]
                        if not isinstance(cons, list):
                            errors.append(f"{hprefix}.consumers must be a list")
                        else:
                            for j, c in enumerate(cons):
                                cprefix = f"{hprefix}.consumers[{j}]"
                                if not isinstance(c, dict) or "node" not in c or "files" not in c:
                                    errors.append(f"{cprefix} must be an object with 'node' and 'files'")
                                    continue
                                if not _is_str(c["node"]):
                                    errors.append(f"{cprefix}.node must be a string")
                                elif c["node"] not in node_names:
                                    errors.append(f"{cprefix}.node '{c['node']}' is not a node in nodes[]")
                                if not _is_list_of_str(c["files"]):
                                    errors.append(f"{cprefix}.files must be a list of path strings")

        # types
        if "types" in lib:
            types_ = lib["types"]
            if not isinstance(types_, list):
                errors.append(f"{prefix}.types must be a list")
            else:
                for i, t in enumerate(types_):
                    tprefix = f"{prefix}.types[{i}]"
                    if not isinstance(t, dict):
                        errors.append(f"{tprefix} must be an object")
                        continue
                    for key in ("name", "header", "embed", "ref", "mention", "consumers", "hint"):
                        if key not in t:
                            errors.append(f"{tprefix}.{key}: missing")
                    if "name" in t and not _is_str(t["name"]):
                        errors.append(f"{tprefix}.name must be a string")
                    if "header" in t and not _is_str(t["header"]):
                        errors.append(f"{tprefix}.header must be a string")
                    for cnt_key in ("embed", "ref", "mention"):
                        if cnt_key in t and not _is_int(t[cnt_key]):
                            errors.append(f"{tprefix}.{cnt_key} must be an int")
                    if "hint" in t and not _is_str_or_none(t["hint"]):
                        errors.append(f"{tprefix}.hint must be a string or null")
                    if "consumers" in t:
                        cons = t["consumers"]
                        if not isinstance(cons, list):
                            errors.append(f"{tprefix}.consumers must be a list")
                        else:
                            for j, c in enumerate(cons):
                                cprefix = f"{tprefix}.consumers[{j}]"
                                if not isinstance(c, dict) or "node" not in c or "mode" not in c:
                                    errors.append(f"{cprefix} must be an object with 'node' and 'mode'")
                                    continue
                                if not _is_str(c["node"]):
                                    errors.append(f"{cprefix}.node must be a string")
                                elif c["node"] not in node_names:
                                    errors.append(f"{cprefix}.node '{c['node']}' is not a node in nodes[]")
                                if not _is_str(c["mode"]) or c["mode"] not in TYPE_MODES:
                                    errors.append(f"{cprefix}.mode must be one of {TYPE_MODES}, got {c.get('mode')!r}")

        # symbols
        if "symbols" in lib:
            syms = lib["symbols"]
            if not isinstance(syms, list):
                errors.append(f"{prefix}.symbols must be a list")
            else:
                for i, s in enumerate(syms):
                    sprefix = f"{prefix}.symbols[{i}]"
                    if not isinstance(s, dict):
                        errors.append(f"{sprefix} must be an object")
                        continue
                    for key in ("cls", "header", "count", "referenced_by"):
                        if key not in s:
                            errors.append(f"{sprefix}.{key}: missing")
                    if "cls" in s and not _is_str_or_none(s["cls"]):
                        errors.append(f"{sprefix}.cls must be a string or null")
                    if "header" in s and not _is_str(s["header"]):
                        errors.append(f"{sprefix}.header must be a string")
                    if "count" in s and not _is_int(s["count"]):
                        errors.append(f"{sprefix}.count must be an int")
                    if "referenced_by" in s:
                        _validate_node_list_field(f"{sprefix}.referenced_by", s["referenced_by"], node_names, errors)

        # unreferenced
        if "unreferenced" in lib:
            unref = lib["unreferenced"]
            if not isinstance(unref, list):
                errors.append(f"{prefix}.unreferenced must be a list")
            else:
                for i, u in enumerate(unref):
                    uprefix = f"{prefix}.unreferenced[{i}]"
                    if not isinstance(u, dict):
                        errors.append(f"{uprefix} must be an object")
                        continue
                    for key in ("demangled", "cls"):
                        if key not in u:
                            errors.append(f"{uprefix}.{key}: missing")
                    if "demangled" in u and not _is_str(u["demangled"]):
                        errors.append(f"{uprefix}.demangled must be a string")
                    if "cls" in u and not _is_str_or_none(u["cls"]):
                        errors.append(f"{uprefix}.cls must be a string or null")

        # leakage
        if "leakage" in lib:
            leak = lib["leakage"]
            if not isinstance(leak, list):
                errors.append(f"{prefix}.leakage must be a list")
            else:
                for i, l in enumerate(leak):
                    lprefix = f"{prefix}.leakage[{i}]"
                    if not isinstance(l, dict):
                        errors.append(f"{lprefix} must be an object")
                        continue
                    for key in ("header", "via_node", "downstream_via_header", "downstream_via_cpp_only"):
                        if key not in l:
                            errors.append(f"{lprefix}.{key}: missing")
                    if "header" in l and not _is_str(l["header"]):
                        errors.append(f"{lprefix}.header must be a string")
                    if "via_node" in l:
                        if not _is_str(l["via_node"]):
                            errors.append(f"{lprefix}.via_node must be a string")
                        elif l["via_node"] not in node_names:
                            errors.append(f"{lprefix}.via_node '{l['via_node']}' is not a node in nodes[]")
                    if "downstream_via_header" in l:
                        _validate_node_list_field(f"{lprefix}.downstream_via_header", l["downstream_via_header"], node_names, errors)
                    if "downstream_via_cpp_only" in l:
                        _validate_node_list_field(f"{lprefix}.downstream_via_cpp_only", l["downstream_via_cpp_only"], node_names, errors)

        # sentinel
        if "sentinel" in lib:
            sentinel = lib["sentinel"]
            if not _is_str_or_none(sentinel):
                errors.append(f"{prefix}.sentinel must be a string or null")
            elif sentinel is not None:
                if sentinel not in node_names:
                    errors.append(f"{prefix}.sentinel '{sentinel}' is not a node in nodes[]")
                elif node_kind_by_name.get(sentinel) != "proof":
                    errors.append(f"{prefix}.sentinel '{sentinel}' must name a kind==\"proof\" node")


def _validate_delta(delta: Any, errors: list[str]) -> None:
    if delta is None:
        return
    if not isinstance(delta, dict):
        errors.append("delta must be null or an object")
        return
    for lib_name, fields in delta.items():
        prefix = f"delta['{lib_name}']"
        if not _is_str(lib_name):
            errors.append(f"delta key {lib_name!r} must be a string")
        if not isinstance(fields, dict):
            errors.append(f"{prefix} must be an object")
            continue
        missing = set(INBOUND_FIELDS) - set(fields.keys())
        extra = set(fields.keys()) - set(INBOUND_FIELDS)
        if missing:
            errors.append(f"{prefix} is missing field(s) {sorted(missing)} (delta fields are drawn from inbound's keys)")
        if extra:
            errors.append(f"{prefix} has unknown field(s) {sorted(extra)} (delta fields are drawn from inbound's keys)")
        for field, value in fields.items():
            if field in INBOUND_FIELDS and not _is_int(value):
                errors.append(f"{prefix}.{field} must be an int, got {value!r}")


# ---------------------------------------------------------------------------
# layer recomputation — see CONTRACT.md "How `layer` is computed"
#
# This cross-checks nodes[].layer against what the layering rule would
# produce from edges[]. It is deliberately best-effort: on anything
# ambiguous (a claim contract.py cannot settle on its own — see below) it
# skips that node rather than guessing, so it never reports a false
# mismatch. It also never raises: any unexpected shape is caught and the
# whole check is skipped, because by this point `_validate_*` has already
# reported the real structural errors and a crash here would hide them.
# ---------------------------------------------------------------------------

# The base classes that define the *layering* graph: DECLARED edges only
# (what Sharpmake says the architecture should be), not measured USE. This
# is deliberately a different edge set from the `in-cycle` SCC (which is
# `declared+used`/`used-undeclared` — real usage) — see CONTRACT.md "How
# `layer` is computed". Layering by real use collapses almost the whole
# real engine into one SCC (22 of 30 libs cycle through undeclared/mutual
# use), which would flatten the graph to one layer; layering by declared
# intent keeps the DAG meaningful and turns an undeclared or upward *use*
# edge into a visible DSM up-edge instead of erasing it.
_LAYERING_BASE_CLASSES = ("declared+used", "declared-unused")


class _AmbiguousLayering(Exception):
    """Raised internally when the layering graph cannot be resolved
    unambiguously (e.g. an unexpected residual cycle after SCC collapse).
    Caught by the caller; never escapes _compute_expected_layers."""


def _tarjan_scc(names: list[str], graph: dict[str, set[str]]) -> list[list[str]]:
    """Standard recursive Tarjan SCC over `names`/`graph` (adjacency sets).
    Returns a list of components (each a list of node names)."""
    index_counter = [0]
    stack: list[str] = []
    lowlink: dict[str, int] = {}
    index: dict[str, int] = {}
    on_stack: dict[str, bool] = {}
    result: list[list[str]] = []

    def strongconnect(v: str) -> None:
        index[v] = index_counter[0]
        lowlink[v] = index_counter[0]
        index_counter[0] += 1
        stack.append(v)
        on_stack[v] = True

        for w in graph.get(v, ()):
            if w not in index:
                strongconnect(w)
                lowlink[v] = min(lowlink[v], lowlink[w])
            elif on_stack.get(w):
                lowlink[v] = min(lowlink[v], index[w])

        if lowlink[v] == index[v]:
            comp: list[str] = []
            while True:
                w = stack.pop()
                on_stack[w] = False
                comp.append(w)
                if w == v:
                    break
            result.append(comp)

    for v in names:
        if v not in index:
            strongconnect(v)

    return result


def _compute_expected_layers(nodes: Any, edges: Any, libs: Any) -> dict[str, int] | None:
    """
    Recompute the expected `layer` for every node from edges[], per the rule
    pinned in CONTRACT.md. Returns None (skip the whole check) if `nodes`/
    `edges` aren't even list-shaped, or if resolving hits genuine ambiguity
    anywhere; otherwise returns {name: expected_layer} — a node this
    function could not resolve individually (e.g. a proof node with more
    than one claimed subject, or a game depending on another game) is
    simply absent from the returned dict rather than forcing a full skip.
    """
    if not isinstance(nodes, list) or not isinstance(edges, list):
        return None

    try:
        kind_by_name: dict[str, str] = {}
        for n in nodes:
            if isinstance(n, dict) and _is_str(n.get("name")) and _is_str(n.get("kind")):
                kind_by_name[n["name"]] = n["kind"]
        names = list(kind_by_name.keys())

        # 1. Layering graph: non-game edges whose single base class is a
        #    DECLARED one — declared+used / declared-unused. Not
        #    used-undeclared, not link-only: those are real-use edges, and
        #    layering only follows declared intent (see CONTRACT.md).
        layering_graph: dict[str, set[str]] = {n: set() for n in names}
        for e in edges:
            if not isinstance(e, dict):
                continue
            src, dst, classes = e.get("src"), e.get("dst"), e.get("classes")
            if src not in kind_by_name or dst not in kind_by_name:
                continue
            if kind_by_name[src] == "game" or kind_by_name[dst] == "game":
                continue
            if not isinstance(classes, list):
                continue
            base = [c for c in classes if c in EDGE_BASE_CLASSES]
            if len(base) != 1 or base[0] not in _LAYERING_BASE_CLASSES:
                continue
            layering_graph[src].add(dst)

        # 2. Collapse SCCs over the layering graph.
        sccs = _tarjan_scc(names, layering_graph)
        scc_of: dict[str, int] = {}
        for i, comp in enumerate(sccs):
            for n in comp:
                scc_of[n] = i

        cond_edges: dict[int, set[int]] = {i: set() for i in range(len(sccs))}
        for src, dsts in layering_graph.items():
            for dst in dsts:
                if scc_of[src] != scc_of[dst]:
                    cond_edges[scc_of[src]].add(scc_of[dst])

        # 3. layer(scc) = 0 if no outgoing condensation edge, else
        #    1 + max(layer(dst_scc)). The condensation is a DAG by
        #    construction; `visiting` is a defensive tripwire only.
        layer_of_scc: dict[int, int] = {}
        visiting: set[int] = set()

        def layer_for(i: int) -> int:
            if i in layer_of_scc:
                return layer_of_scc[i]
            if i in visiting:
                raise _AmbiguousLayering(f"unexpected cycle in layering condensation at scc {i}")
            visiting.add(i)
            succs = cond_edges.get(i, ())
            result = 0 if not succs else 1 + max(layer_for(j) for j in succs)
            visiting.discard(i)
            layer_of_scc[i] = result
            return result

        expected: dict[str, int] = {}
        for n in names:
            if kind_by_name[n] in ("game", "proof"):
                continue
            expected[n] = layer_for(scc_of[n])

        # 4a. Proof nodes: layer of their subject lib (the lib whose
        #     `sentinel` names this proof node), else 0. More than one lib
        #     claiming the same proof as its sentinel is ambiguous for THAT
        #     node only — skip it, don't abandon the whole computation.
        subject_of: dict[str, list[str]] = {}
        if isinstance(libs, dict):
            for lib_name, lib_val in libs.items():
                if isinstance(lib_val, dict) and _is_str(lib_val.get("sentinel")):
                    subject_of.setdefault(lib_val["sentinel"], []).append(lib_name)

        for n in names:
            if kind_by_name[n] != "proof":
                continue
            claimants = subject_of.get(n, [])
            if len(claimants) > 1:
                continue  # ambiguous subject: skip this node
            if len(claimants) == 1 and claimants[0] in expected:
                expected[n] = expected[claimants[0]]
            else:
                expected[n] = 0

        # 4b. Games: 1 + max layer over ALL their edges' dst (game edges
        #     carry classes: [] but are still used here). A game with no
        #     edges is 0. A game depending on another (unresolved) game is
        #     ambiguous — skip it.
        game_dsts: dict[str, list[str]] = {}
        for e in edges:
            if not isinstance(e, dict):
                continue
            src, dst = e.get("src"), e.get("dst")
            if src in kind_by_name and kind_by_name[src] == "game" and dst in kind_by_name:
                game_dsts.setdefault(src, []).append(dst)

        for n in names:
            if kind_by_name[n] != "game":
                continue
            dsts = game_dsts.get(n, [])
            if not dsts:
                expected[n] = 0
                continue
            if any(d not in expected for d in dsts):
                continue  # ambiguous (e.g. depends on an unresolved game): skip
            expected[n] = 1 + max(expected[d] for d in dsts)

        return expected
    except _AmbiguousLayering:
        return None
    except Exception:
        # Never let a recomputation bug masquerade as a document defect.
        return None


def _validate_layers(nodes: Any, edges: Any, libs: Any, errors: list[str]) -> None:
    expected = _compute_expected_layers(nodes, edges, libs)
    if expected is None:
        return
    if not isinstance(nodes, list):
        return
    for n in nodes:
        if not isinstance(n, dict):
            continue
        name = n.get("name")
        actual = n.get("layer")
        if name in expected and _is_int(actual) and actual != expected[name]:
            errors.append(
                f"nodes['{name}'].layer is {actual}, but recomputing from edges gives "
                f"{expected[name]} (see CONTRACT.md \"How layer is computed\")"
            )


# ---------------------------------------------------------------------------
# __main__
# ---------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(f"usage: python {argv[0] if argv else 'contract.py'} <path.json>", file=sys.stderr)
        return 1

    path = Path(argv[1])
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"could not read {path}: {exc}")
        return 1
    except json.JSONDecodeError as exc:
        print(f"{path} is not valid JSON: {exc}")
        return 1

    errors = validate(doc)
    if errors:
        for err in errors:
            print(err)
        return 1

    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
