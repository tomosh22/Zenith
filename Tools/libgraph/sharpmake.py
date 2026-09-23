"""Sharpmake `.cs` scanning for libgraph.

Discovers every `class X : Y` declaration under `Build/*.cs`, strips
comments first (so a comment-only mention of a class name never produces a
node or an edge), and extracts what `model.py` needs to build nodes and
declared edges: the Sharpmake `Name`/`SourceRootPath`, the ordered
`AddPublicDependency<...>(target)` calls (with `RenderBackend` conditional
detection), the `ZenithEngineLibs.All` entry table (for the lib/middleware
kind split and the aggregate expansion), and the `GameProject` base's
inherited `AddPublicDependency<ZenithProject>` edge.

Python 3.12 stdlib only. Flat sibling module: no package, no relative import.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

# Known abstract/framework base-class names that decide kind by themselves.
_PROJECT_BASE = "Project"
_GAME_BASE = "GameProject"
_ZENITHBASE = "ZenithBaseProject"
_SOLUTION_BASES = {"Solution", "GameSolution", "ZenithEngineSolution"}

_CLASS_RE = re.compile(
    r"\b(?P<abstract>abstract\s+)?class\s+(?P<name>\w+)\s*(?::\s*(?P<base>\w+))?"
)
_NAME_RE = re.compile(r'\bName\s*=\s*"([^"]+)"')
_GAME_NAME_RE = re.compile(r'\bGameName\s*=>\s*"([^"]+)"')
_SOURCE_ROOT_RE = re.compile(r'\bSourceRootPath\s*=\s*RootPath\s*\+\s*"([^"]+)"')
_ADDITIONAL_ROOT_RE = re.compile(
    r'\bAdditionalSourceRootPaths\.Add\(RootPath\s*\+\s*"([^"]+)"\)'
)
_ADD_DEP_RE = re.compile(r"\bAddPublicDependency<(\w+)>\(target\)")
_RENDERBACKEND_IF_RE = re.compile(
    r"((?:else\s+)?if)\s*\(\s*target\.RenderBackend\s*==\s*RenderBackend\.(\w+)\s*\)\s*\{"
)
_ENTRY_RE = re.compile(r"new Entry\(typeof\((\w+)\)")
_AGGREGATE_CALL_RE = re.compile(r"AddAllAsDependenciesOfAggregate\(conf,\s*target\)")


def strip_comments(text: str) -> str:
    """Remove `//` and `/* */` comments, careful not to strip inside string
    literals (plain `"..."` or verbatim `@"..."`). Newline count is
    preserved exactly, so line numbers computed against the stripped text
    still point at the original file."""
    out: list[str] = []
    i = 0
    n = len(text)
    in_line_comment = False
    in_block_comment = False
    in_string = False
    verbatim = False
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
            if verbatim:
                if c == '"':
                    if i + 1 < n and text[i + 1] == '"':
                        out.append('"')
                        i += 2
                        continue
                    in_string = False
                    out.append(c)
                    i += 1
                    continue
                out.append(c)
                i += 1
                continue
            else:
                if c == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if c == '"':
                    in_string = False
                out.append(c)
                i += 1
                continue
        # Not in a string/comment.
        if c == "@" and i + 1 < n and text[i + 1] == '"':
            in_string = True
            verbatim = True
            out.append(c)
            out.append('"')
            i += 2
            continue
        if c == '"':
            in_string = True
            verbatim = False
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


def _line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _find_matching_brace(text: str, open_brace_idx: int) -> int:
    """text[open_brace_idx] must be '{'. Returns index of the matching '}'."""
    depth = 0
    i = open_brace_idx
    n = len(text)
    while i < n:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n - 1


@dataclass
class DepRef:
    dst_class: str
    conditional_backend: str | None  # e.g. "Vulkan" / "D3D12" / "Null", or None
    order: int
    line: int


@dataclass
class ClassInfo:
    name: str
    file: str  # repo-relative, forward slashes
    is_abstract: bool
    base: str | None
    proj_name: str | None = None
    game_name: str | None = None
    source_roots: list[str] = field(default_factory=list)
    deps: list[DepRef] = field(default_factory=list)
    is_aggregate_expansion: bool = False
    kind: str | None = None  # filled in by classify()


@dataclass
class ScanResult:
    classes: dict[str, ClassInfo]
    entry_order: list[str]  # class names, in Entry.All order
    entry_table_file: str | None
    games_base: ClassInfo | None  # the abstract GameProject class info
    games_base_file: str | None


def _extract_class_info(
    name: str,
    is_abstract: bool,
    base: str | None,
    body: str,
    body_offset: int,
    rel_file: str,
    full_text: str,
) -> ClassInfo:
    info = ClassInfo(name=name, file=rel_file, is_abstract=is_abstract, base=base)

    m = _NAME_RE.search(body)
    if m:
        info.proj_name = m.group(1)

    m = _GAME_NAME_RE.search(body)
    if m:
        info.game_name = m.group(1)
        info.proj_name = m.group(1)
        info.source_roots = ["Games/" + m.group(1)]

    m = _SOURCE_ROOT_RE.search(body)
    if m:
        root = m.group(1)
        if root.startswith("/"):
            root = root[1:]
        info.source_roots = [root] + info.source_roots

    for m in _ADDITIONAL_ROOT_RE.finditer(body):
        root = m.group(1)
        if root.startswith("/"):
            root = root[1:]
        info.source_roots.append(root)

    if _AGGREGATE_CALL_RE.search(body):
        info.is_aggregate_expansion = True

    # RenderBackend conditional spans, in body-relative offsets.
    spans: list[tuple[int, int, str]] = []
    for m in _RENDERBACKEND_IF_RE.finditer(body):
        brace_idx = m.end() - 1
        close_idx = _find_matching_brace(body, brace_idx)
        spans.append((m.start(), close_idx, m.group(2)))

    order = 0
    for m in _ADD_DEP_RE.finditer(body):
        dst = m.group(1)
        offset = m.start()
        backend = None
        for start, end, name_ in spans:
            if start <= offset <= end:
                backend = name_
                break
        abs_offset = body_offset + offset
        info.deps.append(
            DepRef(
                dst_class=dst,
                conditional_backend=backend,
                order=order,
                line=_line_of(full_text, abs_offset),
            )
        )
        order += 1

    return info


def scan(repo_root: Path) -> ScanResult:
    """Scan Build/*.cs for class declarations."""
    repo_root = Path(repo_root).resolve()
    build_dir = repo_root / "Build"
    classes: dict[str, ClassInfo] = {}
    entry_order: list[str] = []
    entry_table_file: str | None = None
    games_base: ClassInfo | None = None
    games_base_file: str | None = None

    if not build_dir.is_dir():
        return ScanResult(classes, entry_order, entry_table_file, games_base, games_base_file)

    for cs_file in sorted(build_dir.glob("*.cs"), key=lambda p: p.name):
        raw = cs_file.read_text(encoding="utf-8-sig")
        stripped = strip_comments(raw)
        rel_file = str(cs_file.relative_to(repo_root)).replace("\\", "/")

        if "Entry[] All" in stripped or "readonly Entry[] All" in stripped:
            found = _ENTRY_RE.findall(stripped)
            if found:
                entry_order = found
                entry_table_file = rel_file

        for m in _CLASS_RE.finditer(stripped):
            base = m.group("base")
            if base is None:
                continue
            name = m.group("name")
            is_abstract = bool(m.group("abstract"))
            brace_idx = stripped.find("{", m.end())
            if brace_idx == -1:
                continue
            close_idx = _find_matching_brace(stripped, brace_idx)
            body = stripped[brace_idx : close_idx + 1]
            info = _extract_class_info(
                name, is_abstract, base, body, brace_idx, rel_file, stripped
            )
            classes[name] = info

            if name == "GameProject" and base == _ZENITHBASE:
                games_base = info
                games_base_file = rel_file

    return ScanResult(classes, entry_order, entry_table_file, games_base, games_base_file)


def classify(scan_result: ScanResult) -> None:
    """Fill in ClassInfo.kind for every scanned class, in place.

    kind is one of: "lib", "middleware", "game", "proof", or None (ignored:
    abstract classes, Solution-family classes, or an unrecognised base).
    """
    entry_set = set(scan_result.entry_order)
    for info in scan_result.classes.values():
        if info.is_abstract:
            info.kind = None
            continue
        if info.base in _SOLUTION_BASES:
            info.kind = None
            continue
        if info.base == _PROJECT_BASE:
            info.kind = "middleware"
        elif info.base == _GAME_BASE:
            info.kind = "game"
        elif info.base == _ZENITHBASE:
            if info.name in entry_set:
                # Entry-table membership alone decides lib vs middleware:
                # under Middleware/ -> middleware, anything else (Zenith/**
                # AND a repo-root dir like ZenithToolsProject's "Tools") ->
                # lib. Real repo: 30 libs + ZenithJolt/ZenithImGui
                # (middleware) + ZenithTools (lib, source root "Tools") = 33
                # Entry-table entries.
                root = info.source_roots[0] if info.source_roots else ""
                if root.startswith("Middleware/"):
                    info.kind = "middleware"
                else:
                    info.kind = "lib"
            else:
                info.kind = "proof"
        else:
            info.kind = None
