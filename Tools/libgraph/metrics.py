"""libgraph's core analysis stage: turns a Model + IncludeGraph + TypeIndex
(+ optional SymbolTable) into the `libgraph.json` document described by
`tests/CONTRACT.md` ("the binding output contract") and enforced by
`tests/contract.py`.

Run as a script from the repo root for the DoD real-repo report:
    python Tools/libgraph/metrics.py --config <cfg> [--dumpbin-dir <dir>] [--no-symbols]

Python 3.12 stdlib only. Flat sibling module: no package, no relative import.

See the approved plan (C:\\Users\\tomos\\.claude\\plans\\i-want-a-way-quirky-mountain.md),
pipeline step 5, and the W3 brief's "Binding amendments from the critic
pass", which is the final word whenever it disagrees with anything else
(including, in one case -- inbound.link_symbols -- CONTRACT.md's own
inbound-field table; see the note above `_INBOUND_LINK_SYMBOLS_IS_COUNT`
below for exactly which fields that applies to).

Documented, deliberate simplifications (mirroring the "known blind spots"
style of includes.py/cxxtypes.py):

  - **Sentinel absence.** On the real repo the Sentinel* proof projects are
    `ToolsEnabled=False`-only, so they are ABSENT (`model.absent_nodes`)
    from every `*_win64_true` config. That is expected, not an error: a
    lib whose sentinel is absent for this config simply gets
    `libs[L].sentinel = None` and `outbound.lower_bound = []`.
  - **Edge-level `types[]` mode/count.** Per edge (src, dst), a type used
    by more than one src-owned consumer file, with a different strongest
    mode in each file, is reported with ONE combined entry: `mode` is the
    strongest mode seen across every consumer file, `count` is the number
    of distinct consumer files that touch it at all (not just the ones at
    the strongest mode). This mirrors the per-lib `types[].consumers[]`
    "one entry per (node, mode) pair actually observed" rule at the file
    granularity rather than the node granularity, since an edge has no
    per-mode breakdown field of its own (unlike `libs[L].types[]`, which
    has separate `embed`/`ref`/`mention` counts).
  - **`headers[].role` "exposed" BFS** skips only lines whose `dst_file`
    is literally `model.pch_root` (the same rule used for base-class
    classification throughout this module), not every line
    `includes.py` happens to tag `pch` (which also covers lines merely
    INSIDE the PCH closure -- see the module-level note in includes.py
    and CONTRACT.md's "Include-line categories"). A `*.Tests.inl` link in
    that BFS is not specially excluded either (untested by any fixture
    scenario; the real repo's ECS `Internal/` headers do not form such a
    chain).
  - **`symbols_out_of_line`** is keyed by `cls` (binding amendment 8),
    the same key `libs[L].symbols[]` groups by -- NOT by scanning
    `Symbol.scopes` for a `TypeDecl` name. A free function (`cls is
    None`) is therefore never counted against any header, exactly as the
    brief's original (pre-amendment) text already said.
"""

from __future__ import annotations

import argparse
import datetime
import sys
import time
from collections import defaultdict
from pathlib import Path
from typing import Any

import cxxtypes as cxxtypes_mod
import includes as includes_mod
import model as model_mod
import symbols as symbols_mod

_HEADER_EXTS = {".h", ".hpp", ".inl"}

# Mirrors tests/contract.py's INBOUND_FIELDS list, in the same order --
# duplicated here (not imported) because production code must not import
# from tests/: `from contract import INBOUND_FIELDS` inside compute() used
# to raise ModuleNotFoundError for any --baseline run outside unittest,
# since tests/ is never on sys.path for a plain `python metrics.py` or
# `python libgraph.py` invocation. tests/test_metrics.py asserts these two
# tuples/lists can't drift (`metrics.INBOUND_FIELDS ==
# tuple(contract.INBOUND_FIELDS)`).
INBOUND_FIELDS: tuple[str, ...] = (
    "entry_headers",
    "exposed_headers",
    "types_embed",
    "types_ref",
    "link_symbols",
    "consumer_libs",
    "consumer_games",
    "leaking_headers",
)

# The INBOUND_FIELDS whose formula CONTRACT.md's own inbound-field table
# gives directly (a plain SUM across libs[L].types[]): types_embed,
# types_ref. `link_symbols` looks like it should be
# "sum of count across symbols[]" per that same table, but the W3 brief's
# binding amendment 6 explicitly overrides that one field: "inbound.
# link_symbols = the number of distinct externally referenced owned
# symbols (not the sum)." No amendment touches types_embed/types_ref, so
# those keep CONTRACT.md's sum-of-types[] formula unmodified. This
# constant exists purely so that fact is written down once, near the code
# that implements it, rather than only in this docstring.
_INBOUND_LINK_SYMBOLS_IS_COUNT_NOT_SUM = True


# ---------------------------------------------------------------------------
# Small generic helpers.
# ---------------------------------------------------------------------------


def _norm(raw: str) -> str:
    return raw.replace("\\", "/")


def _tarjan_scc(names: list[str], graph: dict[str, set[str]]) -> list[list[str]]:
    """Standard recursive Tarjan SCC. Returns a list of components (each a
    list of node names), same shape as tests/contract.py's own copy (kept
    independent -- this module and the validator must not import each
    other)."""
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


# ---------------------------------------------------------------------------
# Node selection.
# ---------------------------------------------------------------------------


def _select_nodes(model: "model_mod.Model", include_games: bool) -> dict[str, "model_mod.Node"]:
    absent = set(model.absent_nodes)
    out: dict[str, model_mod.Node] = {}
    for name, node in model.nodes.items():
        if name in absent:
            continue
        if not include_games and node.kind == "game":
            continue
        out[name] = node
    return out


# ---------------------------------------------------------------------------
# Declared-edge index: per (src, dst) pair, whether a DIRECT (non-aggregate)
# declared edge exists, and whether an AGGREGATE (via_aggregate) declared
# edge exists -- kept separate because of the via_aggregate refinement
# (binding amendment 1): an aggregate declaration counts as "declared" for
# classification purposes only when the edge also carries >=1 normal
# include line.
# ---------------------------------------------------------------------------


class _DeclaredIndex:
    def __init__(self, model: "model_mod.Model", nodes: dict[str, "model_mod.Node"]) -> None:
        self.direct: set[tuple[str, str]] = set()
        self.aggregate: set[tuple[str, str]] = set()
        # Sentinel (proof node) -> ordered list of (order, dst) for its own
        # UNCONDITIONAL (conditional is None), non-aggregate declared edges
        # -- the Sentinel lower-bound source (binding amendment 9).
        self.sentinel_unconditional: dict[str, list[tuple[int, str]]] = defaultdict(list)

        for d in model.declared:
            if d.src not in nodes or d.dst not in nodes:
                continue
            pair = (d.src, d.dst)
            if d.via_aggregate:
                self.aggregate.add(pair)
            else:
                self.direct.add(pair)
                if d.conditional is None and nodes[d.src].kind == "proof":
                    self.sentinel_unconditional[d.src].append((d.order, d.dst))

        for src in self.sentinel_unconditional:
            self.sentinel_unconditional[src].sort(key=lambda t: t[0])


# ---------------------------------------------------------------------------
# Symbol index: mangled -> unambiguous owner, and mangled -> set of
# referencing (undef) nodes. Built once, not via SymbolTable.owner_of()
# (which is O(nodes) per call) -- this module may process a real-repo
# symbol table with tens of thousands of owned/undef entries.
# ---------------------------------------------------------------------------


class _SymbolIndex:
    owner_of: dict[str, str]
    referencers_of: dict[str, set[str]]
    owned_by_node: dict[str, dict[str, "symbols_mod.Symbol"]]

    def __init__(self, symbols: "symbols_mod.SymbolTable | None", nodes: dict[str, "model_mod.Node"]) -> None:
        self.owner_of = {}
        self.referencers_of = defaultdict(set)
        self.owned_by_node = {}

        if symbols is None:
            return

        owner_lists: dict[str, list[str]] = defaultdict(list)
        for node_name, syms in symbols.owned.items():
            if node_name not in nodes:
                continue
            self.owned_by_node[node_name] = syms
            for mangled in syms:
                owner_lists[mangled].append(node_name)
        for mangled, owners in owner_lists.items():
            if len(owners) == 1:
                self.owner_of[mangled] = owners[0]

        for node_name, undefs in symbols.undef.items():
            if node_name not in nodes:
                continue
            for mangled in undefs:
                owner = self.owner_of.get(mangled)
                if owner is None or owner == node_name:
                    continue
                self.referencers_of[mangled].add(node_name)

    def symbol(self, owner: str, mangled: str) -> "symbols_mod.Symbol | None":
        return self.owned_by_node.get(owner, {}).get(mangled)


# ---------------------------------------------------------------------------
# Include-line categorisation for BASE-CLASS purposes (distinct from
# includes.py's own broader `pch` tag -- see CONTRACT.md "Include-line
# categories" and the module docstring).
# ---------------------------------------------------------------------------


def _line_category(e: "includes_mod.IncludeEdge", pch_root: str | None) -> str:
    if pch_root is not None and e.dst_file == pch_root:
        return "pch"
    if "test" in e.tags:
        return "test"
    return "normal"


# ---------------------------------------------------------------------------
# Edge aggregation: per (src_owner, dst_owner), collect every include line
# (any category) plus every cross-node link symbol.
# ---------------------------------------------------------------------------


class _PairData:
    __slots__ = ("lines", "symbols", "has_normal", "has_test", "has_pch")

    def __init__(self) -> None:
        self.lines: list[tuple["includes_mod.IncludeEdge", str]] = []  # (edge, category)
        self.symbols: list[tuple[str, "symbols_mod.Symbol"]] = []  # (mangled, Symbol)
        self.has_normal = False
        self.has_test = False
        self.has_pch = False


def _add_symbol_pairs(
    pairs: dict[tuple[str, str], _PairData],
    symbols: "symbols_mod.SymbolTable | None",
    nodes: dict[str, "model_mod.Node"],
    sym_index: _SymbolIndex,
) -> None:
    if symbols is None:
        return
    for src_name, undefs in symbols.undef.items():
        if src_name not in nodes:
            continue
        for mangled in sorted(undefs):
            owner = sym_index.owner_of.get(mangled)
            if owner is None or owner == src_name or owner not in nodes:
                continue
            sym = sym_index.symbol(owner, mangled)
            if sym is None or sym.generated:
                continue
            pairs[(src_name, owner)].symbols.append((mangled, sym))


# ---------------------------------------------------------------------------
# Base-class + allowlist classification for one non-game (src, dst) pair.
# ---------------------------------------------------------------------------


def _classify_base(pd: "_PairData", declared: "_DeclaredIndex", src: str, dst: str) -> str | None:
    is_declared = (src, dst) in declared.direct or (
        (src, dst) in declared.aggregate and pd.has_normal
    )
    has_use = pd.has_normal or bool(pd.symbols)

    if is_declared:
        return "declared+used" if has_use else "declared-unused"
    if pd.has_normal:
        return "used-undeclared"
    if pd.has_test:
        return "test-only"
    if pd.has_pch:
        return "pch"
    if pd.symbols:
        return "link-only"
    return None


def _allowlist_match(pd: "_PairData", allowlist: dict[str, str | None]) -> tuple[bool, str | None]:
    matches: list[tuple[str, int, str | None]] = []
    for e, _cat in pd.lines:
        key = f"{e.src_file} => {_norm(e.raw)}"
        if key in allowlist:
            matches.append((e.src_file, e.line, allowlist[key]))
    if not matches:
        return False, None
    matches.sort(key=lambda t: (t[0], t[1]))
    return True, matches[0][2]


# ---------------------------------------------------------------------------
# in-cycle (real-use SCC) and layer (declared-intent SCC/condensation).
# ---------------------------------------------------------------------------

_IN_CYCLE_BASES = ("declared+used", "used-undeclared")
_LAYERING_BASES = ("declared+used", "declared-unused")


def _compute_in_cycle(
    names: list[str], base_by_pair: dict[tuple[str, str], str]
) -> set[str]:
    graph: dict[str, set[str]] = {n: set() for n in names}
    for (src, dst), base in base_by_pair.items():
        if base in _IN_CYCLE_BASES:
            graph[src].add(dst)
    sccs = _tarjan_scc(names, graph)
    in_cycle: set[str] = set()
    for comp in sccs:
        if len(comp) > 1:
            in_cycle.update(comp)
    return in_cycle


def _compute_layers(
    names: list[str],
    kind_of: dict[str, str],
    base_by_pair: dict[tuple[str, str], str],
    game_dsts: dict[str, list[str]],
    sentinel_subject: dict[str, str],
) -> dict[str, int]:
    layering_graph: dict[str, set[str]] = {n: set() for n in names}
    for (src, dst), base in base_by_pair.items():
        if kind_of.get(src) == "game" or kind_of.get(dst) == "game":
            continue
        if base in _LAYERING_BASES:
            layering_graph[src].add(dst)

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

    layer_of_scc: dict[int, int] = {}

    def layer_for(i: int) -> int:
        if i in layer_of_scc:
            return layer_of_scc[i]
        succs = cond_edges.get(i, ())
        result = 0 if not succs else 1 + max(layer_for(j) for j in succs)
        layer_of_scc[i] = result
        return result

    layer: dict[str, int] = {}
    for n in names:
        if kind_of.get(n) in ("game", "proof"):
            continue
        layer[n] = layer_for(scc_of[n])

    for n in names:
        if kind_of.get(n) != "proof":
            continue
        subject = sentinel_subject.get(n)
        layer[n] = layer.get(subject, 0) if subject is not None else 0

    # Games: 1 + max(layer(dst)) over EVERY edge whose src is this game
    # (any base -- game edges carry classes: [] but still count). Iterate
    # to a fixed point in case one game's edges reach another game's own
    # dst set (unlikely in practice, harmless if it doesn't converge in
    # one pass since game->game edges are vanishingly rare and each pass
    # only ever raises a value).
    for n in names:
        if kind_of.get(n) != "game":
            continue
        layer[n] = 0

    changed = True
    guard = 0
    while changed and guard < len(names) + 2:
        changed = False
        guard += 1
        for n in names:
            if kind_of.get(n) != "game":
                continue
            dsts = game_dsts.get(n, [])
            if not dsts:
                continue
            val = 1 + max(layer.get(d, 0) for d in dsts)
            if val != layer[n]:
                layer[n] = val
                changed = True

    return layer


# ---------------------------------------------------------------------------
# Edge-level types[] (per (src,dst) pair).
# ---------------------------------------------------------------------------

_MODE_RANK = {"embed": 2, "ref": 1, "mention": 0}


def _edge_types(
    types: "cxxtypes_mod.TypeIndex",
    files_owned_by: dict[str, str],
    src: str,
    dst: str,
) -> list[dict]:
    per_type: dict[str, dict[str, Any]] = {}
    for u in types.uses:
        owner = files_owned_by.get(u.consumer_file)
        if owner != src:
            continue
        decl = types.decls.get(u.type)
        if decl is None or decl.node != dst:
            continue
        entry = per_type.setdefault(u.type, {"files": set(), "best": u.mode})
        entry["files"].add(u.consumer_file)
        if _MODE_RANK[u.mode] > _MODE_RANK[entry["best"]]:
            entry["best"] = u.mode

    out = []
    for name in sorted(per_type):
        e = per_type[name]
        out.append({"name": name, "mode": e["best"], "count": len(e["files"])})
    return out


# ---------------------------------------------------------------------------
# compute()
# ---------------------------------------------------------------------------


def compute(
    model: "model_mod.Model",
    graph: "includes_mod.IncludeGraph",
    types: "cxxtypes_mod.TypeIndex",
    symbols: "symbols_mod.SymbolTable | None" = None,
    *,
    baseline: dict | None = None,
    baseline_path: str | None = None,
    include_games: bool = True,
    repo_head: str = "",
    generated: str | None = None,
) -> dict:
    nodes = _select_nodes(model, include_games)
    node_names = sorted(nodes)
    kind_of = {n: v.kind for n, v in nodes.items()}
    lib_names = sorted(n for n in node_names if kind_of[n] == "lib")
    files_owned_by = {p: fo.node for p, fo in model.files.items()}

    declared = _DeclaredIndex(model, nodes)
    sym_index = _SymbolIndex(symbols, nodes)

    # -- Aggregate every (src_owner, dst_owner) pair's lines -----------------
    pairs: dict[tuple[str, str], _PairData] = defaultdict(_PairData)
    pch_root = model.pch_root
    for e in graph.edges:
        src_fo = model.files.get(e.src_file)
        dst_fo = model.files.get(e.dst_file)
        if src_fo is None or dst_fo is None:
            continue
        src_owner, dst_owner = src_fo.node, dst_fo.node
        if src_owner == dst_owner or src_owner not in nodes or dst_owner not in nodes:
            continue
        cat = _line_category(e, pch_root)
        pd = pairs[(src_owner, dst_owner)]
        pd.lines.append((e, cat))
        if cat == "normal":
            pd.has_normal = True
        elif cat == "test":
            pd.has_test = True
        else:
            pd.has_pch = True

    _add_symbol_pairs(pairs, symbols, nodes, sym_index)

    # Zero-evidence declared-unused candidates (scenario 12): a DIRECT
    # declared edge between two non-game nodes with no line and no symbol
    # at all still needs a `declared-unused` row (CONTRACT's "Empty
    # edges" rule). An aggregate-only declaration with nothing else is
    # deliberately dropped (binding amendment 1) -- never synthesised
    # here.
    for (src, dst) in declared.direct:
        if kind_of.get(src) == "game" or kind_of.get(dst) == "game":
            continue
        _ = pairs[(src, dst)]  # touch to materialise via defaultdict

    # -- Classify each pair ---------------------------------------------------
    base_by_pair: dict[tuple[str, str], str] = {}
    allowlisted_by_pair: dict[tuple[str, str], tuple[bool, str | None]] = {}
    for (src, dst), pd in list(pairs.items()):
        is_game_edge = kind_of.get(src) == "game" or kind_of.get(dst) == "game"
        allowlisted_by_pair[(src, dst)] = _allowlist_match(pd, model.allowlist)
        if is_game_edge:
            continue
        base = _classify_base(pd, declared, src, dst)
        if base is None:
            del pairs[(src, dst)]
            continue
        base_by_pair[(src, dst)] = base

    # -- in-cycle / layers ------------------------------------------------
    in_cycle_nodes = _compute_in_cycle(node_names, base_by_pair)

    game_dsts: dict[str, list[str]] = defaultdict(list)
    for (src, dst) in pairs:
        if kind_of.get(src) == "game":
            game_dsts[src].append(dst)

    sentinel_subject: dict[str, str] = {
        name: v.subject for name, v in nodes.items() if v.kind == "proof" and v.subject
    }

    layer_by_node = _compute_layers(node_names, kind_of, base_by_pair, game_dsts, sentinel_subject)

    # -- Sentinel lower bound -----------------------------------------------
    lib_sentinel: dict[str, str | None] = {n: None for n in lib_names}
    for proof_name, subject in sentinel_subject.items():
        if subject in lib_sentinel:
            lib_sentinel[subject] = proof_name

    lib_lower_bound: dict[str, list[str]] = {n: [] for n in lib_names}
    for lib_name, sentinel_name in lib_sentinel.items():
        if sentinel_name is None:
            continue
        seen: set[str] = set()
        ordered: list[str] = []
        for _order, dst in declared.sentinel_unconditional.get(sentinel_name, []):
            if dst == lib_name or dst in seen:
                continue
            seen.add(dst)
            ordered.append(dst)
        lib_lower_bound[lib_name] = ordered

    # -- Build edges[] --------------------------------------------------------
    edges_out: list[dict] = []
    for (src, dst) in sorted(pairs):
        pd = pairs[(src, dst)]
        is_game_edge = kind_of.get(src) == "game" or kind_of.get(dst) == "game"

        files_out = sorted(
            (
                {"file": e.src_file, "line": e.line, "raw": e.raw, "tags": sorted(e.tags)}
                for e, _cat in pd.lines
            ),
            key=lambda f: (f["file"], f["line"], f["raw"]),
        )
        symbols_out = sorted(
            ({"demangled": sym.demangled, "cls": sym.cls} for _m, sym in pd.symbols),
            key=lambda s: (s["demangled"], s["cls"] or ""),
        )
        types_out = _edge_types(types, files_owned_by, src, dst)

        is_allowlisted, allow_kind = allowlisted_by_pair.get((src, dst), (False, None))

        if is_game_edge:
            classes: list[str] = []
        else:
            base = base_by_pair.get((src, dst))
            if base is None:
                continue
            classes = [base]
            if is_allowlisted:
                classes.append("allowlisted")
            if src in in_cycle_nodes and dst in in_cycle_nodes and base in _IN_CYCLE_BASES:
                classes.append("in-cycle")

        lower_bound = dst in lib_lower_bound.get(src, [])

        edges_out.append(
            {
                "src": src,
                "dst": dst,
                "classes": classes,
                "allowlist_kind": allow_kind if (not is_game_edge and is_allowlisted) else None,
                "lower_bound": lower_bound,
                "include_lines": len({(e.src_file, e.line) for e, _c in pd.lines}) if pd.lines else len(pd.symbols),
                "files": files_out,
                "types": types_out,
                "symbols": symbols_out,
            }
        )

    # -- meta.health ----------------------------------------------------------
    ambiguous = [
        {"file": a["file"], "line": a["line"], "raw": a["raw"], "candidates": list(a["candidates"])}
        for a in sorted(graph.ambiguous, key=lambda a: (a["file"], a["line"], a["raw"]))
    ]
    missing_link_inputs = sorted(symbols.missing) if symbols is not None else []

    health = {
        "orphans": list(model.orphans),
        "duplicates": list(model.duplicates),
        "ambiguous": ambiguous,
        "unresolved_conditions": list(model.unresolved_conditions),
        "missing_link_inputs": missing_link_inputs,
    }

    # -- Per-lib computations ---------------------------------------------
    libs_out: dict[str, dict] = {}
    for L in lib_names:
        libs_out[L] = _compute_lib(
            L,
            model=model,
            graph=graph,
            types=types,
            nodes=nodes,
            kind_of=kind_of,
            files_owned_by=files_owned_by,
            base_by_pair=base_by_pair,
            pairs=pairs,
            sym_index=sym_index,
            symbols=symbols,
            sentinel=lib_sentinel[L],
            lower_bound=lib_lower_bound[L],
        )

    # -- nodes[] ----------------------------------------------------------
    nodes_out = []
    for n in node_names:
        kind = kind_of[n]
        inbound_size = sum(libs_out[n]["inbound"].values()) if kind == "lib" else 0
        nodes_out.append(
            {
                "name": n,
                "kind": kind,
                "layer": layer_by_node.get(n, 0),
                "in_cycle": n in in_cycle_nodes,
                "inbound_size": inbound_size,
            }
        )

    # -- meta ---------------------------------------------------------------
    if generated is None:
        generated = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    baseline_libs = (baseline or {}).get("libs", {}) if baseline is not None else {}
    delta: dict[str, dict[str, int]] | None
    delta_removed: list[str]
    delta_added: list[str]
    if baseline is not None:
        all_names = sorted(set(libs_out) | set(baseline_libs))
        delta = {}
        for name in all_names:
            cur = libs_out.get(name, {}).get("inbound", {f: 0 for f in INBOUND_FIELDS})
            base = baseline_libs.get(name, {}).get("inbound", {f: 0 for f in INBOUND_FIELDS})
            delta[name] = {f: cur.get(f, 0) - base.get(f, 0) for f in INBOUND_FIELDS}
        delta_removed = sorted(n for n in baseline_libs if n not in libs_out)
        delta_added = sorted(n for n in libs_out if n not in baseline_libs)
    else:
        delta = None
        delta_removed = []
        delta_added = []

    meta = {
        "config": model.config,
        "generated": generated,
        "repo_head": repo_head,
        "baseline": baseline_path,
        "health": health,
        "delta_removed": delta_removed,
        "delta_added": delta_added,
        "symbols": symbols is not None,
    }

    return {
        "meta": meta,
        "nodes": nodes_out,
        "edges": edges_out,
        "libs": libs_out,
        "delta": delta,
    }


# ---------------------------------------------------------------------------
# Per-lib surfaces.
# ---------------------------------------------------------------------------


def _compute_lib(
    L: str,
    *,
    model: "model_mod.Model",
    graph: "includes_mod.IncludeGraph",
    types: "cxxtypes_mod.TypeIndex",
    nodes: dict[str, "model_mod.Node"],
    kind_of: dict[str, str],
    files_owned_by: dict[str, str],
    base_by_pair: dict[tuple[str, str], str],
    pairs: dict[tuple[str, str], _PairData],
    sym_index: _SymbolIndex,
    symbols: "symbols_mod.SymbolTable | None",
    sentinel: str | None,
    lower_bound: list[str],
) -> dict:
    pch_root = model.pch_root

    # -- headers[] -----------------------------------------------------------
    lib_headers = sorted(
        p
        for p, owner in files_owned_by.items()
        if owner == L
        and Path(p).suffix.lower() in _HEADER_EXTS
        and not p.lower().endswith(".tests.inl")
    )

    # Cross-node NORMAL-line consumers, keyed by header path.
    consumers_by_header: dict[str, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    for h in lib_headers:
        for e in graph.in_edges(h):
            src_owner = files_owned_by.get(e.src_file)
            if src_owner is None or src_owner == L:
                continue
            if _line_category(e, pch_root) != "normal":
                continue
            consumers_by_header[h][src_owner].add(e.src_file)

    # L-owned header -> header graph (normal lines only, skip pch_root) for
    # the "exposed" BFS.
    header_set = set(lib_headers)
    own_header_graph: dict[str, set[str]] = {h: set() for h in lib_headers}
    for h in lib_headers:
        for e in graph.out_edges(h):
            if pch_root is not None and e.dst_file == pch_root:
                continue
            if e.dst_file in header_set:
                own_header_graph[h].add(e.dst_file)

    entry_headers = {h for h in lib_headers if consumers_by_header.get(h)}
    exposed_headers: set[str] = set()
    stack = list(entry_headers)
    visited = set(entry_headers)
    while stack:
        cur = stack.pop()
        for nxt in own_header_graph.get(cur, ()):
            if nxt in visited:
                continue
            visited.add(nxt)
            if nxt not in entry_headers:
                exposed_headers.add(nxt)
            stack.append(nxt)

    referenced_syms = _referenced_symbols(L, sym_index)
    syms_by_cls: dict[str | None, list[tuple[str, "symbols_mod.Symbol"]]] = defaultdict(list)
    for mangled, sym in referenced_syms.items():
        syms_by_cls[sym.cls].append((mangled, sym))

    header_out_of_line: dict[str, int] = defaultdict(int)
    for cls, entries in syms_by_cls.items():
        if cls is None:
            continue
        decl = types.decls.get(cls)
        if decl is None:
            continue
        header_out_of_line[decl.header] += len(entries)

    headers_out = []
    for h in lib_headers:
        cons = consumers_by_header.get(h, {})
        declared_internal = "Internal" in Path(h).parts
        if h in entry_headers:
            role = "entry"
        elif h in exposed_headers:
            role = "exposed"
        elif declared_internal:
            role = "internal"
        else:
            role = "unused-public"

        if pch_root is not None and h == pch_root:
            hint = "precompiled header: leave public"
        elif role == "entry" and declared_internal:
            hint = "encapsulation break: Internal/ header included from outside"
        elif role == "entry" and len(cons) == 1:
            (only_node,) = cons.keys()
            hint = f"1 consumer ({only_node}): move it there or behind an interface"
        elif role == "exposed":
            hint = "exposed transitively: forward-declare to cut"
        elif role == "unused-public" and header_out_of_line.get(h, 0) > 0:
            # No outside #include reaches this header at all, yet its
            # class(es) are linked from outside -- e.g. AI Tests.inl files
            # compiled into EntityComponent's own .cpp (test code), or a
            # forward-declared extern. consumers[] can't see that (it only
            # counts #include lines), so an ordinary "no outside users"
            # hint would be a false all-clear. Orchestrator-directed
            # refinement (2026-09-23 review).
            n_sool = header_out_of_line[h]
            hint = (
                f"no outside #include, but {n_sool} out-of-line symbols are linked "
                f"from outside (test code or extern declarations): check before moving"
            )
        elif role == "unused-public":
            hint = "no outside users: move to Internal/"
        else:
            hint = None

        headers_out.append(
            {
                "path": h,
                "role": role,
                "declared_internal": declared_internal,
                "consumers": [
                    {"node": node, "files": sorted(fset)} for node, fset in sorted(cons.items())
                ],
                "symbols_out_of_line": header_out_of_line.get(h, 0),
                "hint": hint,
            }
        )

    # -- types[] --------------------------------------------------------------
    lib_type_names = sorted(name for name, d in types.decls.items() if d.node == L)
    # file -> {type: mode} restricted to consumers owned by a DIFFERENT node.
    per_type_files: dict[str, dict[str, str]] = defaultdict(dict)  # type -> file -> mode
    for u in types.uses:
        decl = types.decls.get(u.type)
        if decl is None or decl.node != L:
            continue
        consumer_owner = files_owned_by.get(u.consumer_file)
        if consumer_owner is None or consumer_owner == L:
            continue
        cur = per_type_files[u.type].get(u.consumer_file)
        if cur is None or _MODE_RANK[u.mode] > _MODE_RANK[cur]:
            per_type_files[u.type][u.consumer_file] = u.mode

    types_out = []
    for t in lib_type_names:
        decl = types.decls[t]
        files_modes = per_type_files.get(t, {})
        embed = sum(1 for m in files_modes.values() if m == "embed")
        ref = sum(1 for m in files_modes.values() if m == "ref")
        mention = sum(1 for m in files_modes.values() if m == "mention")

        # One (node, mode) pair per DISTINCT mode actually observed across
        # that node's files -- binding amendment 7 ("a node may appear
        # once per mode").
        node_modes: set[tuple[str, str]] = set()
        for file, mode in files_modes.items():
            owner = files_owned_by.get(file)
            if owner is not None:
                node_modes.add((owner, mode))
        consumers_list = sorted(node_modes, key=lambda nm: (nm[0], nm[1]))

        if embed > 0:
            hint = f"embedded by value in {embed} consumer files: blocks swapping"
        elif ref > 0:
            hint = "only used by pointer/reference: forward-declare instead of including"
        elif mention > 0:
            hint = None
        else:
            hint = "unused outside: make internal"

        types_out.append(
            {
                "name": t,
                "header": decl.header,
                "embed": embed,
                "ref": ref,
                "mention": mention,
                "consumers": [{"node": n, "mode": m} for n, m in consumers_list],
                "hint": hint,
            }
        )

    # -- symbols[] / unreferenced[] --------------------------------------
    symbols_out = []
    for cls in sorted(syms_by_cls, key=lambda c: (c is None, c or "")):
        entries = syms_by_cls[cls]
        header = ""
        if cls is not None:
            decl = types.decls.get(cls)
            if decl is not None:
                header = decl.header
        count = sum(len(sym_index.referencers_of.get(m, ())) for m, _s in entries)
        referenced_by: set[str] = set()
        for m, _s in entries:
            referenced_by.update(sym_index.referencers_of.get(m, ()))
        symbols_out.append(
            {
                "cls": cls,
                "header": header,
                "count": count,
                "referenced_by": sorted(referenced_by),
            }
        )

    owned_all = sym_index.owned_by_node.get(L, {})
    unreferenced_out = sorted(
        (
            {"demangled": sym.demangled, "cls": sym.cls}
            for mangled, sym in owned_all.items()
            if not sym.generated and mangled not in referenced_syms
        ),
        key=lambda s: (s["demangled"]),
    )

    # -- leakage[] -------------------------------------------------------
    leakage_out = _compute_leakage(L, model, graph, files_owned_by, kind_of)

    # -- inbound / outbound -------------------------------------------------
    inbound = {
        "entry_headers": len(entry_headers),
        "exposed_headers": len(exposed_headers),
        "types_embed": sum(t["embed"] for t in types_out),
        "types_ref": sum(t["ref"] for t in types_out),
        "link_symbols": len(referenced_syms),
        "consumer_libs": len(
            {
                src
                for (src, dst), base in base_by_pair.items()
                if dst == L and kind_of.get(src) == "lib" and base in ("declared+used", "used-undeclared")
            }
        ),
        "consumer_games": len({src for (src, dst) in pairs if dst == L and kind_of.get(src) == "game"}),
        "leaking_headers": len(leakage_out),
    }

    outbound_edges = sorted({dst for (src, dst) in pairs if src == L})
    symbols_by_target: dict[str, list[dict]] = {}
    for (src, dst), pd in pairs.items():
        if src != L or not pd.symbols:
            continue
        symbols_by_target[dst] = sorted(
            ({"demangled": sym.demangled, "cls": sym.cls} for _m, sym in pd.symbols),
            key=lambda s: (s["demangled"], s["cls"] or ""),
        )

    outbound = {
        "edges": outbound_edges,
        "symbols_by_target": dict(sorted(symbols_by_target.items())),
        "lower_bound": lower_bound,
    }

    return {
        "inbound": inbound,
        "outbound": outbound,
        "headers": headers_out,
        "types": types_out,
        "symbols": symbols_out,
        "unreferenced": unreferenced_out,
        "leakage": leakage_out,
        "sentinel": sentinel,
    }


def _referenced_symbols(L: str, sym_index: _SymbolIndex) -> dict[str, "symbols_mod.Symbol"]:
    owned = sym_index.owned_by_node.get(L, {})
    return {
        m: s
        for m, s in owned.items()
        if not s.generated and sym_index.referencers_of.get(m)
    }


def _compute_leakage(
    L: str,
    model: "model_mod.Model",
    graph: "includes_mod.IncludeGraph",
    files_owned_by: dict[str, str],
    kind_of: dict[str, str],
) -> list[dict]:
    l_headers = {p for p, owner in files_owned_by.items() if owner == L}
    if not l_headers:
        return []

    # foreign HEADER H (owned by V != L) with a NORMAL line directly
    # including an L-owned header. H itself must be a header (.h/.hpp/
    # .inl) -- a .cpp can't be included onward by anyone else, so it
    # can't "leak" L outward regardless of what it includes directly.
    leaking: dict[str, str] = {}  # header -> via_node
    for h_path, owner in files_owned_by.items():
        if owner == L or owner is None:
            continue
        if Path(h_path).suffix.lower() not in _HEADER_EXTS:
            continue
        if kind_of.get(owner) == "game":
            # A game is a leaf: nothing else can #include one of ITS
            # headers, so a game-owned header can't leak L onward to
            # anyone -- it's just that game's own direct reach into L,
            # already fully represented by the game's own edge. Orchestrator
            # -directed refinement (2026-09-23 review).
            continue
        for e in graph.out_edges(h_path):
            if e.dst_file in l_headers and _line_category(e, model.pch_root) == "normal":
                leaking[h_path] = owner
                break

    if not leaking:
        return []

    # Which D files have a normal line directly including an L-owned
    # header at all (used to exclude D from "reaches L only via H").
    direct_l_reachers: set[str] = set()
    for e in graph.edges:
        if e.dst_file in l_headers and _line_category(e, model.pch_root) == "normal":
            src_owner = files_owned_by.get(e.src_file)
            if src_owner is not None:
                direct_l_reachers.add(src_owner)

    out = []
    for h_path in sorted(leaking):
        via_node = leaking[h_path]
        downstream_header: set[str] = set()
        downstream_cpp_only: set[str] = set()

        # Candidate downstream nodes: everyone except L, via_node, proof.
        by_node_files: dict[str, list[str]] = defaultdict(list)
        for p, owner in files_owned_by.items():
            if owner in (L, via_node) or owner is None:
                continue
            if kind_of.get(owner) == "proof":
                continue
            by_node_files[owner].append(p)

        for D, files in by_node_files.items():
            if D in direct_l_reachers:
                continue
            reaches_via_h = False
            reaches_via_header_file = False
            for f in files:
                if h_path in graph.closure_no_pch(f):
                    reaches_via_h = True
                    if Path(f).suffix.lower() in _HEADER_EXTS:
                        reaches_via_header_file = True
            if not reaches_via_h:
                continue
            if reaches_via_header_file:
                downstream_header.add(D)
            else:
                downstream_cpp_only.add(D)

        out.append(
            {
                "header": h_path,
                "via_node": via_node,
                "downstream_via_header": sorted(downstream_header),
                "downstream_via_cpp_only": sorted(downstream_cpp_only),
            }
        )

    # Sort by total downstream count desc (the genuine, high-signal leaks
    # first), then header path for a stable tiebreak. Orchestrator-directed
    # refinement (2026-09-23 review).
    out.sort(
        key=lambda entry: (
            -(len(entry["downstream_via_header"]) + len(entry["downstream_via_cpp_only"])),
            entry["header"],
        )
    )

    return out


# ---------------------------------------------------------------------------
# __main__ -- the W3 DoD real-repo report.
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="libgraph metrics")
    parser.add_argument("--config", default="vulkan_vs2022_debug_win64_true")
    parser.add_argument("--repo-root", default=None)
    parser.add_argument("--dumpbin-dir", default=None)
    parser.add_argument("--no-symbols", action="store_true")
    parser.add_argument("--baseline", default=None)
    parser.add_argument("--out", default=None)
    parser.add_argument("--repo-head", default="")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[2]

    t0 = time.time()
    m = model_mod.build_model(repo_root, args.config)
    t_model = time.time() - t0

    t1 = time.time()
    graph = includes_mod.build(m)
    t_includes = time.time() - t1

    t2 = time.time()
    types = cxxtypes_mod.build(m, graph)
    t_types = time.time() - t2

    symbols = None
    t_symbols = 0.0
    if not args.no_symbols:
        t3 = time.time()
        symbols = symbols_mod.build(m, dumpbin_dir=Path(args.dumpbin_dir) if args.dumpbin_dir else None)
        t_symbols = time.time() - t3

    baseline_doc = None
    if args.baseline:
        import json

        baseline_doc = json.loads(Path(args.baseline).read_text(encoding="utf-8"))

    t4 = time.time()
    doc = compute(
        m, graph, types, symbols, baseline=baseline_doc, baseline_path=args.baseline, repo_head=args.repo_head
    )
    t_compute = time.time() - t4

    print(f"model: {t_model:.2f}s  includes: {t_includes:.2f}s  types: {t_types:.2f}s  "
          f"symbols: {t_symbols:.2f}s  compute: {t_compute:.2f}s  total: {time.time() - t0:.2f}s")
    print(f"nodes: {len(doc['nodes'])}  edges: {len(doc['edges'])}  libs: {len(doc['libs'])}")

    if args.out:
        import json

        out_path = Path(args.out)
        out_path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
        print(f"wrote {out_path} ({out_path.stat().st_size} bytes)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
