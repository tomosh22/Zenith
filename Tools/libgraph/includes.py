"""libgraph's include graph: parses `#include "..."` edges out of every file
in the model, resolves them the way MSVC's quoted-include search does, and
exposes the transitive header closure.

Built on top of `model.py` (wave 1): `build(model)` walks every file in
`model.files` (listed or dir-fallback) plus `model.orphans`, and returns an
`IncludeGraph` of `IncludeEdge`s plus the ambiguous/unresolved self-checks
described in the approved plan
(C:\\Users\\tomos\\.claude\\plans\\i-want-a-way-quirky-mountain.md, pipeline
step 2) and pinned by the W2a brief.

Resolution order (MSVC's quoted-include order) for an include inside a file
F owned by node N:
  1. F's own directory.
  2. The parent include stack: the directories of files that (transitively)
     include F, nearest first. Implemented as a fixed-point fallback pass
     over whatever's still unresolved after step 1+3, since there's no
     single static stack.
  3. N's `include_dirs`, in order.
Resolution is case-insensitive (Windows), but `dst_file` is always recorded
in the canonical on-disk case (the form `model.files` keys use). `../`
segments are normalised with `posixpath.normpath`, never string
concatenation. A quoted include satisfied by 2+ distinct existing files
across steps 1-3 is reported in `ambiguous`; the first candidate (in search
order) still wins as `dst_file`. A quoted include that resolves to nothing
in the repo (a middleware/system/SDK header, or a genuine typo) is recorded
in `unresolved`, never as an edge.

Known heuristic blind spots (documented per the brief -- these are the same
family of blind spot as types.py's use-classifier):
  - The parent-include-stack fallback is a global fixed point over ALL
    files' resolved edges, not a per-translation-unit static stack -- two
    different .cpp files including the same ambiguous header via different
    paths could, in a pathological tree, resolve differently than a real
    compiler would for one of them. Not observed in the fixture or the real
    repo (only `Zenith_OS_Include.h` has a second, non-win64 candidate).
  - An include string that resolves to a real file OUTSIDE the repo (an
    absolute SDK/middleware include dir) is folded into `unresolved`
    indistinguishably from a genuine typo -- we don't touch the real
    filesystem to tell them apart, only the tracked repo-file universe.
  - The include-guard exception only recognises the single top-of-file
    `#ifndef X` / `#define X` pair; a file using `#pragma once` AND a
    redundant legacy guard, or a guard whose `#define` isn't the very next
    non-blank line, isn't specially exempted (harmless here since the real
    repo uses `#pragma once` universally -- see the brief's binding
    amendment 8).
"""

from __future__ import annotations

import posixpath
import re
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

# Verbatim copy of Tools/analyze_code_complexity.py:92-98 (per the W1 brief
# and the approved plan -- NOT an import, a copy).
_LOCAL_INCLUDE_PATTERN: re.Pattern = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.MULTILINE)
_PREPROC_OPEN_PATTERN: re.Pattern = re.compile(r'^[ \t]*#[ \t]*(?:if|ifdef|ifndef)\b', re.MULTILINE)
_PREPROC_CLOSE_PATTERN: re.Pattern = re.compile(r'^[ \t]*#[ \t]*endif\b', re.MULTILINE)

_GUARD_IFNDEF_RE = re.compile(r'^[ \t]*#[ \t]*ifndef[ \t]+(\w+)[ \t]*$')
_GUARD_DEFINE_RE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)\b')

_HEADER_EXTS = {".h", ".hpp", ".inl", ".cpp", ".c"}


# ---------------------------------------------------------------------------
# Contract dataclasses.
# ---------------------------------------------------------------------------


@dataclass
class IncludeEdge:
    src_file: str
    line: int
    raw: str
    dst_file: str
    tags: set[str] = field(default_factory=set)


@dataclass
class IncludeGraph:
    edges: list[IncludeEdge]
    unresolved: list[tuple[str, int, str]]
    ambiguous: list[dict]
    pch_root: str | None = None
    _out: dict[str, list[IncludeEdge]] = field(default_factory=dict, repr=False, compare=False)
    _in: dict[str, list[IncludeEdge]] = field(default_factory=dict, repr=False, compare=False)
    _closure_cache: dict[str, frozenset[str]] = field(default_factory=dict, repr=False, compare=False)
    _closure_no_pch_cache: dict[str, frozenset[str]] = field(default_factory=dict, repr=False, compare=False)

    def __post_init__(self) -> None:
        for e in self.edges:
            self._out.setdefault(e.src_file, []).append(e)
            self._in.setdefault(e.dst_file, []).append(e)

    def out_edges(self, file: str) -> list[IncludeEdge]:
        return list(self._out.get(file, ()))

    def in_edges(self, file: str) -> list[IncludeEdge]:
        return list(self._in.get(file, ()))

    def closure(self, file: str) -> set[str]:
        cached = self._closure_cache.get(file)
        if cached is not None:
            return set(cached)
        seen: set[str] = set()
        visited = {file}
        stack = [file]
        while stack:
            cur = stack.pop()
            for e in self.out_edges(cur):
                if e.dst_file not in visited:
                    visited.add(e.dst_file)
                    seen.add(e.dst_file)
                    stack.append(e.dst_file)
        self._closure_cache[file] = frozenset(seen)
        return set(seen)

    def closure_no_pch(self, file: str) -> set[str]:
        cached = self._closure_no_pch_cache.get(file)
        if cached is not None:
            return set(cached)
        seen: set[str] = set()
        visited = {file}
        stack = [file]
        while stack:
            cur = stack.pop()
            for e in self.out_edges(cur):
                # Never traverse INTO the PCH root: skip this edge entirely
                # (don't add it, don't recurse through it) rather than
                # subtracting closure(pch_root) afterward -- a header
                # reachable through an INDEPENDENT, non-pch-root edge stays
                # in the result even though it's also inside
                # closure(pch_root) (fixture scenario 25).
                if self.pch_root is not None and e.dst_file == self.pch_root:
                    continue
                if e.dst_file not in visited:
                    visited.add(e.dst_file)
                    seen.add(e.dst_file)
                    stack.append(e.dst_file)
        self._closure_no_pch_cache[file] = frozenset(seen)
        return set(seen)


# ---------------------------------------------------------------------------
# Comment stripping (C/C++ // and /* */, string/char literal aware).
# ---------------------------------------------------------------------------


def strip_comments(text: str) -> str:
    """Removes `//` and `/* */` comments, aware of `"..."` and `'...'`
    literals (so a `//` or `/*` inside one isn't misread). Newline count is
    preserved exactly, so line numbers computed against the result still
    point at the original file. Public: reused by types.py."""
    out: list[str] = []
    i = 0
    n = len(text)
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    while i < n:
        c = text[i]
        if in_line_comment:
            if c == "\n":
                in_line_comment = False
                out.append(c)
            i += 1
            continue
        if in_block_comment:
            if c == "*" and i + 1 < n and text[i + 1] == "/":
                in_block_comment = False
                i += 2
                continue
            if c == "\n":
                out.append(c)
            i += 1
            continue
        if in_string:
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            out.append(c)
            i += 1
            continue
        if in_char:
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == "'":
                in_char = False
            out.append(c)
            i += 1
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if c == "'":
            in_char = True
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            in_line_comment = True
            i += 2
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            in_block_comment = True
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


# ---------------------------------------------------------------------------
# Conditional-scope tracking (the #if/#ifdef/#ifndef nesting rule, with the
# top-of-file include-guard exception).
# ---------------------------------------------------------------------------


def compute_conditional_lines(text: str) -> set[int]:
    """Returns the 1-based line numbers that sit inside any
    #if/#ifdef/#ifndef block, EXCEPT a top-of-file #ifndef X / #define X
    include guard (whose own span doesn't count -- only further nesting
    inside it does). `text` should already have comments stripped (so a
    commented-out #if doesn't throw off the nesting)."""
    lines = text.split("\n")
    n = len(lines)

    guard_open_idx: int | None = None
    first_idx: int | None = None
    for i, line in enumerate(lines):
        if line.strip() == "":
            continue
        first_idx = i
        break
    if first_idx is not None:
        m = _GUARD_IFNDEF_RE.match(lines[first_idx])
        if m:
            guard_name = m.group(1)
            for j in range(first_idx + 1, n):
                if lines[j].strip() == "":
                    continue
                m2 = _GUARD_DEFINE_RE.match(lines[j])
                if m2 and m2.group(1) == guard_name:
                    guard_open_idx = first_idx
                break

    stack: list[bool] = []  # True = counts toward conditional depth
    conditional_lines: set[int] = set()
    for i, line in enumerate(lines):
        if _PREPROC_OPEN_PATTERN.match(line):
            counts = i != guard_open_idx
            stack.append(counts)
        elif _PREPROC_CLOSE_PATTERN.match(line):
            if stack:
                stack.pop()
        if any(stack):
            conditional_lines.add(i + 1)
    return conditional_lines


# ---------------------------------------------------------------------------
# Per-file include parsing.
# ---------------------------------------------------------------------------


def _parse_file_includes(text: str) -> list[tuple[int, str, bool]]:
    """Returns [(line, raw, is_conditional), ...] for one file's text."""
    stripped = strip_comments(text)
    conditional_lines = compute_conditional_lines(stripped)
    result: list[tuple[int, str, bool]] = []
    for m in _LOCAL_INCLUDE_PATTERN.finditer(stripped):
        raw = m.group(1)
        line = stripped.count("\n", 0, m.start()) + 1
        result.append((line, raw, line in conditional_lines))
    return result


# ---------------------------------------------------------------------------
# Path join/normalise helper (amendment 4: never by string concatenation).
# ---------------------------------------------------------------------------


def _join_normalise(directory: str, raw: str) -> str:
    joined = raw if not directory else f"{directory}/{raw}"
    return posixpath.normpath(joined).replace("\\", "/")


def _dirname(path: str) -> str:
    d = posixpath.dirname(path)
    return d


# ---------------------------------------------------------------------------
# build()
# ---------------------------------------------------------------------------


def _collect_ancestor_dirs(src: str, parents: dict[str, set[str]]) -> list[str]:
    """BFS over `parents` (dst -> set of files that include it), nearest
    first, returning the deduplicated directories of every file that
    (transitively) includes `src`."""
    order: list[str] = []
    seen_dirs: set[str] = set()
    visited_files = {src}
    queue: deque[str] = deque([src])
    while queue:
        cur = queue.popleft()
        for p in sorted(parents.get(cur, ())):
            if p in visited_files:
                continue
            visited_files.add(p)
            d = _dirname(p)
            if d not in seen_dirs:
                seen_dirs.add(d)
                order.append(d)
            queue.append(p)
    return order


def _resolve_in_dirs(raw: str, dirs: list[str], lower_index: dict[str, str]) -> list[str]:
    candidates: list[str] = []
    seen: set[str] = set()
    for d in dirs:
        candidate = _join_normalise(d, raw)
        canon = lower_index.get(candidate.lower())
        if canon is not None and canon not in seen:
            seen.add(canon)
            candidates.append(canon)
    return candidates


def build(model) -> IncludeGraph:
    repo_root = Path(model.repo_root)

    universe: set[str] = set(model.files.keys()) | set(model.orphans)
    lower_index: dict[str, str] = {}
    for p in universe:
        lower_index.setdefault(p.lower(), p)

    parsed: dict[str, list[tuple[int, str, bool]]] = {}
    for f in universe:
        try:
            text = (repo_root / f).read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            parsed[f] = []
            continue
        parsed[f] = _parse_file_includes(text)

    edges: list[IncludeEdge] = []
    ambiguous: list[dict] = []
    pending: list[tuple[str, int, str, bool]] = []

    def search_dirs_for(src: str) -> list[str]:
        owner = model.files.get(src)
        dirs = [_dirname(src)]
        if owner is not None:
            node = model.nodes.get(owner.node)
            if node is not None:
                dirs.extend(node.include_dirs)
        return dirs

    def make_edge(src: str, line: int, raw: str, dst: str, is_conditional: bool) -> IncludeEdge:
        tags: set[str] = set()
        if is_conditional:
            tags.add("conditional")
        if src.endswith(".Tests.inl") or dst.endswith(".Tests.inl"):
            tags.add("test")
        return IncludeEdge(src_file=src, line=line, raw=raw, dst_file=dst, tags=tags)

    for src in sorted(universe):
        dirs = search_dirs_for(src)
        for line, raw, is_cond in parsed[src]:
            candidates = _resolve_in_dirs(raw, dirs, lower_index)
            if candidates:
                if len(candidates) > 1:
                    ambiguous.append(
                        {"file": src, "line": line, "raw": raw, "candidates": list(candidates)}
                    )
                edges.append(make_edge(src, line, raw, candidates[0], is_cond))
            else:
                pending.append((src, line, raw, is_cond))

    parents: dict[str, set[str]] = {}
    for e in edges:
        parents.setdefault(e.dst_file, set()).add(e.src_file)

    changed = True
    still_pending = pending
    while changed and still_pending:
        changed = False
        next_pending: list[tuple[str, int, str, bool]] = []
        for src, line, raw, is_cond in still_pending:
            dirs = _collect_ancestor_dirs(src, parents)
            candidates = _resolve_in_dirs(raw, dirs, lower_index)
            if candidates:
                if len(candidates) > 1:
                    ambiguous.append(
                        {"file": src, "line": line, "raw": raw, "candidates": list(candidates)}
                    )
                dst = candidates[0]
                edges.append(make_edge(src, line, raw, dst, is_cond))
                parents.setdefault(dst, set()).add(src)
                changed = True
            else:
                next_pending.append((src, line, raw, is_cond))
        still_pending = next_pending

    unresolved: list[tuple[str, int, str]] = [
        (src, line, raw) for (src, line, raw, _is_cond) in still_pending
    ]
    unresolved.sort(key=lambda t: (t[0], t[1], t[2]))

    graph = IncludeGraph(edges=edges, unresolved=unresolved, ambiguous=ambiguous, pch_root=model.pch_root)

    if model.pch_root is not None:
        pch_closure = graph.closure(model.pch_root)
        for e in edges:
            if e.dst_file == model.pch_root or e.src_file == model.pch_root or e.src_file in pch_closure:
                e.tags.add("pch")

    return graph


# ---------------------------------------------------------------------------
# __main__
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    import argparse
    import sys

    import model as model_mod

    parser = argparse.ArgumentParser(description="libgraph include graph")
    parser.add_argument("--config", default="vulkan_vs2022_debug_win64_true")
    parser.add_argument("--repo-root", default=None)
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[2]
    m = model_mod.build_model(repo_root, args.config)
    graph = build(m)

    print(f"edges: {len(graph.edges)}")
    print(f"unresolved: {len(graph.unresolved)}")
    print(f"ambiguous: {len(graph.ambiguous)}")
    for a in graph.ambiguous:
        print(f"  {a['file']}:{a['line']} \"{a['raw']}\" -> {a['candidates']}")
    return 0


if __name__ == "__main__":
    import sys

    sys.exit(main())
