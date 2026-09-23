"""
Tools/libgraph/report.py -- renders a libgraph.json-shaped dict (see
Tools/libgraph/tests/CONTRACT.md) into a single, fully offline HTML report:
template.html with the three vendored scripts (Tools/libgraph/vendor/) and
the JSON payload inlined. No external URL appears anywhere in the output.

Usage (renders an EXISTING libgraph.json -- this is what the orchestrator
uses to preview the sample fixture):
    python Tools/libgraph/report.py <libgraph.json> [--out <dir>]

Flat sibling module (no package, no __init__.py); run in script mode from
the repo root. Python 3.12 stdlib only.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_VENDOR_DIR = _HERE / "vendor"
_TEMPLATE_PATH = _HERE / "template.html"

# Load order matters (binding amendment 3): cytoscape, then dagre, then
# cytoscape-dagre -- cytoscape-dagre.js expects both `cytoscape` and `dagre`
# as already-defined globals when it runs.
_VENDOR_FILES = ("cytoscape.min.js", "dagre.min.js", "cytoscape-dagre.js")

_MARKER_CYTOSCAPE = "/*__LIBGRAPH_VENDOR_CYTOSCAPE__*/"
_MARKER_DAGRE = "/*__LIBGRAPH_VENDOR_DAGRE__*/"
_MARKER_CYTOSCAPE_DAGRE = "/*__LIBGRAPH_VENDOR_CYTOSCAPE_DAGRE__*/"
_MARKER_JSON = "__LIBGRAPH_JSON_DATA__"


def _embed_json(doc: dict) -> str:
    """
    Serialize `doc` for safe embedding inside a
    <script type="application/json">...</script> element (binding amendment
    2). Every '<' is replaced with its JSON unicode escape (\\u003c) -- one
    pass, and it is enough: with no literal '<' character left anywhere in
    the blob, there is no way to spell '</script' or '<!--' inside it either.
    JSON.parse() decodes \\u003c back to '<' when the page reads the tag's
    textContent, so the document round-trips exactly.

    U+2028/U+2029 need no handling: this is a `type="application/json"` blob
    read with JSON.parse(), not a JS string literal, so the two line
    separator characters that would break a *script* literal are inert here.
    """
    raw = json.dumps(doc, ensure_ascii=False)
    return raw.replace("<", "\\u003c")


def _read_vendor(name: str) -> str:
    path = _VENDOR_DIR / name
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise RuntimeError(
            f"libgraph: missing vendored JS file {path} "
            f"(see Tools/libgraph/vendor/README.md to re-fetch it)"
        ) from exc


def render_html(doc: dict) -> str:
    """
    Render `doc` (a libgraph.json-shaped dict) into one self-contained,
    fully offline HTML page: template.html with the three vendored scripts
    and the JSON payload inlined. No external URL appears anywhere in the
    output, whether as `src=`, `href=` or `@import`/`url()`.
    """
    template = _TEMPLATE_PATH.read_text(encoding="utf-8")

    for marker in (_MARKER_CYTOSCAPE, _MARKER_DAGRE, _MARKER_CYTOSCAPE_DAGRE, _MARKER_JSON):
        if marker not in template:
            raise RuntimeError(f"libgraph: template.html is missing its {marker!r} placeholder")

    html = template
    html = html.replace(_MARKER_CYTOSCAPE, _read_vendor("cytoscape.min.js"))
    html = html.replace(_MARKER_DAGRE, _read_vendor("dagre.min.js"))
    html = html.replace(_MARKER_CYTOSCAPE_DAGRE, _read_vendor("cytoscape-dagre.js"))
    html = html.replace(_MARKER_JSON, _embed_json(doc))
    return html


def write_report(doc: dict, out_dir: Path) -> tuple[Path, Path]:
    """
    Write both output files: `<out_dir>/libgraph.json` (the raw model, as
    committed by the pipeline / the orchestrator's CLI) and
    `<out_dir>/index.html` (the rendered, offline report). Returns their
    paths as `(json_path, html_path)`.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    json_path = out_dir / "libgraph.json"
    html_path = out_dir / "index.html"

    json_path.write_text(json.dumps(doc, indent=2, ensure_ascii=False), encoding="utf-8")
    html_path.write_text(render_html(doc), encoding="utf-8")

    return json_path, html_path


def _main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        prog="report.py",
        description="Render an existing libgraph.json into a self-contained, offline HTML report.",
    )
    parser.add_argument("json_path", type=Path, help="path to an existing libgraph.json")
    parser.add_argument(
        "--out", type=Path, default=None,
        help="output directory (default: the JSON file's own directory)",
    )
    args = parser.parse_args(argv)

    try:
        doc = json.loads(args.json_path.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"libgraph: could not read {args.json_path}: {exc}", file=sys.stderr)
        return 1
    except json.JSONDecodeError as exc:
        print(f"libgraph: {args.json_path} is not valid JSON: {exc}", file=sys.stderr)
        return 1

    out_dir = args.out if args.out is not None else args.json_path.resolve().parent
    try:
        json_path, html_path = write_report(doc, out_dir)
    except RuntimeError as exc:
        print(f"libgraph: {exc}", file=sys.stderr)
        return 1

    print(f"libgraph: wrote {json_path}")
    print(f"libgraph: wrote {html_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv[1:]))
