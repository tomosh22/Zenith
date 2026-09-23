"""MSBuild .vcxproj discovery and parsing for libgraph.

Reads generated Sharpmake output (`Build/*_win64.vcxproj`,
`Games/*/build/*_win64.vcxproj`, and the handful of top-level tool
directories) the way ``Tools/check_project_membership.ps1``'s
``Get-CandidateVcxprojFiles`` does, and extracts the per-config facts
``model.py`` needs: OutDir/IntDir/TargetName, AdditionalIncludeDirectories,
PrecompiledHeaderFile/PrecompiledHeader, FunctionLevelLinking, and the
ClCompile/ClInclude file listing (respecting per-config ExcludedFromBuild).

Python 3.12 stdlib only. Flat sibling module: no package, no relative import.
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

MSBUILD_NS = "http://schemas.microsoft.com/developer/msbuild/2003"

# Top-level directories that are not part of the tracked tree at all
# (gitignored worktree scratch, VCS internals, permission-denied scratch
# dirs) -- mirrors Get-CandidateVcxprojFiles's $excludeTopDirs.
_SKIP_TOP_DIRS = {".git", ".claude", ".codex_tmp", "node_modules"}

# Generated-artifact / vendored-project-file exclusions -- mirrors
# Get-CandidateVcxprojFiles's $excludeSegmentRegex. Middleware ships its own
# vcxproj/sln files that are not Sharpmake output.
_SKIP_SEGMENT_RE = re.compile(r"(?i)[\\/](obj|output|\.vs|dist|Middleware)[\\/]")
_SKIP_ARTIFACTS_RE = re.compile(r"(?i)[\\/]Build[\\/]artifacts[\\/]")
# Tools/libgraph/tests/fixtures/repo ships its OWN Sharpmake-shaped vcxproj
# files for the libgraph test suite -- not real Sharpmake output for the
# actual repo, and (since Tools/ is a real top-level dir this walk does not
# skip) a real-repo scan would otherwise descend into it and hard-error on
# a <ProjectName> the real Build/*.cs classes never declared. Mirrors the
# Middleware exclusion's own reasoning: a vendored/fixture tree ships vcxproj
# files that are not Sharpmake output and must not corrupt the graph.
_SKIP_FIXTURES_RE = re.compile(r"(?i)[\\/]Tools[\\/]libgraph[\\/]tests[\\/]fixtures[\\/]")

# A per-config Condition attribute looks like:
#   '$(Configuration)|$(Platform)'=='Vulkan_vs2022_Debug_Win64_True|x64'
_CONFIG_TOKEN_RE = re.compile(r"=='([^|']+)\|")


def _discover_vcxproj_files(repo_root: Path, want_agde: bool) -> list[Path]:
    """Shared walk for discover_vcxproj_files()/discover_agde_vcxproj_files().

    Skips .git/.claude/.codex_tmp/node_modules as top-level dirs, any path
    containing an obj/output/.vs/dist/Middleware segment, Build/artifacts,
    and the libgraph fixture tree -- then keeps only *_agde.vcxproj
    (want_agde=True) or only the rest (want_agde=False).
    """
    repo_root = Path(repo_root).resolve()
    if not repo_root.is_dir():
        raise ValueError(f"repo root not found: {repo_root}")
    results: list[Path] = []
    for top in sorted(repo_root.iterdir(), key=lambda p: p.name):
        if not top.is_dir():
            continue
        if top.name in _SKIP_TOP_DIRS:
            continue
        for p in sorted(top.rglob("*.vcxproj"), key=lambda p: str(p)):
            # Checked relative to repo_root (not the absolute path): running
            # this tool WITH repo_root pointed AT the fixture tree itself
            # (as the test suite and --repo-root both do) must not exclude
            # the fixture's own files just because the fixture directory
            # happens to be nested under a real path containing these
            # segment names.
            rel = "/" + str(p.resolve().relative_to(repo_root))
            if _SKIP_SEGMENT_RE.search(rel):
                continue
            if _SKIP_ARTIFACTS_RE.search(rel):
                continue
            if _SKIP_FIXTURES_RE.search(rel):
                continue
            is_agde = p.name.lower().endswith("_agde.vcxproj")
            if is_agde != want_agde:
                continue
            results.append(p)
    return results


def discover_vcxproj_files(repo_root: Path) -> list[Path]:
    """Walk repo_root for *_win64.vcxproj (and other non-agde vcxproj) the
    way check_project_membership.ps1's Get-CandidateVcxprojFiles does,
    excluding *_agde.vcxproj -- this tool's per-config data (OutDir,
    include dirs, PCH, FunctionLevelLinking, out_lib/int_dir) only ever
    comes from these files."""
    return _discover_vcxproj_files(repo_root, want_agde=False)


def discover_agde_vcxproj_files(repo_root: Path) -> list[Path]:
    """Walk repo_root for *_agde.vcxproj only. Used for OWNERSHIP alone: an
    agde-only class (e.g. ZenithAndroid, restricted to Platform.agde in its
    own AddTargets) has no *_win64.vcxproj at all, so its files would
    otherwise show up as orphans in a win64 scan. Never used for per-config
    data -- an agde vcxproj's Configuration tokens (e.g.
    "Vulkan_arm64_v8a_vs2022_Debug_Agde_False") never match a win64 config
    string, so parse_vcxproj's has_config naturally stays False for it."""
    return _discover_vcxproj_files(repo_root, want_agde=True)


def _strip_ns(tag: str) -> str:
    return tag.split("}", 1)[-1] if "}" in tag else tag


def _config_token(condition: str | None) -> str | None:
    if not condition:
        return None
    m = _CONFIG_TOKEN_RE.search(condition)
    return m.group(1) if m else None


@dataclass
class VcxprojFile:
    """Everything model.py needs out of one .vcxproj for one chosen config."""

    path: Path
    project_name: str | None
    has_config: bool
    out_dir: str | None = None
    int_dir: str | None = None
    target_name: str | None = None
    include_dirs: list[str] = field(default_factory=list)
    pch_header: str | None = None
    pch_mode: str | None = None
    function_level_linking: bool | None = None
    # (repo-relative forward-slash path, "ClCompile" | "ClInclude")
    files: list[tuple[str, str]] = field(default_factory=list)


def _resolve_include_dir(vcx_dir: Path, raw: str, repo_root: Path) -> str:
    """Resolve one AdditionalIncludeDirectories entry to repo-relative
    forward-slash form, or an absolute forward-slash path if it falls
    outside repo_root."""
    raw = raw.strip()
    candidate = (vcx_dir / raw.replace("\\", "/")).resolve()
    try:
        rel = candidate.relative_to(repo_root)
        return str(rel).replace("\\", "/")
    except ValueError:
        return str(candidate).replace("\\", "/")


def _resolve_file_path(vcx_dir: Path, raw: str, repo_root: Path) -> str:
    candidate = (vcx_dir / raw.replace("\\", "/")).resolve()
    try:
        rel = candidate.relative_to(repo_root)
        return str(rel).replace("\\", "/")
    except ValueError:
        return str(candidate).replace("\\", "/")


def parse_vcxproj(path: Path, config: str, repo_root: Path) -> VcxprojFile:
    """Parse one .vcxproj for the chosen (case-insensitive) config name."""
    repo_root = Path(repo_root).resolve()
    path = Path(path)
    vcx_dir = path.resolve().parent
    config_lower = config.lower()

    tree = ET.parse(path)
    root = tree.getroot()

    project_name: str | None = None
    for el in root.iter():
        if _strip_ns(el.tag) == "ProjectName":
            project_name = (el.text or "").strip()
            break

    has_config = False
    out_dir = int_dir = target_name = None
    for pg in root.iter():
        if _strip_ns(pg.tag) != "PropertyGroup":
            continue
        token = _config_token(pg.get("Condition"))
        if token is None or token.lower() != config_lower:
            continue
        if pg.get("Label") == "Configuration":
            has_config = True
            continue
        for child in pg:
            ctag = _strip_ns(child.tag)
            if ctag == "OutDir":
                out_dir = child.text
            elif ctag == "IntDir":
                int_dir = child.text
            elif ctag == "TargetName":
                target_name = child.text

    include_dirs: list[str] = []
    pch_header: str | None = None
    pch_mode: str | None = None
    fll: bool | None = None
    for idg in root.iter():
        if _strip_ns(idg.tag) != "ItemDefinitionGroup":
            continue
        token = _config_token(idg.get("Condition"))
        if token is None or token.lower() != config_lower:
            continue
        for cl in idg:
            if _strip_ns(cl.tag) != "ClCompile":
                continue
            for child in cl:
                ctag = _strip_ns(child.tag)
                if ctag == "AdditionalIncludeDirectories":
                    raw = child.text or ""
                    parts = [
                        p
                        for p in raw.split(";")
                        if p.strip() and "%(" not in p and "$(" not in p
                    ]
                    include_dirs = [
                        _resolve_include_dir(vcx_dir, p, repo_root) for p in parts
                    ]
                elif ctag == "PrecompiledHeaderFile":
                    pch_header = child.text
                elif ctag == "PrecompiledHeader":
                    pch_mode = child.text
                elif ctag == "FunctionLevelLinking":
                    fll = (child.text or "").strip().lower() == "true"

    if pch_mode is not None and pch_mode.strip() == "NotUsing":
        pch_header = None

    files: list[tuple[str, str]] = []
    for ig in root.iter():
        if _strip_ns(ig.tag) != "ItemGroup":
            continue
        for item in ig:
            itag = _strip_ns(item.tag)
            if itag not in ("ClCompile", "ClInclude"):
                continue
            inc = item.get("Include")
            if not inc:
                continue
            excluded = False
            for child in item:
                if _strip_ns(child.tag) != "ExcludedFromBuild":
                    continue
                tok = _config_token(child.get("Condition"))
                if tok is not None and tok.lower() == config_lower:
                    if (child.text or "").strip().lower() == "true":
                        excluded = True
            if excluded:
                continue
            files.append((_resolve_file_path(vcx_dir, inc, repo_root), itag))

    return VcxprojFile(
        path=path,
        project_name=project_name,
        has_config=has_config,
        out_dir=out_dir,
        int_dir=int_dir,
        target_name=target_name,
        include_dirs=include_dirs,
        pch_header=pch_header,
        pch_mode=pch_mode,
        function_level_linking=fll,
        files=files,
    )
