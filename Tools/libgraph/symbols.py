"""libgraph's link-surface measurement: parses `dumpbin /symbols` output into
a SymbolTable and drives running dumpbin (or reading canned fixture text)
across every analysed node in a Model.

See the approved plan (C:\\Users\\tomos\\.claude\\plans\\i-want-a-way-quirky-mountain.md),
pipeline step 4 and "Shared contracts" / "The dumpbin seam", and the W2b
brief's "Binding amendments" section, which is the final word whenever it
disagrees with the plan prose.

Run as a script from the repo root for the DoD smoke checks:
    python Tools/libgraph/symbols.py --config <cfg> [--dumpbin-dir <dir>]

Python 3.12 stdlib only. Flat sibling module: no package, no relative import
(model is imported as a plain top-level module).
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path

import model as model_mod

# ---------------------------------------------------------------------------
# Contract dataclasses.
# ---------------------------------------------------------------------------


class SymbolsError(Exception):
    """Every refusal in this module raises this, with a message a human can
    act on (names the node/config, or prints the exact expected path)."""


@dataclass
class Symbol:
    mangled: str
    demangled: str
    scopes: list[str] = field(default_factory=list)
    cls: str | None = None
    is_data: bool = False
    generated: bool = False


@dataclass
class SymbolTable:
    owned: dict[str, dict[str, Symbol]] = field(default_factory=dict)
    inline: dict[str, set[str]] = field(default_factory=dict)
    undef: dict[str, set[str]] = field(default_factory=dict)
    conflicts: dict[str, list[str]] = field(default_factory=dict)
    stats: dict[str, dict[str, int]] = field(default_factory=dict)
    missing: list[str] = field(default_factory=list)

    def owner_of(self, mangled: str) -> str | None:
        """The node that owns `mangled` non-COMDAT, else None (not found,
        or a conflict -- owned by 2+ nodes)."""
        owners = [node for node, syms in self.owned.items() if mangled in syms]
        if len(owners) == 1:
            return owners[0]
        return None


_EMPTY_STATS = {
    "external_defs": 0,
    "owned": 0,
    "comdat_lines": 0,
    "inline_unique": 0,
    "undef": 0,
}

# ---------------------------------------------------------------------------
# Compiler-generated mangled-name prefixes (amendment 4): these get
# scopes=[]/cls=None WITHOUT attempting to demangle-parse them.
# ---------------------------------------------------------------------------

_GENERATED_PREFIXES = (
    "??_7",  # vftable
    "??_R",  # RTTI
    "_CT??_R0",  # RTTI (complete object locator entries)
    "_TI",  # RTTI (throw info)
    "_CTA",  # RTTI (catchable type array)
    "??_C@",  # string literal
    "__real@",
    "__xmm@",
    "??__E",  # dynamic initializer
    "??__F",  # atexit destructor
    "__imp_",  # import thunk
)


def _is_generated(mangled: str) -> bool:
    return mangled.startswith(_GENERATED_PREFIXES)


# ---------------------------------------------------------------------------
# Scope/cls extraction from a demangled string (amendment 3).
#
# Split at "::" only at depth 0 of BOTH <...> and backtick-quote `...'.
# A backtick-quoted segment is one atomic token. The entity's own
# unqualified name is the LAST split segment and is dropped -- `scopes` is
# only the ENCLOSING qualifier chain, outermost first.
# ---------------------------------------------------------------------------


def _scan(s: str) -> tuple[list[tuple[int, int]], list[int], list[int]]:
    """One pass over `s`, tracking a combined <>/() nesting depth and
    backtick-quote spans (a backtick-quoted `...' run is opaque -- nothing
    inside it counts toward depth, spaces or "::" splits).

    Returns (top_level_paren_groups, top_level_space_indices,
    top_level_double_colon_indices)."""
    depth = 0
    in_backtick = False
    paren_stack: list[int] = []
    paren_groups: list[tuple[int, int]] = []
    spaces: list[int] = []
    colons: list[int] = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if in_backtick:
            if c == "'":
                in_backtick = False
            i += 1
            continue
        if c == "`":
            in_backtick = True
            i += 1
            continue
        if depth == 0 and c == " ":
            spaces.append(i)
        if depth == 0 and c == ":" and i + 1 < n and s[i + 1] == ":":
            colons.append(i)
        if c in "(<":
            if c == "(" and depth == 0:
                paren_stack.append(i)
            depth += 1
        elif c in ")>":
            if depth > 0:
                depth -= 1
                if c == ")" and depth == 0 and paren_stack:
                    paren_groups.append((paren_stack.pop(), i))
        i += 1
    return paren_groups, spaces, colons


def _strip_template_args(segment: str) -> str:
    """`Foo<int>` -> `Foo`; a plain segment (no `<`) is returned unchanged.
    Never applied to a backtick-atomic segment (caller's job to skip it)."""
    idx = segment.find("<")
    if idx == -1:
        return segment
    return segment[:idx]


def _extract_scopes(demangled: str, is_data: bool) -> list[str]:
    s = demangled.strip()

    for spec in ("public:", "protected:", "private:"):
        if s.startswith(spec):
            s = s[len(spec):].strip()
            break

    changed = True
    while changed:
        changed = False
        for kw in ("static ", "virtual "):
            if s.startswith(kw):
                s = s[len(kw):].lstrip()
                changed = True

    if not is_data:
        groups, _spaces, _colons = _scan(s)
        if groups:
            # The LAST top-level (...) group is the real parameter list --
            # scanning left-to-right and keeping the last one found handles
            # "operator()(void)const" correctly: the empty "()" that is
            # part of the operator's own NAME is an earlier top-level group,
            # the real "(void)" params is the last one.
            s = s[: groups[-1][0]]

    s = s.rstrip()
    _groups2, spaces2, _colons2 = _scan(s)
    if spaces2:
        s = s[spaces2[-1] + 1:]
    s = s.strip()

    if not s:
        return []

    _groups3, _spaces3, colons3 = _scan(s)
    segments: list[str] = []
    start = 0
    for idx in colons3:
        segments.append(s[start:idx])
        start = idx + 2
    segments.append(s[start:])
    segments = [seg for seg in segments if seg]
    if not segments:
        return []

    # The entity's own unqualified name is the last segment; scopes is only
    # the enclosing chain.
    scopes_raw = segments[:-1]
    if not scopes_raw:
        return []

    first = scopes_raw[0]
    if first.startswith("`") and first.endswith("'") and len(first) >= 2:
        inner = first[1:-1]
        # A lambda/local-scope entity's outer segment is itself a full
        # function signature ("public: static void __cdecl X::F(void)") --
        # recurse into it and use ITS scopes as the whole chain (collapsing
        # the synthetic `2'/<lambda_N>/operator() suffix), recovering the
        # real enclosing class. A plain pseudo-scope like
        # `anonymous namespace' has neither "(" nor "::" and is kept as-is.
        if "(" in inner or "::" in inner:
            return _extract_scopes(inner, is_data=False)

    cleaned: list[str] = []
    for seg in scopes_raw:
        if seg.startswith("`"):
            cleaned.append(seg)
        else:
            cleaned.append(_strip_template_args(seg))
    return cleaned


def _cls_from_scopes(scopes: list[str]) -> str | None:
    if not scopes:
        return None
    last = scopes[-1]
    if last.startswith("`"):
        return None
    return last


def _build_symbol(mangled: str, demangled: str, is_func: bool) -> Symbol:
    is_data = not is_func
    if _is_generated(mangled):
        return Symbol(
            mangled=mangled,
            demangled=demangled,
            scopes=[],
            cls=None,
            is_data=is_data,
            generated=True,
        )
    scopes = _extract_scopes(demangled, is_data)
    cls = _cls_from_scopes(scopes)
    return Symbol(
        mangled=mangled,
        demangled=demangled,
        scopes=scopes,
        cls=cls,
        is_data=is_data,
        generated=False,
    )


# ---------------------------------------------------------------------------
# Line-level parsing of `dumpbin /symbols` text.
# ---------------------------------------------------------------------------

_HEX_RE = re.compile(r"^[0-9A-Fa-f]+$")


def _parse_record_line(line: str):
    """Returns (section, storage, is_func, mangled, demangled) for a symbol
    table record line, or None if the line isn't one (aux lines, the file
    header, the trailing Summary block, "COFF SYMBOL TABLE" itself, etc. --
    none of those contain a "|"). No fixed-width column slicing: whitespace
    split only, so differing padding (SECT5 vs SECT23B) never matters."""
    if "|" not in line:
        return None
    left, right = line.split("|", 1)
    left_tokens = left.split()
    if len(left_tokens) < 4:
        return None
    idx, addr, section = left_tokens[0], left_tokens[1], left_tokens[2]
    if not _HEX_RE.match(idx) or not _HEX_RE.match(addr):
        return None
    storage = left_tokens[-1]
    type_tokens = left_tokens[3:-1]
    if not type_tokens:
        return None
    is_func = "()" in type_tokens

    right = right.strip()
    if not right:
        return None
    parts = right.split(None, 1)
    mangled = parts[0]
    rest = parts[1].strip() if len(parts) > 1 else ""
    if rest.startswith("(") and rest.endswith(")"):
        demangled = rest[1:-1]
    else:
        # amendment 5: no-parenthesis names are first-class.
        demangled = mangled

    return section, storage, is_func, mangled, demangled


def parse_dumpbin_text(node_name: str, text: str) -> SymbolTable:
    """Partial SymbolTable for ONE node, parsed from one `dumpbin /symbols`
    text blob (which may hold several COFF SYMBOL TABLE blocks -- one per
    archive/object member; section numbers are per-block)."""
    owned: dict[str, Symbol] = {}
    inline: set[str] = set()
    undef: set[str] = set()
    stats = dict(_EMPTY_STATS)
    comdat_sections: set[str] = set()

    lines = text.splitlines()
    n = len(lines)
    i = 0
    while i < n:
        line = lines[i]
        if line.strip() == "COFF SYMBOL TABLE":
            comdat_sections = set()
            i += 1
            continue

        rec = _parse_record_line(line)
        if rec is None:
            i += 1
            continue
        section, storage, is_func, mangled, demangled = rec

        next_line = lines[i + 1] if i + 1 < n else ""
        if next_line.strip().startswith("Section length"):
            # A static section-defining row (e.g. "SECT5 notype Static |
            # .text$mn") followed by its aux descriptor line -- COMDAT iff
            # that aux line names a selection, scoped to THIS block only.
            if section.startswith("SECT") and "selection" in next_line:
                comdat_sections.add(section)
            i += 2
            continue

        if storage != "External":
            i += 1
            continue

        if section == "UNDEF":
            undef.add(mangled)
            stats["undef"] += 1
            i += 1
            continue

        if not section.startswith("SECT"):
            # ABS, DEBUG, or anything else not SECTn/UNDEF -- skip.
            i += 1
            continue

        stats["external_defs"] += 1
        if section in comdat_sections:
            inline.add(mangled)
            stats["comdat_lines"] += 1
        else:
            owned[mangled] = _build_symbol(mangled, demangled, is_func)
            stats["owned"] += 1
        i += 1

    stats["inline_unique"] = len(inline)

    return SymbolTable(
        owned={node_name: owned},
        inline={node_name: inline},
        undef={node_name: undef},
        conflicts={},
        stats={node_name: stats},
        missing=[],
    )


# ---------------------------------------------------------------------------
# Merging partial tables (one node may be parsed from several dumpbin
# invocations -- a game's chunked .obj command lines).
# ---------------------------------------------------------------------------


def _merge_tables(tables: list[SymbolTable]) -> SymbolTable:
    owned: dict[str, dict[str, Symbol]] = {}
    inline: dict[str, set[str]] = {}
    undef_raw: dict[str, set[str]] = {}
    stats: dict[str, dict[str, int]] = {}
    missing: list[str] = []

    for t in tables:
        missing.extend(t.missing)
        for node, syms in t.owned.items():
            owned.setdefault(node, {}).update(syms)
        for node, s in t.inline.items():
            inline.setdefault(node, set()).update(s)
        for node, s in t.undef.items():
            undef_raw.setdefault(node, set()).update(s)
        for node, st in t.stats.items():
            acc = stats.setdefault(node, dict(_EMPTY_STATS))
            for k in _EMPTY_STATS:
                acc[k] += st.get(k, 0)

    # inline_unique is a DEDUPED count -- recompute from the final merged
    # set rather than summing per-chunk counts (which would double-count a
    # COMDAT symbol instantiated in more than one .obj/block).
    for node in stats:
        stats[node]["inline_unique"] = len(inline.get(node, set()))

    owner_lists: dict[str, list[str]] = {}
    for node, syms in owned.items():
        for mangled in syms:
            owner_lists.setdefault(mangled, []).append(node)
    conflicts = {m: sorted(nodes) for m, nodes in owner_lists.items() if len(nodes) > 1}

    # A single unambiguous owner per mangled name (conflicts resolve to
    # nobody, same as SymbolTable.owner_of()).
    owner_index = {m: nodes[0] for m, nodes in owner_lists.items() if len(nodes) == 1}

    # Drop UNDEFs that resolve to no owner at all (CRT, the SDK,
    # __CheckForDebuggerJustMyCode, glfw, vulkan, slang import libs, ...) and
    # ones that resolve back to the SAME node (an internal reference some
    # .obj member happened to also mark External/UNDEF) -- keep only undefs
    # whose owner is a genuinely different node.
    undef: dict[str, set[str]] = {}
    for node, mangled_set in undef_raw.items():
        undef[node] = {
            m for m in mangled_set if owner_index.get(m) not in (None, node)
        }

    return SymbolTable(
        owned=owned,
        inline=inline,
        undef=undef,
        conflicts=conflicts,
        stats=stats,
        missing=missing,
    )


# ---------------------------------------------------------------------------
# Locating dumpbin.exe (amendment 2).
# ---------------------------------------------------------------------------

_VSWHERE = r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
_DUMPBIN_FIND_PATTERN = r"VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe"
_MSVC_VERSION_RE = re.compile(r"MSVC[\\/]([\d.]+)[\\/]")


def _version_sort_key(path: str) -> tuple:
    m = _MSVC_VERSION_RE.search(path)
    if not m:
        return ((), path)
    parts = tuple(int(p) for p in m.group(1).split(".") if p.isdigit())
    return (parts, path)


def locate_dumpbin() -> str:
    override = os.environ.get("LIBGRAPH_DUMPBIN")
    if override:
        return override

    vswhere = os.path.expandvars(_VSWHERE)
    if not os.path.isfile(vswhere):
        raise SymbolsError(
            f"vswhere.exe not found at {vswhere} -- set LIBGRAPH_DUMPBIN to "
            f"dumpbin.exe's full path to override."
        )
    try:
        proc = subprocess.run(
            [vswhere, "-latest", "-prerelease", "-products", "*", "-find", _DUMPBIN_FIND_PATTERN],
            capture_output=True,
            text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise SymbolsError(f"vswhere failed to locate dumpbin.exe: {exc}") from exc

    candidates = [ln.strip() for ln in proc.stdout.splitlines() if ln.strip()]
    if not candidates:
        raise SymbolsError(
            "vswhere found no dumpbin.exe under any installed MSVC toolset -- "
            "set LIBGRAPH_DUMPBIN to override."
        )
    # -find returns one line per installed MSVC toolset -- pick the highest
    # version (the last one once sorted).
    candidates.sort(key=_version_sort_key)
    return candidates[-1]


# ---------------------------------------------------------------------------
# Running dumpbin (real, non-fixture path).
# ---------------------------------------------------------------------------

_MAX_CMDLINE_CHARS = 30000


def _chunk_paths(paths: list[str], base_len: int) -> list[list[str]]:
    """Chunks `paths` into groups whose total command-line length (plus
    `base_len` for the exe path and fixed flags) stays under the Windows
    CreateProcess limit."""
    chunks: list[list[str]] = []
    current: list[str] = []
    current_len = base_len
    for p in paths:
        add_len = len(p) + 3  # separating space + a little slack for quoting
        if current and current_len + add_len > _MAX_CMDLINE_CHARS:
            chunks.append(current)
            current = []
            current_len = base_len
        current.append(p)
        current_len += add_len
    if current:
        chunks.append(current)
    return chunks


def _dumpbin_worker(dumpbin_exe: str, paths: list[str], node_name: str) -> dict:
    """Runs dumpbin over `paths` and parses the result. Module-level so it's
    picklable for ProcessPoolExecutor; returns a compact dict rather than a
    whole SymbolTable (amendment 9 -- ProcessPoolExecutor for dumpbin+parse
    per input, since the GIL makes a thread pool useless for the parsing
    cost)."""
    proc = subprocess.run(
        [dumpbin_exe, "/symbols", *paths],
        capture_output=True,
        text=True,
        errors="replace",
    )
    table = parse_dumpbin_text(node_name, proc.stdout)
    return {
        "node": node_name,
        "owned": table.owned.get(node_name, {}),
        "inline": table.inline.get(node_name, set()),
        "undef": table.undef.get(node_name, set()),
        "stats": table.stats.get(node_name, dict(_EMPTY_STATS)),
    }


def _table_from_compact(compact: dict) -> SymbolTable:
    node = compact["node"]
    return SymbolTable(
        owned={node: compact["owned"]},
        inline={node: set(compact["inline"])},
        undef={node: set(compact["undef"])},
        conflicts={},
        stats={node: dict(compact["stats"])},
        missing=[],
    )


# ---------------------------------------------------------------------------
# Cache (amendment 11): keyed by (absolute path, size, mtime_ns, parser
# version), one JSON file per cache entry under
# <repo_root>/Build/artifacts/libgraph/cache/. Bypassed entirely when
# dumpbin_dir is given.
# ---------------------------------------------------------------------------

_PARSER_VERSION = 1


def _cache_dir(repo_root: Path) -> Path:
    d = repo_root / "Build" / "artifacts" / "libgraph" / "cache"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _cache_key(paths: list[Path]) -> str:
    h = hashlib.sha256()
    h.update(str(_PARSER_VERSION).encode("utf-8"))
    for p in paths:
        st = p.stat()
        h.update(str(p.resolve()).encode("utf-8"))
        h.update(str(st.st_size).encode("utf-8"))
        h.update(str(st.st_mtime_ns).encode("utf-8"))
    return h.hexdigest()


def _cache_file(repo_root: Path, key: str) -> Path:
    return _cache_dir(repo_root) / f"{key}.json"


def _load_cache(repo_root: Path, key: str) -> dict | None:
    p = _cache_file(repo_root, key)
    if not p.is_file():
        return None
    try:
        with p.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    try:
        owned = {m: Symbol(**s) for m, s in data["owned"].items()}
        return {
            "node": data["node"],
            "owned": owned,
            "inline": set(data["inline"]),
            "undef": set(data["undef"]),
            "stats": data["stats"],
        }
    except (KeyError, TypeError):
        return None


def _save_cache(repo_root: Path, key: str, compact: dict) -> None:
    p = _cache_file(repo_root, key)
    serializable = {
        "node": compact["node"],
        "owned": {m: dataclasses.asdict(s) for m, s in compact["owned"].items()},
        "inline": sorted(compact["inline"]),
        "undef": sorted(compact["undef"]),
        "stats": compact["stats"],
    }
    tmp = p.with_suffix(".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(serializable, f)
    tmp.replace(p)


# ---------------------------------------------------------------------------
# build()
# ---------------------------------------------------------------------------


def _fll_refusal(node: "model_mod.Node", config: str) -> str:
    return (
        f"{node.name}: FunctionLevelLinking is enabled for config '{config}' -- "
        f"/Gy puts every function into its own COMDAT section, which defeats "
        f"the owned/inline (COMDAT) distinction symbols.py relies on. Choose a "
        f"Debug config (FunctionLevelLinking=false) instead."
    )


def build(model: "model_mod.Model", dumpbin_dir: Path | str | None = None, jobs: int = 0) -> SymbolTable:
    repo_root = Path(model.repo_root)
    dumpbin_dir_path = Path(dumpbin_dir) if dumpbin_dir is not None else None

    selected_libs = [
        n for n in model.nodes.values() if n.kind in ("lib", "middleware") and n.out_lib
    ]
    selected_games = [n for n in model.nodes.values() if n.kind == "game"]

    for node in selected_libs + selected_games:
        if node.function_level_linking:
            raise SymbolsError(_fll_refusal(node, model.config))

    tables: list[SymbolTable] = []
    extra_missing: list[str] = []

    if dumpbin_dir_path is not None:
        for node in selected_libs + selected_games:
            txt_path = dumpbin_dir_path / f"{node.name}.txt"
            if not txt_path.is_file():
                raise SymbolsError(
                    f"{node.name}: expected dumpbin fixture not found at {txt_path}"
                )
            text = txt_path.read_text(encoding="utf-8", errors="replace")
            tables.append(parse_dumpbin_text(node.name, text))
    else:
        dumpbin_exe = locate_dumpbin()

        # (node_name, args, cache_paths)
        work_items: list[tuple[str, list[str], list[Path]]] = []

        for node in selected_libs:
            lib_path = repo_root / node.out_lib
            if not lib_path.is_file():
                raise SymbolsError(
                    f"{node.name}: expected library not found at {lib_path} -- "
                    f"run `zenith build engine` (or the owning game) for config "
                    f"{model.config} to produce it."
                )
            work_items.append((node.name, [str(lib_path)], [lib_path]))

        base_len = len(dumpbin_exe) + len("/symbols") + 8
        for node in selected_games:
            if not node.int_dir:
                extra_missing.append(
                    f"{node.name}: no int_dir known for config {model.config} -- "
                    f"run `zenith build {node.name}` for this config."
                )
                continue
            int_dir_path = repo_root / node.int_dir
            obj_paths = sorted(int_dir_path.rglob("*.obj")) if int_dir_path.is_dir() else []
            if not obj_paths:
                extra_missing.append(
                    f"{node.name}: no .obj files under {int_dir_path} -- run "
                    f"`zenith build {node.name}` for config {model.config} to "
                    f"produce them."
                )
                continue
            for chunk in _chunk_paths([str(p) for p in obj_paths], base_len):
                work_items.append((node.name, chunk, [Path(p) for p in chunk]))

        compacts: list[dict] = []
        to_run: list[tuple[str, list[str], str]] = []
        for node_name, args, cache_paths in work_items:
            key = _cache_key(cache_paths)
            cached = _load_cache(repo_root, key)
            if cached is not None:
                compacts.append(cached)
            else:
                to_run.append((node_name, args, key))

        if to_run:
            jobs_n = jobs or os.cpu_count() or 1
            with ProcessPoolExecutor(max_workers=jobs_n) as ex:
                futures = {
                    ex.submit(_dumpbin_worker, dumpbin_exe, args, node_name): key
                    for node_name, args, key in to_run
                }
                for fut in as_completed(futures):
                    key = futures[fut]
                    compact = fut.result()
                    _save_cache(repo_root, key, compact)
                    compacts.append(compact)

        for compact in compacts:
            tables.append(_table_from_compact(compact))

    merged = _merge_tables(tables)
    merged.missing = sorted(set(merged.missing) | set(extra_missing))
    return merged


# ---------------------------------------------------------------------------
# __main__ -- the W2b DoD smoke checks.
# ---------------------------------------------------------------------------


def _smoke_real_lib(lib_path: Path) -> None:
    print(f"--- smoke: parse_dumpbin_text on {lib_path} ---")
    dumpbin_exe = locate_dumpbin()
    t0 = time.time()
    proc = subprocess.run(
        [dumpbin_exe, "/symbols", str(lib_path)],
        capture_output=True,
        text=True,
        errors="replace",
    )
    dump_elapsed = time.time() - t0
    t1 = time.time()
    table = parse_dumpbin_text(lib_path.stem, proc.stdout)
    parse_elapsed = time.time() - t1
    st = table.stats[lib_path.stem]
    print(f"dumpbin wall clock: {dump_elapsed:.2f}s, parse wall clock: {parse_elapsed:.2f}s")
    print(
        f"External-defined total: {st['external_defs']}  owned: {st['owned']}  "
        f"comdat_lines (inline, raw): {st['comdat_lines']}  "
        f"inline_unique: {st['inline_unique']}  undef: {st['undef']}"
    )
    counts: dict[str, int] = {}
    for sym in table.owned[lib_path.stem].values():
        if sym.cls:
            counts[sym.cls] = counts.get(sym.cls, 0) + 1
    top10 = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[:10]
    print("Top 10 classes by owned-symbol count:")
    for cls, n in top10:
        print(f"  {cls}: {n}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="libgraph symbols module smoke checks")
    parser.add_argument("--config", default="vulkan_vs2022_debug_win64_true")
    parser.add_argument("--repo-root", default=None)
    parser.add_argument("--dumpbin-dir", default=None)
    parser.add_argument("--smoke-lib", default=None, help="Path to a .lib for the DoD smoke check")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[2]

    if args.smoke_lib:
        _smoke_real_lib(Path(args.smoke_lib))
        return 0

    try:
        m = model_mod.build_model(repo_root, args.config)
        dumpbin_dir = Path(args.dumpbin_dir) if args.dumpbin_dir else None
        t0 = time.time()
        table = build(m, dumpbin_dir=dumpbin_dir)
        elapsed = time.time() - t0
    except SymbolsError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"build() wall clock: {elapsed:.2f}s")
    print(f"nodes with owned symbols: {len(table.owned)}")
    print(f"conflicts: {len(table.conflicts)}")
    print(f"missing: {table.missing}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
