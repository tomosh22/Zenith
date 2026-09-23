"""libgraph's shared model: dataclasses + the build_model() pipeline.

Ties vcxproj.py (MSBuild XML facts) and sharpmake.py (declared-edge facts)
together into one `Model` for a chosen `--config`, plus the allowlist
loader and the tree-walk reconciliation (orphans/duplicates/middleware
routing) described in the approved plan
(C:\\Users\\tomos\\.claude\\plans\\i-want-a-way-quirky-mountain.md).

Run as a script from the repo root:
    python Tools/libgraph/model.py [--config <cfg>] [--repo-root <dir>] [--json]

Python 3.12 stdlib only. Flat sibling module: no package, no relative import
(vcxproj/sharpmake are imported as plain top-level modules).
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

import sharpmake
import vcxproj

_SOURCE_EXTS = {".h", ".hpp", ".inl", ".cpp", ".c"}

# Tree-walk exclusions (orphan/duplicate reconciliation) -- OUR rule, see the
# W1 brief's "Tree walk for orphans" bullet.
_SKIP_SEGMENTS = {
    "obj",
    "output",
    ".vs",
    "dist",
    ".claude",
    ".codex_tmp",
    "node_modules",
    ".git",
    "Middleware",
}
_SKIP_PREFIXES = (
    "Build/artifacts",
    "Tools/Vault",
    # libgraph's own test fixture tree ships a miniature Zenith checkout
    # (its own Zenith/Games/Tests/Tools dirs) nested under the real repo's
    # Tools/ -- a real-repo orphan/duplicate walk must not descend into it
    # (see vcxproj.py's matching _SKIP_FIXTURES_RE for the vcxproj side).
    "Tools/libgraph/tests/fixtures",
)
# Gitignored Android/Gradle build intermediates (.gitignore:159-162): not part
# of the tracked tree, so they must not surface as orphans even though the
# W1 brief's segment list (written before a local Android build populated
# these directories) doesn't name them explicitly. Mirrors
# ZenithBaseProject's own SourceFilesExcludeRegex (`\Android\app\build\`,
# `\Android\.gradle\`) in Build/Sharpmake_Common.cs.
_SKIP_SUBSTRINGS = ("Android/app/build/", "Android/.gradle/", "Android/build/")
_SOURCE_TREE_DIRS = ("Zenith", "Games", "Tests", "Tools")


# ---------------------------------------------------------------------------
# The contract dataclasses (field names/order pinned by the W1 brief -- do
# not rename).
# ---------------------------------------------------------------------------


@dataclass
class Node:
    name: str
    kind: str  # "lib" | "middleware" | "game" | "proof"
    vcxproj: str | None = None
    out_lib: str | None = None
    int_dir: str | None = None
    include_dirs: list[str] = field(default_factory=list)
    pch_header: str | None = None
    function_level_linking: bool | None = None
    source_roots: list[str] = field(default_factory=list)
    subject: str | None = None


@dataclass
class FileOwner:
    path: str
    node: str
    how: str  # "listed" | "dir-fallback"
    duplicate_of: list[str] = field(default_factory=list)


@dataclass
class DeclaredEdge:
    src: str
    dst: str
    conditional: str | None
    origin_file: str
    order: int
    # True only for an edge produced by expanding
    # ZenithEngineLibs.AddAllAsDependenciesOfAggregate (the aggregate "Zenith"
    # -> every Entry-table lib). False for every literal
    # AddPublicDependency<X> edge, including the games' inherited
    # <ZenithProject> edge -- the aggregate "declares" everything, so
    # metrics must be able to tell that apart from a genuine, file-level
    # declared dependency (otherwise the aggregate's real link-only /
    # undeclared structure gets hidden behind a wall of trivially-true
    # "declared" edges).
    via_aggregate: bool = False


@dataclass
class Model:
    config: str
    repo_root: str
    nodes: dict[str, Node]
    files: dict[str, FileOwner]
    declared: list[DeclaredEdge]
    orphans: list[str]
    duplicates: list[str]
    allowlist: dict[str, str | None]
    pch_root: str | None
    unresolved_conditions: list[str]
    absent_nodes: list[str]
    # Scanned Sharpmake classes with NO matching vcxproj at all (neither
    # win64 nor agde) -- one readable string per class: its name, its
    # Sharpmake Name, and the .cs file it's declared in. Never raises; this
    # is what a new .cs added without running Build\regen.ps1 looks like.
    # Defaulted (unlike this dataclass's other fields) so existing
    # `Model(...)` call sites in other waves' hand-built test fixtures --
    # which predate this field -- keep constructing without needing an
    # edit; build_model() itself always passes it explicitly.
    missing_vcxproj: list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Allowlist loader.
# ---------------------------------------------------------------------------

_KIND_PREFIX_RE = re.compile(r"^([\w-]+):\s*(.*)$")


def _split_kv(line: str) -> tuple[str, str] | None:
    if " => " not in line:
        return None
    left, right = line.split(" => ", 1)
    return left.strip(), right.strip()


def load_allowlist(repo_root: Path) -> dict[str, str | None]:
    """Reads Tools/layering_allowlist.txt, Tools/architecture_allowlist.txt
    and Tools/*_leaf_allowlist.txt into one normalised key -> kind map.

    Key = f"{src_file} => {include.replace('\\\\', '/')}" (matches the
    layering gate's own key format). Missing files are fine (empty)."""
    repo_root = Path(repo_root)
    tools_dir = repo_root / "Tools"
    result: dict[str, str | None] = {}

    def add(src: str, include: str, kind: str | None) -> None:
        key = f"{src} => {include.replace(chr(92), '/')}"
        result[key] = kind

    def read_plain(path: Path) -> None:
        if not path.is_file():
            return
        for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            kv = _split_kv(line)
            if kv is None:
                continue
            add(kv[0], kv[1], None)

    def read_architecture(path: Path) -> None:
        if not path.is_file():
            return
        for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            kind: str | None = None
            rest = line
            m = _KIND_PREFIX_RE.match(line)
            if m:
                kind = m.group(1)
                rest = m.group(2)
            kv = _split_kv(rest)
            if kv is None:
                continue
            add(kv[0], kv[1], kind)

    read_plain(tools_dir / "layering_allowlist.txt")
    read_architecture(tools_dir / "architecture_allowlist.txt")
    if tools_dir.is_dir():
        for leaf_file in sorted(tools_dir.glob("*_leaf_allowlist.txt")):
            read_plain(leaf_file)

    return result


# ---------------------------------------------------------------------------
# build_model()
# ---------------------------------------------------------------------------


def _to_repo_relative(p: Path, repo_root: Path) -> str:
    try:
        return str(p.resolve().relative_to(repo_root)).replace("\\", "/")
    except ValueError:
        return str(p.resolve()).replace("\\", "/")


def _backend_prefix(config: str) -> str | None:
    lower = config.lower()
    if lower.startswith("vulkan_"):
        return "Vulkan"
    if lower.startswith("null_"):
        return "Null"
    if lower.startswith("d3d12_"):
        return "D3D12"
    return None


def _match_middleware_owner(
    path: str,
    middleware_nodes: list[Node],
    synthesized: dict[str, Node],
) -> str:
    best_name: str | None = None
    best_len = -1
    for n in middleware_nodes:
        for root in n.source_roots:
            if path == root or path.startswith(root + "/"):
                if len(root) > best_len:
                    best_len = len(root)
                    best_name = n.name
    if best_name is not None:
        return best_name

    parts = path.split("/")
    dir_name = parts[1] if len(parts) >= 2 and parts[0] == "Middleware" else parts[0]
    if dir_name not in synthesized:
        synthesized[dir_name] = Node(
            name=dir_name,
            kind="middleware",
            vcxproj=None,
            source_roots=[f"Middleware/{dir_name}"],
        )
    return dir_name


def _iter_source_files(root_dir: Path, repo_root: Path, skip_middleware_seg: bool):
    skip_segments = set(_SKIP_SEGMENTS)
    if not skip_middleware_seg:
        skip_segments.discard("Middleware")
    for p in sorted(root_dir.rglob("*")):
        if not p.is_file():
            continue
        if p.suffix.lower() not in _SOURCE_EXTS:
            continue
        rel_str = _to_repo_relative(p, repo_root)
        parts = rel_str.split("/")
        if any(seg in skip_segments for seg in parts):
            continue
        if any(rel_str.startswith(pre) for pre in _SKIP_PREFIXES):
            continue
        if any(sub in rel_str for sub in _SKIP_SUBSTRINGS):
            continue
        yield rel_str


def build_model(repo_root: str | Path, config: str = "vulkan_vs2022_debug_win64_true") -> Model:
    repo_root = Path(repo_root).resolve()

    scan_result = sharpmake.scan(repo_root)
    sharpmake.classify(scan_result)

    # -- Nodes + vcxproj join ------------------------------------------------
    proj_name_to_class: dict[str, sharpmake.ClassInfo] = {
        info.proj_name: info
        for info in scan_result.classes.values()
        if info.kind is not None and info.proj_name
    }

    def _parse_and_validate(paths: list[Path]) -> dict[str, vcxproj.VcxprojFile]:
        out: dict[str, vcxproj.VcxprojFile] = {}
        for path in paths:
            parsed = vcxproj.parse_vcxproj(path, config, repo_root)
            if parsed.project_name is None:
                continue
            if parsed.project_name not in proj_name_to_class:
                raise ValueError(
                    f"{_to_repo_relative(path, repo_root)}: <ProjectName>{parsed.project_name}"
                    f"</ProjectName> matches no known Sharpmake class -- run Build\\regen.ps1 "
                    f"to regenerate."
                )
            out[parsed.project_name] = parsed
        return out

    parsed_by_projname = _parse_and_validate(vcxproj.discover_vcxproj_files(repo_root))
    # agde-only classes (e.g. ZenithAndroid, ZenithRenderBackendVulkanPlatform-
    # Android -- restricted to Platform.agde in their own AddTargets) have no
    # *_win64.vcxproj at all. Parsed for OWNERSHIP ONLY: has_config naturally
    # stays False for every agde vcxproj under a win64 `config` (its
    # Configuration tokens never match), so the per-config fields below
    # (include_dirs/pch/FLL/out_lib/int_dir) never get populated from one --
    # only the file listing (used for raw_listers below) and, when a class
    # has no win64 vcxproj at all, the fallback vcxproj/absent_nodes wiring.
    parsed_by_projname_agde = _parse_and_validate(vcxproj.discover_agde_vcxproj_files(repo_root))

    nodes: dict[str, Node] = {}
    absent_nodes: list[str] = []
    missing_vcxproj: list[str] = []

    for info in scan_result.classes.values():
        if info.kind is None or not info.proj_name:
            continue
        node = Node(name=info.proj_name, kind=info.kind, source_roots=list(info.source_roots))
        vfile = parsed_by_projname.get(info.proj_name)
        if vfile is None:
            vfile = parsed_by_projname_agde.get(info.proj_name)
        if vfile is None:
            # A scanned, non-abstract Sharpmake project class with NO
            # matching vcxproj at all (neither win64 nor agde) -- typically
            # a new .cs added without running regen.ps1. Report it (both in
            # absent_nodes, since the node genuinely carries no per-config
            # data for THIS config either, and in missing_vcxproj, which
            # names the specific problem) rather than silently degrading to
            # an empty node. Synthesized third-party middleware nodes never
            # reach this loop at all (they have no class -- they're added to
            # `nodes` later, from tree-walk ownership, not from
            # scan_result.classes), so they're exempt for free.
            absent_nodes.append(node.name)
            missing_vcxproj.append(
                f"{info.name} (Name=\"{info.proj_name}\") in {info.file} has no matching "
                f"vcxproj -- run Build\\regen.ps1"
            )
        if vfile is not None:
            node.vcxproj = _to_repo_relative(vfile.path, repo_root)
            if vfile.has_config:
                vcx_dir = vfile.path.resolve().parent
                node.include_dirs = list(vfile.include_dirs)
                node.pch_header = vfile.pch_header
                node.function_level_linking = vfile.function_level_linking
                if vfile.int_dir:
                    node.int_dir = _to_repo_relative(
                        vcx_dir / vfile.int_dir.replace("\\", "/"), repo_root
                    )
                if node.kind in ("lib", "middleware") and vfile.out_dir and vfile.target_name:
                    node.out_lib = _to_repo_relative(
                        vcx_dir / vfile.out_dir.replace("\\", "/") / (vfile.target_name + ".lib"),
                        repo_root,
                    )
            else:
                absent_nodes.append(node.name)
        nodes[node.name] = node

    # -- Sentinel subject ------------------------------------------------------
    for info in scan_result.classes.values():
        if info.kind != "proof" or not info.name.startswith("Sentinel"):
            continue
        if not info.deps or not info.proj_name:
            continue
        first_dep = info.deps[0]
        target = scan_result.classes.get(first_dep.dst_class)
        if target is not None and target.proj_name:
            nodes[info.proj_name].subject = target.proj_name

    # -- Declared edges ----------------------------------------------------
    declared: list[DeclaredEdge] = []
    unresolved_conditions: list[str] = []
    backend_prefix = _backend_prefix(config)

    def resolve(class_name: str, origin_file: str, line: int) -> str:
        target = scan_result.classes.get(class_name)
        if target is None or target.kind is None or not target.proj_name:
            raise ValueError(
                f"{origin_file}:{line}: AddPublicDependency<{class_name}> references "
                f"unknown class '{class_name}'"
            )
        return target.proj_name

    for src in scan_result.classes.values():
        if src.kind is None or not src.proj_name:
            continue

        for dep in src.deps:
            dst_name = resolve(dep.dst_class, src.file, dep.line)
            if dep.conditional_backend is None:
                declared.append(
                    DeclaredEdge(
                        src=src.proj_name,
                        dst=dst_name,
                        conditional=None,
                        origin_file=src.file,
                        order=dep.order,
                    )
                )
                continue
            if backend_prefix is not None and dep.conditional_backend == backend_prefix:
                declared.append(
                    DeclaredEdge(
                        src=src.proj_name,
                        dst=dst_name,
                        conditional=None,
                        origin_file=src.file,
                        order=dep.order,
                    )
                )
            elif backend_prefix is not None:
                continue  # a sibling branch not taken for this resolved config
            else:
                raw = f"target.RenderBackend == RenderBackend.{dep.conditional_backend}"
                declared.append(
                    DeclaredEdge(
                        src=src.proj_name,
                        dst=dst_name,
                        conditional=raw,
                        origin_file=src.file,
                        order=dep.order,
                    )
                )
                unresolved_conditions.append(f"{src.file}:{dep.line}: {raw}")

        if src.is_aggregate_expansion and scan_result.entry_order:
            origin = scan_result.entry_table_file or src.file
            idx = 0
            for entry_class in scan_result.entry_order:
                if entry_class == src.name:
                    continue
                dst_name = resolve(entry_class, origin, 0)
                declared.append(
                    DeclaredEdge(
                        src=src.proj_name,
                        dst=dst_name,
                        conditional=None,
                        origin_file=origin,
                        order=idx,
                        via_aggregate=True,
                    )
                )
                idx += 1

        if src.kind == "game" and scan_result.games_base is not None and scan_result.games_base.deps:
            base_dep = scan_result.games_base.deps[0]
            origin = scan_result.games_base_file or scan_result.games_base.file
            dst_name = resolve(base_dep.dst_class, origin, base_dep.line)
            declared.append(
                # via_aggregate=False (the default): this is a literal
                # AddPublicDependency<ZenithProject> call inherited from
                # GameProject's own ConfigureAll, not an aggregate-expansion
                # edge -- ZenithProject is the DST here, not the SRC.
                DeclaredEdge(
                    src=src.proj_name, dst=dst_name, conditional=None, origin_file=origin, order=0
                )
            )

    # -- Ownership: vcxproj listings -----------------------------------------
    # File listings (ClCompile/ClInclude ItemGroups) carry no per-config
    # Condition in Sharpmake's output -- only ExcludedFromBuild does (already
    # applied by vcxproj.parse_vcxproj for the chosen config). So a node
    # absent for THIS config (e.g. a Sentinel that only declares
    # ToolsEnabled.False, absent from a "..._True" config) still lists its
    # files project-wide; gate on the vcxproj existing at all, not on
    # has_config, or its files would wrongly show up as orphans.
    raw_listers: dict[str, list[str]] = {}
    for node in nodes.values():
        # Merge BOTH the win64 and the agde vcxproj's file listing for this
        # node (e.g. ZenithCore compiles on both platforms). A path listed by
        # both of the SAME node's two vcxproj files is not a duplicate --
        # dedupe per (node, path) before ever touching raw_listers, so only a
        # DIFFERENT node listing the same path can make it a duplicate.
        node_files: set[str] = set()
        for vfile in (parsed_by_projname.get(node.name), parsed_by_projname_agde.get(node.name)):
            if vfile is None:
                continue
            for path, _itag in vfile.files:
                ext = "." + path.rsplit(".", 1)[-1].lower() if "." in path else ""
                if path.lower().endswith(".tests.inl"):
                    ext = ".inl"
                if ext not in _SOURCE_EXTS:
                    continue
                node_files.add(path)
        for path in node_files:
            raw_listers.setdefault(path, []).append(node.name)

    middleware_nodes = [n for n in nodes.values() if n.kind == "middleware"]
    synthesized: dict[str, Node] = {}

    files: dict[str, FileOwner] = {}
    duplicates: list[str] = []

    for path, listers in raw_listers.items():
        if len(listers) > 1:
            duplicates.append(path)
        if path.startswith("Middleware/"):
            owner = _match_middleware_owner(path, middleware_nodes, synthesized)
        else:
            owner = listers[0]
        dup_of = [l for l in listers if l != owner]
        files[path] = FileOwner(path=path, node=owner, how="listed", duplicate_of=dup_of)

    duplicates.sort()

    # -- Tree walk: orphans + unlisted middleware routing --------------------
    orphans: list[str] = []
    for dirname in _SOURCE_TREE_DIRS:
        d = repo_root / dirname
        if not d.is_dir():
            continue
        for rel_str in _iter_source_files(d, repo_root, skip_middleware_seg=True):
            if rel_str in raw_listers:
                continue
            orphans.append(rel_str)
            best_node = None
            best_len = -1
            for n in nodes.values():
                for root in n.source_roots:
                    if rel_str == root or rel_str.startswith(root + "/"):
                        if len(root) > best_len:
                            best_len = len(root)
                            best_node = n.name
            if best_node is not None:
                files[rel_str] = FileOwner(path=rel_str, node=best_node, how="dir-fallback", duplicate_of=[])

    middleware_dir = repo_root / "Middleware"
    if middleware_dir.is_dir():
        for rel_str in _iter_source_files(middleware_dir, repo_root, skip_middleware_seg=False):
            if rel_str in raw_listers:
                continue
            owner = _match_middleware_owner(rel_str, middleware_nodes, synthesized)
            files[rel_str] = FileOwner(path=rel_str, node=owner, how="dir-fallback", duplicate_of=[])

    for name, synth_node in synthesized.items():
        nodes.setdefault(name, synth_node)

    orphans.sort()

    # -- pch_root --------------------------------------------------------------
    lib_nodes = [n for n in nodes.values() if n.kind == "lib" and n.name not in absent_nodes]
    counts = Counter(n.pch_header for n in lib_nodes if n.pch_header)
    pch_root: str | None = None
    if counts:
        mode_value, _ = counts.most_common(1)[0]
        for n in lib_nodes:
            if n.pch_header != mode_value:
                continue
            for inc_dir in n.include_dirs:
                candidate = repo_root / inc_dir / mode_value
                if candidate.is_file():
                    pch_root = f"{inc_dir}/{mode_value}"
                    break
            if pch_root:
                break

    allowlist = load_allowlist(repo_root)

    return Model(
        config=config.lower(),
        repo_root=str(repo_root),
        nodes=nodes,
        files=files,
        declared=declared,
        orphans=orphans,
        duplicates=duplicates,
        allowlist=allowlist,
        pch_root=pch_root,
        unresolved_conditions=unresolved_conditions,
        absent_nodes=absent_nodes,
        missing_vcxproj=missing_vcxproj,
    )


# ---------------------------------------------------------------------------
# __main__
# ---------------------------------------------------------------------------


def _model_to_dict(model: Model) -> dict:
    return {
        "config": model.config,
        "repo_root": model.repo_root,
        "nodes": {k: dataclasses.asdict(v) for k, v in model.nodes.items()},
        "files": {k: dataclasses.asdict(v) for k, v in model.files.items()},
        "declared": [dataclasses.asdict(e) for e in model.declared],
        "orphans": model.orphans,
        "duplicates": model.duplicates,
        "allowlist": model.allowlist,
        "pch_root": model.pch_root,
        "unresolved_conditions": model.unresolved_conditions,
        "absent_nodes": model.absent_nodes,
        "missing_vcxproj": model.missing_vcxproj,
    }


def _print_summary(model: Model) -> None:
    counts: dict[str, int] = {}
    for n in model.nodes.values():
        counts[n.kind] = counts.get(n.kind, 0) + 1
    print(f"config: {model.config}")
    print(f"repo_root: {model.repo_root}")
    print("node counts:")
    for kind in sorted(counts):
        print(f"  {kind}: {counts[kind]}")
    print(f"absent_nodes ({len(model.absent_nodes)}): {sorted(model.absent_nodes)}")
    if model.missing_vcxproj:
        print(f"missing_vcxproj ({len(model.missing_vcxproj)}) -- run Build\\regen.ps1:")
        for m in model.missing_vcxproj:
            print(f"  {m}")
    print(f"pch_root: {model.pch_root}")
    print(f"declared edges: {len(model.declared)}")
    print(f"unresolved_conditions ({len(model.unresolved_conditions)}):")
    for c in model.unresolved_conditions:
        print(f"  {c}")
    print(f"orphans ({len(model.orphans)}):")
    for o in model.orphans:
        print(f"  {o}")
    print(f"duplicates ({len(model.duplicates)}):")
    for d in model.duplicates:
        print(f"  {d}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="libgraph project model")
    parser.add_argument("--config", default="vulkan_vs2022_debug_win64_true")
    parser.add_argument("--repo-root", default=None)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[2]

    try:
        model = build_model(repo_root, args.config)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(_model_to_dict(model), indent=2))
    else:
        _print_summary(model)
    return 0


if __name__ == "__main__":
    sys.exit(main())
