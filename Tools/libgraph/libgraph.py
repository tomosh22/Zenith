"""
Tools/libgraph/libgraph.py -- the libgraph CLI.

Runs the full pipeline (model -> includes -> cxxtypes -> symbols -> metrics
-> report) for a chosen Sharpmake --config and writes libgraph.json plus
index.html to --out: a self-contained interactive HTML report of the
boundaries between Zenith engine libraries (dependencies, public
header/type/link surfaces, leakage, and a swap contract per lib). See
README.md for what each view means and how to read the output for the two
review goals -- trimming a lib's public surface, and proving a lib is
swappable.

Usage:
    python Tools/libgraph/libgraph.py [--config <cfg>] [--no-symbols]
        [--no-games] [--baseline <json>] [--out <dir>] [--open]
        [--repo-root <dir>] [--dumpbin-dir <dir>] [--jobs N] [--quiet]
        [--debug]

Exit codes:
    0  ok
    1  model or input error -- a stale/missing vcxproj, an unknown
       Sharpmake class, a bad --repo-root, a template/vendor-JS problem
    2  symbols refusal (symbols.SymbolsError) -- FunctionLevelLinking is
       enabled for the chosen config, or a lib's .lib / a game's .obj
       files are missing. Re-run with --no-symbols for a header-only
       report that needs no build.
    3  --baseline is unreadable, isn't valid JSON, or isn't a
       libgraph.json-shaped document

Every error is printed as one clear, actionable line to stderr, with no
traceback. Pass --debug to also see the full Python traceback.

Flat sibling module (no package, no __init__.py); run in script mode from
the repo root. Python 3.12 stdlib only.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import traceback
import webbrowser
from collections import Counter
from pathlib import Path

import cxxtypes as cxxtypes_mod
import includes as includes_mod
import metrics as metrics_mod
import model as model_mod
import report as report_mod
import symbols as symbols_mod

_DEFAULT_CONFIG = "vulkan_vs2022_debug_win64_true"


# ---------------------------------------------------------------------------
# Argument parsing.
# ---------------------------------------------------------------------------


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="libgraph.py",
        description=(
            "Generate libgraph's self-contained interactive HTML report of the "
            "boundaries between Zenith engine libraries: dependencies, public "
            "header/type/link surfaces, leakage, and a swap contract per lib."
        ),
        epilog=(
            "Exit codes: 0 ok; "
            "1 model/input error (e.g. a stale or missing vcxproj, an unknown "
            "Sharpmake class, a bad --repo-root); "
            "2 symbols refusal (SymbolsError, e.g. FunctionLevelLinking is on "
            "for the chosen config, or a .lib/.obj is missing) -- re-run with "
            "--no-symbols for a header-only report; "
            "3 --baseline is unreadable, not valid JSON, or not a "
            "libgraph.json-shaped document."
        ),
    )
    parser.add_argument(
        "--config", default=_DEFAULT_CONFIG,
        help=f"Sharpmake config to analyse (default: {_DEFAULT_CONFIG})",
    )
    parser.add_argument(
        "--no-symbols", action="store_true",
        help="skip the dumpbin link-surface stage -- a header-only report that needs no build",
    )
    parser.add_argument(
        "--no-games", action="store_true",
        help="exclude game nodes from the report",
    )
    parser.add_argument(
        "--baseline", default=None, metavar="JSON",
        help="a previous libgraph.json to diff this run against",
    )
    parser.add_argument(
        "--out", default=None, metavar="DIR",
        help="output directory (default: <repo-root>/Build/artifacts/libgraph, gitignored)",
    )
    parser.add_argument(
        "--open", action="store_true",
        help="open index.html in the default browser once the report is written",
    )
    parser.add_argument(
        "--repo-root", default=None, metavar="DIR",
        help="repo root to analyse (default: the repo containing this script)",
    )
    parser.add_argument(
        "--dumpbin-dir", default=None, metavar="DIR",
        help="read canned `dumpbin /symbols` text from this directory instead of "
             "running dumpbin.exe (test use)",
    )
    parser.add_argument(
        "--jobs", type=int, default=0, metavar="N",
        help="parallel dumpbin worker processes (default: os.cpu_count())",
    )
    parser.add_argument(
        "--quiet", action="store_true",
        help="suppress the per-stage timing lines and the summary",
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="show a full Python traceback alongside the one-line error message",
    )
    return parser


# ---------------------------------------------------------------------------
# --baseline loading + shape validation (exit 3). metrics.compute() itself
# does no shape checking of the baseline dict it's handed, so this is done
# here, before the pipeline runs at all.
# ---------------------------------------------------------------------------


def _load_baseline(path_str: str) -> dict:
    """Loads and shape-checks a --baseline document. Raises ValueError with
    a one-line, actionable message on any problem -- an unreadable file, a
    parse error, or a document that isn't a dict with a "libs" dict and a
    "meta" dict. Callers of this function turn that ValueError into exit
    code 3, distinct from the generic exit-1 ValueError handling around the
    rest of the pipeline."""
    path = Path(path_str)
    try:
        raw = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValueError(f"--baseline {path_str}: could not read it ({exc})") from exc
    try:
        doc = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"--baseline {path_str}: not valid JSON ({exc})") from exc
    if not isinstance(doc, dict) or not isinstance(doc.get("libs"), dict) or not isinstance(doc.get("meta"), dict):
        raise ValueError(
            f"--baseline {path_str}: not a libgraph.json document "
            f"(expected an object with a \"libs\" object and a \"meta\" object)"
        )
    return doc


# ---------------------------------------------------------------------------
# repo_head: git -C <repo_root> rev-parse --show-toplevel, then rev-parse
# HEAD only when that toplevel IS repo_root (binding amendment 6). The
# libgraph fixture tree sits inside the real repo, so a plain `rev-parse
# HEAD` run from inside it would report the REAL repo's commit rather than
# "no repo here" -- comparing --show-toplevel against repo_root is what
# tells the two cases apart. "" on any failure: git missing, repo_root not
# a git work tree at all, or a resolved toplevel that isn't repo_root
# itself.
# ---------------------------------------------------------------------------


def _run_git(repo_root: Path, *args: str) -> subprocess.CompletedProcess | None:
    try:
        return subprocess.run(
            ["git", "-C", str(repo_root), *args],
            capture_output=True, encoding="utf-8", errors="replace",
        )
    except OSError:
        return None


def _compute_repo_head(repo_root: Path) -> str:
    top = _run_git(repo_root, "rev-parse", "--show-toplevel")
    if top is None or top.returncode != 0 or not top.stdout.strip():
        return ""
    try:
        toplevel = Path(top.stdout.strip()).resolve()
    except OSError:
        return ""
    if toplevel != repo_root.resolve():
        return ""
    head = _run_git(repo_root, "rev-parse", "HEAD")
    if head is None or head.returncode != 0:
        return ""
    return head.stdout.strip()


# ---------------------------------------------------------------------------
# Summary (printed after a successful run, unless --quiet).
# ---------------------------------------------------------------------------


def _up_edge_count(doc: dict) -> int:
    """CONTRACT.md's up-edge definition: a non-game edge where
    layer[src] <= layer[dst] -- src reaches dst at the same layer or
    higher, which cannot happen for a clean, declared-only DAG walked
    strictly downward."""
    layer_by_name = {n["name"]: n["layer"] for n in doc["nodes"]}
    kind_by_name = {n["name"]: n["kind"] for n in doc["nodes"]}
    count = 0
    for e in doc["edges"]:
        if kind_by_name.get(e["src"]) == "game" or kind_by_name.get(e["dst"]) == "game":
            continue
        if layer_by_name.get(e["src"], 0) <= layer_by_name.get(e["dst"], 0):
            count += 1
    return count


def _in_cycle_scc_sizes(doc: dict) -> list[int]:
    """Union-find (via simple BFS component-labelling) over the nodes with
    in_cycle true, joined by edges whose classes include "in-cycle" --
    those edges are exactly the intra-SCC ones (CONTRACT.md's `in-cycle`
    definition), so the connected components this produces ARE the real-use
    SCCs. Returned sizes, largest first."""
    in_cycle_names = sorted(n["name"] for n in doc["nodes"] if n["in_cycle"])
    adjacency: dict[str, set[str]] = {name: set() for name in in_cycle_names}
    for e in doc["edges"]:
        if "in-cycle" in e["classes"]:
            src, dst = e["src"], e["dst"]
            if src in adjacency and dst in adjacency:
                adjacency[src].add(dst)
                adjacency[dst].add(src)

    seen: set[str] = set()
    sizes: list[int] = []
    for name in in_cycle_names:
        if name in seen:
            continue
        component: set[str] = set()
        stack = [name]
        while stack:
            cur = stack.pop()
            if cur in component:
                continue
            component.add(cur)
            seen.add(cur)
            stack.extend(adjacency[cur] - component)
        sizes.append(len(component))
    sizes.sort(reverse=True)
    return sizes


def _print_summary(doc: dict, json_path: Path, html_path: Path) -> None:
    kind_counts = Counter(n["kind"] for n in doc["nodes"])
    print("node counts:")
    for kind in sorted(kind_counts):
        print(f"  {kind}: {kind_counts[kind]}")

    base_counts = Counter(e["classes"][0] for e in doc["edges"] if e["classes"])
    print("edge counts by base class:")
    for cls in sorted(base_counts):
        print(f"  {cls}: {base_counts[cls]}")

    print(f"up-edges: {_up_edge_count(doc)}")
    print(f"in-cycle SCC sizes: {_in_cycle_scc_sizes(doc)}")

    health = doc["meta"]["health"]
    print("health:")
    print(f"  orphans: {len(health['orphans'])}")
    print(f"  duplicates: {len(health['duplicates'])}")
    print(f"  ambiguous: {len(health['ambiguous'])}")
    print(f"  unresolved_conditions: {len(health['unresolved_conditions'])}")
    print(f"  missing_link_inputs: {len(health['missing_link_inputs'])}")
    for line in health["missing_link_inputs"]:
        print(f"  {line}")

    print(f"wrote {json_path}")
    print(f"wrote {html_path}")


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

    parser = _build_arg_parser()
    args = parser.parse_args(argv)

    default_repo_root = Path(__file__).resolve().parents[2]
    repo_root = Path(args.repo_root).resolve() if args.repo_root else default_repo_root

    if not repo_root.is_dir():
        print(f"repo root not found: {repo_root}", file=sys.stderr)
        return 1

    out_dir = Path(args.out) if args.out else (repo_root / "Build" / "artifacts" / "libgraph")

    baseline_doc: dict | None = None
    if args.baseline:
        try:
            baseline_doc = _load_baseline(args.baseline)
        except ValueError as exc:
            print(f"error: {exc}", file=sys.stderr)
            if args.debug:
                traceback.print_exc()
            return 3

    def log(msg: str) -> None:
        if not args.quiet:
            print(msg)

    t_total0 = time.time()

    try:
        t0 = time.time()
        m = model_mod.build_model(repo_root, args.config)
        log(f"model: {time.time() - t0:.2f}s")
        for w in m.missing_vcxproj:
            log(f"warning: {w}")

        t0 = time.time()
        graph = includes_mod.build(m)
        log(f"includes: {time.time() - t0:.2f}s")

        t0 = time.time()
        types = cxxtypes_mod.build(m, graph)
        log(f"cxxtypes: {time.time() - t0:.2f}s")

        symbols = None
        if not args.no_symbols:
            t0 = time.time()
            symbols = symbols_mod.build(
                m,
                dumpbin_dir=Path(args.dumpbin_dir) if args.dumpbin_dir else None,
                jobs=args.jobs,
            )
            log(f"symbols: {time.time() - t0:.2f}s")

        repo_head = _compute_repo_head(repo_root)

        t0 = time.time()
        doc = metrics_mod.compute(
            m,
            graph,
            types,
            symbols,
            baseline=baseline_doc,
            baseline_path=args.baseline,
            include_games=not args.no_games,
            repo_head=repo_head,
        )
        log(f"metrics: {time.time() - t0:.2f}s")

        t0 = time.time()
        json_path, html_path = report_mod.write_report(doc, out_dir)
        log(f"report: {time.time() - t0:.2f}s")

    except symbols_mod.SymbolsError as exc:
        print(f"error: {exc}", file=sys.stderr)
        print("re-run with --no-symbols for a header-only report", file=sys.stderr)
        if args.debug:
            traceback.print_exc()
        return 2
    except (ValueError, RuntimeError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        if args.debug:
            traceback.print_exc()
        return 1

    log(f"total: {time.time() - t_total0:.2f}s")

    if not args.quiet:
        _print_summary(doc, json_path, html_path)

    if args.open:
        webbrowser.open(html_path.resolve().as_uri())

    return 0


if __name__ == "__main__":
    sys.exit(main())
