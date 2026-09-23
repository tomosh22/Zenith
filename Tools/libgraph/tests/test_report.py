"""
Unit tests for Tools/libgraph/report.py (render_html, write_report). These
also exercise Tools/libgraph/template.html indirectly, since render_html()
inlines it directly -- a template defect that breaks the offline/escaping
contract shows up here.

Run with:
    python -m unittest discover Tools/libgraph/tests -p "test_report.py" -v
    # or, from this directory:
    python -m unittest test_report

Mirrors Tools/tests/test_complexity.py's sys.path setup and this
directory's own test_model.py / test_contract.py pattern: Tools/libgraph/
itself goes on sys.path (flat sibling modules, no __init__.py) so
`import report` resolves, and this directory goes on sys.path too so
`import contract` resolves to tests/contract.py sitting right beside this
file.

Python 3.12 stdlib only.
"""

from __future__ import annotations

import copy
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))  # Tools/libgraph/ -> import report
sys.path.insert(0, str(_HERE))         # tests/          -> import contract

import contract  # noqa: E402
import report  # noqa: E402

_SAMPLE_PATH = _HERE / "fixtures" / "sample_libgraph.json"

_JSON_BLOB_RE = re.compile(
    r'<script type="application/json" id="libgraph-data">(.*?)</script>',
    re.DOTALL,
)


def _load_sample() -> dict:
    return json.loads(_SAMPLE_PATH.read_text(encoding="utf-8"))


def _extract_json_blob(html: str) -> str:
    """Return the raw inner text of the
    <script type="application/json" id="libgraph-data">...</script> tag."""
    m = _JSON_BLOB_RE.search(html)
    if m is None:
        raise AssertionError("libgraph-data script tag not found in rendered HTML")
    return m.group(1)


def _external_ref_violations(html: str) -> list[str]:
    """
    Structural offline check (binding amendment 6, replacing a literal
    'http://' scan): no `src=`/`href=` attribute value, and no
    `@import`/`url()` inside a <style> block, may point at `http:`,
    `https:`, or a protocol-relative `//`. Returns the offending matches; an
    empty list means clean. The vendor code may still contain URL-shaped
    strings (e.g. the SVG namespace, or licence-comment URLs) -- those never
    appear as `src=`/`href=` attribute values or inside a <style> block, so
    they don't trip this check.
    """
    violations: list[str] = []

    for quote in ('"', "'"):
        pattern = re.compile(
            r"\b(?:src|href)\s*=\s*" + quote + r"([^" + quote + r"]*)" + quote,
            re.IGNORECASE,
        )
        for m in pattern.finditer(html):
            val = m.group(1).strip()
            if val.startswith("http:") or val.startswith("https:") or val.startswith("//"):
                violations.append(m.group(0))

    for style_m in re.finditer(r"<style\b[^>]*>(.*?)</style>", html, re.DOTALL | re.IGNORECASE):
        style_body = style_m.group(1)
        for m in re.finditer(r"@import\s+[^;]*", style_body, re.IGNORECASE):
            if "http:" in m.group(0) or "https:" in m.group(0) or "//" in m.group(0):
                violations.append(m.group(0))
        for m in re.finditer(r"url\(([^)]*)\)", style_body, re.IGNORECASE):
            val = m.group(1).strip().strip("'\"")
            if val.startswith("http:") or val.startswith("https:") or val.startswith("//"):
                violations.append(m.group(0))

    return violations


def _minimal_doc(sample: dict) -> dict:
    """A minimal-but-contract-valid document: the sample with most lists
    emptied and delta null (per the Definition of Done's "rendering a
    minimal valid doc works" requirement)."""
    doc = copy.deepcopy(sample)
    doc["meta"]["baseline"] = None
    doc["meta"]["delta_removed"] = []
    doc["meta"]["delta_added"] = []
    doc["meta"]["health"] = {
        "orphans": [], "duplicates": [], "ambiguous": [], "unresolved_conditions": [],
        "missing_link_inputs": [],
    }
    doc["meta"]["symbols"] = True
    doc["delta"] = None

    doc["nodes"] = [
        {"name": "LibA", "kind": "lib", "layer": 0, "in_cycle": False, "inbound_size": 0},
    ]
    doc["edges"] = []
    doc["libs"] = {
        "LibA": {
            "inbound": {f: 0 for f in contract.INBOUND_FIELDS},
            "outbound": {"edges": [], "symbols_by_target": {}, "lower_bound": []},
            "headers": [],
            "types": [],
            "symbols": [],
            "unreferenced": [],
            "leakage": [],
            "sentinel": None,
        },
    }
    return doc


def _no_symbols_doc(sample: dict) -> dict:
    """The sample as a header-only (--no-symbols) run would produce it:
    `meta.symbols` false, `meta.health.missing_link_inputs` empty (there was
    no symbols stage to have skipped anything in), and every link-derived
    count/list zeroed "by omission, not by observation" per CONTRACT.md."""
    doc = copy.deepcopy(sample)
    doc["meta"]["symbols"] = False
    doc["meta"]["health"]["missing_link_inputs"] = []
    for edge in doc["edges"]:
        edge["symbols"] = []
    for lib in doc["libs"].values():
        lib["inbound"]["link_symbols"] = 0
        lib["outbound"]["symbols_by_target"] = {}
        lib["symbols"] = []
        lib["unreferenced"] = []
    return doc


class RenderHtmlTests(unittest.TestCase):
    def setUp(self):
        self.sample = _load_sample()
        self.html = report.render_html(self.sample)

    def test_contains_embedded_json_and_round_trips(self):
        self.assertIn('id="libgraph-data"', self.html)
        blob = _extract_json_blob(self.html)
        round_tripped = json.loads(blob)
        self.assertEqual(round_tripped, self.sample)

    def test_no_external_urls(self):
        violations = _external_ref_violations(self.html)
        self.assertEqual(violations, [], f"external references found: {violations}")

    def test_contains_all_three_vendored_libraries(self):
        # Distinctive, byte-for-byte-preserved identifiers from each vendor
        # file -- proof each one is actually inlined, not just referenced.
        self.assertIn('kc.version="3.30.4"', self.html)  # cytoscape.min.js
        self.assertIn("g.dagre=f()", self.html)  # dagre.min.js UMD wrapper
        self.assertIn('"cytoscapeDagre"', self.html)  # cytoscape-dagre.js UMD wrapper

    def test_vendor_scripts_appear_in_order(self):
        # Binding amendment 3: cytoscape, then dagre, then cytoscape-dagre.
        i_cy = self.html.index('kc.version="3.30.4"')
        i_dagre = self.html.index("g.dagre=f()")
        i_cydagre = self.html.index('"cytoscapeDagre"')
        self.assertLess(i_cy, i_dagre, "cytoscape.min.js must be inlined before dagre.min.js")
        self.assertLess(i_dagre, i_cydagre, "dagre.min.js must be inlined before cytoscape-dagre.js")

    def test_dagre_registration_guard_present(self):
        # Register only if not already registered, then verify, and show a
        # visible (but no longer fatal -- see test_layered_layout_present)
        # warning banner if it fails to register.
        self.assertIn("cytoscape.use(cytoscapeDagre)", self.html)
        self.assertIn("graph-error", self.html)

    def test_layered_layout_present_and_default(self):
        """Nodes are placed from nodes[].layer directly (a `preset` layout
        the app code computes itself via barycenter sweeps), not from
        dagre's edge-driven ranking -- dagre stays selectable as the
        alternative "usage" layout, per the orchestrator's layering-rule
        change (declared layers only; real-use layering collapses most of
        the real engine into one SCC)."""
        self.assertIn("var graphLayoutMode = 'layered'", self.html)
        self.assertIn("function computeLayeredPositions(", self.html)
        self.assertIn("name: 'preset'", self.html)
        self.assertIn("layered (declared)", self.html)
        self.assertIn("dagre (usage)", self.html)
        # dagre registration failure must no longer block the Graph view --
        # only the default layered layout is required to work.
        self.assertIn("does not need it and still works", self.html)

    def test_up_edge_rendering_present(self):
        """layer[src] <= layer[dst] (excluding game edges) is an up-edge --
        styled thicker and drawn on top in the Graph view, highlighted in
        the DSM, filterable with its own toggle, and explained in the
        legend."""
        self.assertIn("function isUpEdge(e)", self.html)
        self.assertIn("edge.up-edge", self.html)
        self.assertIn("up-edges only", self.html)
        self.assertIn("cls.push('upedge')", self.html)
        self.assertIn("vertical position = declared layer", self.html)

    def test_hairball_defaults_and_node_focus_present(self):
        """Games default OFF, declared+used edges dim by default (a
        toggleable 'dim expected edges' setting), and clicking a node
        isolates its own edges at full strength while hiding every other
        edge until the background is clicked -- the visual hierarchy fix
        for a real-repo-scale (45 nodes / a few hundred edges) hairball."""
        self.assertIn("games: false, middleware: false, proof: false", self.html)
        self.assertIn("'dim-expected': true", self.html)
        self.assertIn("dim expected edges", self.html)
        self.assertIn("edge.dim-expected", self.html)
        self.assertIn("edge.focused-edge", self.html)
        self.assertIn("edge.focus-hidden", self.html)
        self.assertIn("function applyStaticEdgeClasses(", self.html)

    def test_search_enter_and_datalist_present(self):
        """Enter in the search box commits the match, switches to the Graph
        tab and focuses/zooms to it; a <datalist> backs the box with every
        node name as a native suggestion list."""
        self.assertIn('list="node-names-datalist"', self.html)
        self.assertIn('id="node-names-datalist"', self.html)
        self.assertIn("function findSearchMatch(", self.html)
        self.assertIn("evt.key !== 'Enter'", self.html)
        self.assertIn("activateTab('graph')", self.html)

    def test_label_zoom_threshold_present(self):
        """Labels hide below a zoomed-size threshold rather than shrinking
        into illegible pixels, and are forced back on for a focused or
        hovered node regardless of zoom."""
        self.assertIn("'min-zoomed-font-size'", self.html)
        self.assertIn("node.hovered", self.html)
        self.assertIn("mouseover", self.html)

    def test_narrow_pane_overlay_present(self):
        """Below ~900px the detail panel becomes a fixed full-width
        overlay drawer instead of squeezing the graph into a sliver."""
        self.assertIn("@media (max-width: 899px)", self.html)
        self.assertIn("position: fixed", self.html)

    def test_minimal_doc_renders(self):
        minimal = _minimal_doc(self.sample)
        errors = contract.validate(minimal)
        self.assertEqual(errors, [], f"minimal fixture itself is not contract-valid: {errors}")

        html = report.render_html(minimal)
        self.assertIn("<html", html.lower())
        blob = _extract_json_blob(html)
        self.assertEqual(json.loads(blob), minimal)

    def test_no_symbols_doc_renders(self):
        """`meta.symbols == false` (a header-only run): the doc must stay
        contract-valid and render, with the page noting link data wasn't
        collected rather than rendering the omitted zeros as measured."""
        doc = _no_symbols_doc(self.sample)
        errors = contract.validate(doc)
        self.assertEqual(errors, [], f"no-symbols fixture is not contract-valid: {errors}")

        html = report.render_html(doc)
        blob = _extract_json_blob(html)
        self.assertEqual(json.loads(blob), doc)

        # The template's own app code branches on meta.symbols / n/a --
        # confirm the source actually carries that branch (the app code
        # runs client-side, so this checks the mechanism is present rather
        # than executing it).
        self.assertIn("header-only run: link data not collected", html)
        self.assertIn("SYMBOLS_COLLECTED", html)


class JsonEscapingTests(unittest.TestCase):
    """
    Binding amendment 2: every '<' in the serialized JSON blob is replaced
    with its \\u003c escape -- one pass that makes '</script' and '<!--'
    impossible to spell inside it. This replaces the brief's original
    "a JSON string containing </script> is escaped" test with the amendment's
    structural rule.
    """

    def test_close_script_and_html_comment_are_escaped(self):
        doc = copy.deepcopy(_load_sample())
        doc["edges"][0]["files"][0]["raw"] = '#include "x.h" </script><!-- hi -->'
        html = report.render_html(doc)
        blob = _extract_json_blob(html)

        self.assertNotIn("<", blob, "no literal '<' may remain in the embedded JSON blob")
        self.assertIn("\\u003c/script", blob)
        self.assertIn("\\u003c!--", blob)

        round_tripped = json.loads(blob)
        self.assertEqual(
            round_tripped["edges"][0]["files"][0]["raw"],
            '#include "x.h" </script><!-- hi -->',
        )

    def test_html_special_chars_never_appear_as_literal_markup(self):
        """
        Binding amendment 1: a `demangled`/`raw` value carrying HTML-special
        characters is only ever present, escaped, inside the JSON blob --
        never as literal markup anywhere else in the page (the template does
        no server-side interpolation of document data outside that blob).
        """
        doc = copy.deepcopy(_load_sample())
        payload = "<b>x</b>&amp;"
        doc["edges"][0]["symbols"][0]["demangled"] = payload
        doc["edges"][0]["files"][0]["raw"] = payload
        html = report.render_html(doc)

        # Never appears as literal markup anywhere in the rendered page.
        self.assertNotIn(payload, html)
        self.assertNotIn("<b>x</b>", html)

        # It IS present, correctly escaped, inside the JSON blob, and
        # round-trips back to the exact original string.
        blob = _extract_json_blob(html)
        self.assertIn("\\u003cb>", blob)  # only '<' is escaped, per binding amendment 2
        round_tripped = json.loads(blob)
        self.assertEqual(round_tripped["edges"][0]["symbols"][0]["demangled"], payload)
        self.assertEqual(round_tripped["edges"][0]["files"][0]["raw"], payload)


class WriteReportTests(unittest.TestCase):
    def test_writes_both_files(self):
        sample = _load_sample()
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "sub" / "dir"
            json_path, html_path = report.write_report(sample, out_dir)

            self.assertEqual(json_path, out_dir / "libgraph.json")
            self.assertEqual(html_path, out_dir / "index.html")
            self.assertTrue(json_path.is_file())
            self.assertTrue(html_path.is_file())

            written_doc = json.loads(json_path.read_text(encoding="utf-8"))
            self.assertEqual(written_doc, sample)

            html = html_path.read_text(encoding="utf-8")
            blob = _extract_json_blob(html)
            self.assertEqual(json.loads(blob), sample)


if __name__ == "__main__":
    unittest.main()
