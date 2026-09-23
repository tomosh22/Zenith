"""libgraph's type index: which types each lib's headers declare at
namespace scope, and how outside code uses them (embed/ref/mention).

Built on top of `model.py` (wave 1) and `includes.py` (this wave):
`build(model, graph)` scans every header (`.h`/`.hpp`/`.inl`) owned by a
`kind == "lib"` or `kind == "game"` node for `class`/`struct`/`enum`
declarations plus `using`/`typedef` aliases, then scans every file (headers
and `.cpp`/`.c`, any node kind) for CROSS-NODE uses of those types,
classifying each use by the strongest context found in that file: embed >
ref > mention. See the approved plan
(C:\\Users\\tomos\\.claude\\plans\\i-want-a-way-quirky-mountain.md, pipeline
step 3) and the W2a brief's binding amendments for the exact rules.

Declarations are deliberately NOT collected from `kind == "middleware"`
headers, NOR from any header under a `Middleware/` path segment at any
depth (vendored/third-party trees: freetype, the Vulkan SDK, glm, Jolt,
SDL2, assimp, slang, imgui, ...) -- see `_is_vendor_path`. The report only
ever analyses `lib` surfaces, and those vendored trees reuse short, generic
names (`root`, `base`, `face`, `name`, `int64_t`, ...) across dozens of
their own internal headers -- on the real repo that produced ~4,920
spurious `collisions` entries that were pure vendor-on-vendor noise and
dwarfed the ~50 that actually involve a Zenith/Games header. Both checks
are needed: `freetype`/`msdf-atlas-gen` are their own `kind=="middleware"`
Entry-table/Project classes (caught by the node-kind check alone), but
`assimp` lives at `Tools/Middleware/assimp/**` -- bundled directly into
`ZenithTools`'s OWN file listing (`kind=="lib"`, source root `Tools`), so
only the path check catches it. A `kind == "middleware"` or vendored-path
file can still appear as a USE **consumer** (some Zenith header could
theoretically be included from vendored code), just never as a decl
source.

Note: this module is named `cxxtypes.py`, not `types.py` -- the obvious
name collides with the stdlib `types` module. Tools/libgraph has no
`__init__.py` (flat siblings, per the plan), so `python
Tools/libgraph/<anything>.py` puts Tools/libgraph at the front of
`sys.path`; a sibling literally named `types.py` would then get imported
AS the stdlib `types` module by the first unrelated stdlib import chain
that reaches `from types import ...` (e.g. `argparse` -> `re` -> `enum`),
crashing every script in the directory. Renaming is the fix; this module
carries no sys.path workaround.

This is heuristic, regex-based source scanning (no real C++ parser), and
its known blind spots are:
  - **Declarations**: a macro-wrapped declaration whose macro sits BEFORE
    the class/struct/enum keyword (e.g. a hypothetical `ZENITH_API class
    X`) is not recognised at all -- the brief's binding amendment 6 confirms
    the real repo has no such form, only macros AFTER the name
    (`class X ZENITH_FINAL`), which IS handled. A type declared inside a
    macro expansion invisible to plain text (e.g. `DECLARE_ZENITH_TYPE(X)`)
    is invisible to this scanner too.
  - A local class/struct defined inside a function body, or any declaration
    nested inside a non-namespace, non-class brace (an `if`/`for`/lambda
    body), is excluded from `decls` by the same brace-depth rule that
    excludes class-nested declarations -- namespace scope is `namespace`
    frames only; everything else counts as "not namespace scope" (see
    `_namespace_scope_intervals`).
  - Multiple inheritance and template base classes are handled (base list
    is comma-split and stripped of access-specifiers/`virtual`), but a base
    written through a macro (`MIXIN(X)`) is invisible.
  - A `typedef`'s alias name is extracted with `_typedef_alias_name`, which
    tries the function-pointer form (`typedef Ret (*Name)(Args...);` -- the
    real repo's `Zenith_SaveData.h` shape) and the array form
    (`typedef T Name[N];`) before falling back to "last identifier before
    the `;`" for the simple `typedef T Name;` case. A typedef'd array-of-
    function-pointers, or any other compound C declarator not one of these
    three shapes, still falls through to the "last identifier" heuristic
    and can extract the wrong token (unobserved on the real repo).
  - `name: str` is a BARE identifier with no C++ namespace qualification --
    two same-named types in two different `namespace` blocks (or one
    namespaced and one global) collide as a single name. Observed on the
    real repo: `Zenith_Maths::Quat` (`Zenith/Maths/Zenith_Maths.h`) and
    DevilsPlayground's own global-scope `using Quat = Zenith_Maths::Quat;`
    re-export (`Games/DevilsPlayground/Source/DPCommonTypes.h`) collide.
    The lib-before-game processing order (see `build()`'s `header_paths`
    sort) means the LIB's `Quat` wins the canonical decl here -- a game
    only ever re-exports or consumes engine types, so it must never shadow
    one in the surface report -- but a lib-vs-LIB namespace collision (two
    different libs' own namespaced types sharing a bare name) is still
    resolved by path order alone and can still mis-attribute.
  - **Uses**: the ref/embed classifiers are line-local regex heuristics,
    not real type resolution. A pointer-to-pointer (`X**`), a reference to
    a pointer (`X*&`), or a type used only as a template's SECOND-OR-LATER
    argument's nested argument (`Foo<int, std::vector<X>>`) may
    misclassify or miss entirely; the brief only requires `<X>`, `<X,`,
    `, X>` forms. A `typedef`'d alias used as a type name works (aliases
    are ordinary `decls` entries), but a use of the alias's UNDERLYING type
    spelled out directly is obviously not attributed to the alias.
  - A name collision between a local variable/parameter and a tracked type
    name (shadowing) is not detected -- any token matching a known type
    name is treated as a use of that type, per the brief's own use-vs-decl
    node/closure guard (which exists precisely to keep this from firing on
    same-node incidental collisions, not on shadowing within the same
    unrelated file).
  - The `<X>`/`<X,`/`,X>` template-argument scan runs over `<`/`,`
    characters generically, so a comparison/shift expression that happens
    to bracket a known type name (vanishingly unlikely given this
    codebase's type-name shapes) could false-positive.
  - A name declared multiple times in the SAME header behind different
    `#ifdef` branches (no preprocessor is run) is reported as a spurious
    self-collision against its own declaring header -- observed on the
    real repo for `Zenith_BackendAliases.h`'s backend-conditional aliases.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

import includes

_HEADER_EXTS = {".h", ".hpp", ".inl"}
_USE_SCAN_EXTS = _HEADER_EXTS | {".cpp", ".c"}
_DECL_KINDS = {"lib", "game"}
# Canonical-decl tie-break: a lib's declaration always wins over a same-
# named game declaration, regardless of path order (see build()'s
# header_paths sort key).
_DECL_KIND_RANK = {"lib": 0, "game": 1}

_IDENTIFIER_RE = re.compile(r"\b[A-Za-z_]\w*\b")

_TEMPLATE_KEYWORD_RE = re.compile(r"\btemplate\b")

_ENUM_FULL_RE = re.compile(r"\benum(?:\s+(class|struct))?\s+(\w+)\s*(?::[^{;]*)?\s*\{")
_ENUM_FWD_RE = re.compile(r"\benum(?:\s+(class|struct))?\s+(\w+)\s*(?::[^{;]*)?\s*;")

_CLASS_STRUCT_FULL_RE = re.compile(
    r"\b(class|struct)\s+(\w+)"
    r"(?:\s+(?:final|ZENITH_\w+))*"
    r"\s*(?::\s*([^{;]*))?"
    r"\s*\{"
)
_CLASS_STRUCT_FWD_RE = re.compile(r"\b(class|struct)\s+(\w+)\s*;")

_USING_ALIAS_RE = re.compile(r"\busing\s+(\w+)\s*=")
_TYPEDEF_RE = re.compile(r"\btypedef\b([^;]*);")
# A typedef's alias name isn't always the last identifier before the `;` --
# that's only true for the simple `typedef <type> Name;` form. A
# function-pointer typedef (`typedef void(*Name)(Args...);`, the real
# repo's SaveData.h shape) has its LAST identifier inside the parameter
# list (a param name, e.g. "pxUserData"), not the alias itself, so these
# are tried in order before falling back to "last identifier":
#   1. a function-pointer form: `(*Name)(` -- Name is right after the `*`.
#   2. a plain function-typedef form (rare): `Name(` with no `*` before it.
#   3. an array typedef: `Name[`.
_TYPEDEF_FUNCPTR_RE = re.compile(r"\(\s*(?:__cdecl\s+|__stdcall\s+)?\*\s*(\w+)\s*\)\s*\(")
_TYPEDEF_FUNC_RE = re.compile(r"(\w+)\s*\(")
_TYPEDEF_ARRAY_RE = re.compile(r"(\w+)\s*\[")


def _typedef_alias_name(body: str) -> str | None:
    m = _TYPEDEF_FUNCPTR_RE.search(body)
    if m:
        return m.group(1)
    m = _TYPEDEF_ARRAY_RE.search(body)
    if m:
        return m.group(1)
    m = _TYPEDEF_FUNC_RE.search(body)
    if m:
        return m.group(1)
    names = re.findall(r"[A-Za-z_]\w*", body)
    return names[-1] if names else None

# Uses.
_MEMBER_DECL_RE = re.compile(r"\b([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(?:;|=|\{|\[)")
_REF_RE = re.compile(r"\b([A-Za-z_]\w*)\b\s*(?:const\s*)?[*&]")
_TEMPLATE_ARG_RE = re.compile(r"[<,]\s*(\w+)\s*([*&]?)\s*(?=[,>])")

_BASE_QUALIFIER_RE = re.compile(r"^(?:public|private|protected|virtual)\s+")


# ---------------------------------------------------------------------------
# Contract dataclasses.
# ---------------------------------------------------------------------------


@dataclass
class TypeDecl:
    name: str
    header: str
    node: str
    kind: str  # "class" | "struct" | "enum" | "alias"
    forward: bool


@dataclass
class TypeUse:
    consumer_file: str
    type: str
    header: str
    mode: str  # "embed" | "ref" | "mention"


@dataclass
class TypeIndex:
    decls: dict[str, TypeDecl]
    forward_decls: list[TypeDecl]
    collisions: dict[str, list[str]]
    uses: list[TypeUse] = field(default_factory=list)

    def uses_of(self, type_name: str) -> list[TypeUse]:
        return [u for u in self.uses if u.type == type_name]

    def uses_in(self, consumer_file: str) -> list[TypeUse]:
        return [u for u in self.uses if u.consumer_file == consumer_file]


# ---------------------------------------------------------------------------
# Declaration scanning.
# ---------------------------------------------------------------------------


def _namespace_scope_intervals(text: str) -> list[tuple[int, int]]:
    """Half-open [start,end) offset ranges where brace-depth is 0, i.e. NOT
    nested inside any class/struct/enum/function/other brace. Entering
    `namespace X {` (or an anonymous `namespace {`) does not count toward
    depth -- only non-namespace braces do."""
    intervals: list[tuple[int, int]] = []
    stack: list[bool] = []  # True = namespace frame (non-counting)
    depth = 0
    at_zero_since = 0
    n = len(text)
    ns_re = re.compile(r"\bnamespace\b(\s+[\w:]+)?\s*$")
    i = 0
    while i < n:
        c = text[i]
        if c == "{":
            j = i - 1
            while j >= 0 and text[j] in " \t\r\n":
                j -= 1
            k = j
            while k >= 0 and text[k] not in ";{}":
                k -= 1
            preceding = text[k + 1 : j + 1]
            is_ns = bool(ns_re.search(preceding))
            was_zero = depth == 0
            stack.append(is_ns)
            if not is_ns:
                depth += 1
            if was_zero and depth != 0:
                intervals.append((at_zero_since, i))
            i += 1
            continue
        if c == "}":
            was_zero = depth == 0
            if stack:
                is_ns = stack.pop()
                if not is_ns:
                    depth = max(0, depth - 1)
            if depth == 0 and not was_zero:
                at_zero_since = i + 1
            i += 1
            continue
        i += 1
    if depth == 0:
        intervals.append((at_zero_since, n))
    return intervals


def _pos_in_intervals(pos: int, intervals: list[tuple[int, int]]) -> bool:
    lo, hi = 0, len(intervals)
    while lo < hi:
        mid = (lo + hi) // 2
        a, b = intervals[mid]
        if pos < a:
            hi = mid
        elif pos >= b:
            lo = mid + 1
        else:
            return True
    return False


def _blank_template_param_lists(text: str) -> str:
    """Blanks the interior of every top-level `template<...>` parameter
    list (keeping newlines), so `template<class T>` never lets the
    declaration scanner mistake the template-parameter synonym `class T`
    for a real type declaration. Does NOT touch template ARGUMENTS at use
    sites (`std::vector<X>`) -- callers only apply this to the text used
    for declaration scanning, never to the text used for use scanning."""
    out = list(text)
    n = len(text)
    for m in _TEMPLATE_KEYWORD_RE.finditer(text):
        j = m.end()
        while j < n and text[j] in " \t\r\n":
            j += 1
        if j >= n or text[j] != "<":
            continue
        depth = 0
        k = j
        while k < n:
            if text[k] == "<":
                depth += 1
            elif text[k] == ">":
                depth -= 1
                if depth == 0:
                    k += 1
                    break
            k += 1
        for idx in range(j, k):
            if out[idx] != "\n":
                out[idx] = " "
    return "".join(out)


def _extract_base_identifiers(base_text: str) -> list[str]:
    result: list[str] = []
    for seg in base_text.split(","):
        seg = seg.strip()
        # Strip up to two leading qualifiers, e.g. "public virtual X".
        for _ in range(2):
            new_seg = _BASE_QUALIFIER_RE.sub("", seg)
            if new_seg == seg:
                break
            seg = new_seg
        m = re.match(r"[A-Za-z_]\w*", seg)
        if m:
            result.append(m.group(0))
    return result


def _scan_declarations(text: str) -> list[dict]:
    """Scans comment-stripped, template-param-blanked text for namespace-
    scope class/struct/enum decls (+forward decls) and using/typedef
    aliases. Returns a list of dicts: name, kind, forward, bases (only
    meaningful for class/struct full decls, used for base-class embed
    detection)."""
    intervals = _namespace_scope_intervals(text)
    entries: list[dict] = []

    # Enums first, then blank their header spans, so "enum class X {" never
    # also matches the class/struct regexes below (which would otherwise
    # see the literal substring "class X {").
    work = list(text)
    for m in _ENUM_FULL_RE.finditer(text):
        if _pos_in_intervals(m.start(), intervals):
            entries.append({"name": m.group(2), "kind": "enum", "forward": False, "bases": []})
        for idx in range(m.start(), m.end()):
            if work[idx] != "\n":
                work[idx] = " "
    for m in _ENUM_FWD_RE.finditer(text):
        if _pos_in_intervals(m.start(), intervals):
            entries.append({"name": m.group(2), "kind": "enum", "forward": True, "bases": []})
        for idx in range(m.start(), m.end()):
            if work[idx] != "\n":
                work[idx] = " "
    text2 = "".join(work)

    for m in _CLASS_STRUCT_FULL_RE.finditer(text2):
        if _pos_in_intervals(m.start(), intervals):
            bases = _extract_base_identifiers(m.group(3) or "")
            entries.append({"name": m.group(2), "kind": m.group(1), "forward": False, "bases": bases})
    for m in _CLASS_STRUCT_FWD_RE.finditer(text2):
        if _pos_in_intervals(m.start(), intervals):
            entries.append({"name": m.group(2), "kind": m.group(1), "forward": True, "bases": []})
    for m in _USING_ALIAS_RE.finditer(text):
        if _pos_in_intervals(m.start(), intervals):
            entries.append({"name": m.group(1), "kind": "alias", "forward": False, "bases": []})
    for m in _TYPEDEF_RE.finditer(text):
        if _pos_in_intervals(m.start(), intervals):
            name = _typedef_alias_name(m.group(1))
            if name:
                entries.append({"name": name, "kind": "alias", "forward": False, "bases": []})
    return entries


# ---------------------------------------------------------------------------
# build()
# ---------------------------------------------------------------------------


def _ext(path: str) -> str:
    dot = path.rfind(".")
    return path[dot:].lower() if dot != -1 else ""


def _read(repo_root: Path, path: str) -> str:
    try:
        return (repo_root / path).read_text(encoding="utf-8-sig", errors="replace")
    except OSError:
        return ""


def _node_kind(model, node_name: str | None) -> str | None:
    if node_name is None:
        return None
    node = model.nodes.get(node_name)
    return node.kind if node is not None else None


def _is_vendor_path(path: str) -> bool:
    """True for anything under a `Middleware/` directory, at ANY depth --
    not just the repo-root `Middleware/` tree that model.py's own ownership
    routing (`_match_middleware_owner`) handles, but also a vendored tree
    bundled a level deeper inside a real lib's own source root, e.g.
    `Tools/Middleware/assimp/**`. `ZenithTools` (a genuine `kind=="lib"`
    Entry-table project, source root `Tools`) lists assimp's headers
    directly in its own vcxproj, so they come back `kind=="lib"` from the
    node-kind check alone -- the node-kind filter and this path check are
    both needed to actually exclude every vendored tree the plan and the
    orchestrator's review named (freetype/msdf-atlas-gen ARE their own
    `kind=="middleware"` Entry-table/Project classes and would already be
    caught by the node-kind filter alone; assimp would not)."""
    return "Middleware" in path.split("/")


def build(model, graph: includes.IncludeGraph) -> TypeIndex:
    repo_root = Path(model.repo_root)

    # Declarations are only collected from lib/game-owned, non-vendored
    # headers -- see the module docstring and _is_vendor_path: middleware/
    # vendored trees are pure noise for a report that only ever analyses
    # lib surfaces.
    #
    # Processing order is the tie-break for "first name wins the canonical
    # decl, the rest go to collisions": (lib before game), THEN path. Games
    # only re-export or consume engine types (a `using Quat = Zenith_Maths::
    # Quat;` re-export is the real-repo shape, in
    # Games/DevilsPlayground/Source/DPCommonTypes.h), so a game's own decl
    # must never shadow a same-named lib type in the surface report just
    # because its path happens to sort first alphabetically ("Games/..." <
    # "Zenith/..."). Within one kind, path order is unchanged.
    header_paths = sorted(
        (
            p
            for p in model.files
            if _ext(p) in _HEADER_EXTS
            and _node_kind(model, model.files[p].node) in _DECL_KINDS
            and not _is_vendor_path(p)
        ),
        key=lambda p: (_DECL_KIND_RANK[_node_kind(model, model.files[p].node)], p),
    )

    decls: dict[str, TypeDecl] = {}
    forward_decls: list[TypeDecl] = []
    collisions: dict[str, list[str]] = {}

    for header in header_paths:
        text = _read(repo_root, header)
        stripped = includes.strip_comments(text)
        decl_text = _blank_template_param_lists(stripped)
        owner = model.files[header].node
        for e in _scan_declarations(decl_text):
            name = e["name"]
            if e["forward"]:
                forward_decls.append(
                    TypeDecl(name=name, header=header, node=owner, kind=e["kind"], forward=True)
                )
                continue
            if name in decls:
                collisions.setdefault(name, []).append(header)
            else:
                decls[name] = TypeDecl(name=name, header=header, node=owner, kind=e["kind"], forward=False)

    decl_names = set(decls.keys())
    # Uses are scanned in EVERY file (any node kind) -- a lib type could in
    # principle be referenced from a game, a middleware shim, or a proof
    # project; only the DECLARING side is restricted to lib/game.
    consumer_paths = sorted(p for p in model.files if _ext(p) in _USE_SCAN_EXTS)

    uses: list[TypeUse] = []
    for file in consumer_paths:
        text = _read(repo_root, file)
        stripped = includes.strip_comments(text)
        tokens = set(_IDENTIFIER_RE.findall(stripped))
        candidates = tokens & decl_names
        if not candidates:
            continue

        file_node = model.files[file].node
        file_closure = graph.closure(file)
        direct = {e.dst_file for e in graph.out_edges(file)}
        is_header = _ext(file) in _HEADER_EXTS

        decl_text = _blank_template_param_lists(stripped)
        base_embeds: set[str] = set()
        for e in _scan_declarations(decl_text):
            if e["forward"]:
                continue
            for b in e.get("bases", ()):
                if b in candidates:
                    base_embeds.add(b)

        member_embeds: set[str] = set()
        if is_header:
            for m in _MEMBER_DECL_RE.finditer(stripped):
                name = m.group(1)
                if name in candidates:
                    member_embeds.add(name)

        template_embeds: set[str] = set()
        template_refs: set[str] = set()
        for m in _TEMPLATE_ARG_RE.finditer(stripped):
            name = m.group(1)
            if name not in candidates:
                continue
            if m.group(2) in ("*", "&"):
                template_refs.add(name)
            else:
                template_embeds.add(name)

        plain_refs: set[str] = set()
        for m in _REF_RE.finditer(stripped):
            name = m.group(1)
            if name in candidates:
                plain_refs.add(name)

        for type_name in sorted(candidates):
            decl = decls[type_name]
            if decl.node == file_node:
                continue
            if not (decl.header in file_closure or decl.header in direct):
                continue
            if type_name in base_embeds or type_name in member_embeds or type_name in template_embeds:
                mode = "embed"
            elif type_name in plain_refs or type_name in template_refs:
                mode = "ref"
            else:
                mode = "mention"
            uses.append(TypeUse(consumer_file=file, type=type_name, header=decl.header, mode=mode))

    return TypeIndex(decls=decls, forward_decls=forward_decls, collisions=collisions, uses=uses)


# ---------------------------------------------------------------------------
# __main__
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    import argparse

    import model as model_mod

    parser = argparse.ArgumentParser(description="libgraph type index")
    parser.add_argument("--config", default="vulkan_vs2022_debug_win64_true")
    parser.add_argument("--repo-root", default=None)
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[2]
    m = model_mod.build_model(repo_root, args.config)
    graph = includes.build(m)
    idx = build(m, graph)

    print(f"decls: {len(idx.decls)}")
    print(f"forward_decls: {len(idx.forward_decls)}")
    print(f"collisions: {len(idx.collisions)}")
    print(f"uses: {len(idx.uses)}")
    return 0


if __name__ == "__main__":
    import sys

    sys.exit(main())
