"""Tests for metrics.py -- the core analysis stage that turns
model.py/includes.py/cxxtypes.py/symbols.py's facts into a `libgraph.json`
document. Built against the real fixture tree at `tests/fixtures/repo`
(config `fx_debug`) and validated with `tests/contract.py`.

Run:
    python -m unittest Tools.libgraph.tests.test_metrics -v
or, from the repo root:
    python -m unittest discover Tools/libgraph/tests
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))
sys.path.insert(0, str(_HERE))

import cxxtypes
import includes
import metrics
import model as model_mod
import symbols

import contract  # noqa: E402 -- tests/contract.py, made importable above

FIXTURE_REPO = _HERE / "fixtures" / "repo"
DUMPBIN_DIR = _HERE / "fixtures" / "dumpbin"


def _edge(doc: dict, src: str, dst: str) -> dict | None:
    for e in doc["edges"]:
        if e["src"] == src and e["dst"] == dst:
            return e
    return None


def _lib(doc: dict, name: str) -> dict:
    return doc["libs"][name]


def _header(lib_doc: dict, path: str) -> dict:
    for h in lib_doc["headers"]:
        if h["path"] == path:
            return h
    raise AssertionError(f"header {path!r} not found; have {[h['path'] for h in lib_doc['headers']]}")


def _type(lib_doc: dict, name: str) -> dict:
    for t in lib_doc["types"]:
        if t["name"] == name:
            return t
    raise AssertionError(f"type {name!r} not found; have {[t['name'] for t in lib_doc['types']]}")


class MetricsFixtureTestCase(unittest.TestCase):
    """Builds the full fixture pipeline once per class (expensive: includes
    + cxxtypes + symbols), then every test method reads from the shared
    result -- mirrors Tools/tests/test_complexity.py's own setUpClass
    style, and keeps the many small assertions below fast."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.model = model_mod.build_model(FIXTURE_REPO, "fx_debug")
        cls.graph = includes.build(cls.model)
        cls.types = cxxtypes.build(cls.model, cls.graph)
        cls.symtab = symbols.build(cls.model, dumpbin_dir=DUMPBIN_DIR)
        cls.doc = metrics.compute(
            cls.model,
            cls.graph,
            cls.types,
            cls.symtab,
            repo_head="deadbeefcafef00d",
            generated="2026-09-23T00:00:00Z",
        )


# ---------------------------------------------------------------------------
# 1. contract.validate() -- with and without symbols, and a self-baseline.
# ---------------------------------------------------------------------------


class InboundFieldsDriftTests(unittest.TestCase):
    """metrics.py must not import tests/contract.py (production code
    importing from tests/ breaks any --baseline run outside unittest,
    since tests/ is never on sys.path for a plain CLI invocation) -- it
    carries its own INBOUND_FIELDS tuple instead. This guards the one
    thing that duplication risks: the two copies drifting apart."""

    def test_inbound_fields_match_contract(self) -> None:
        self.assertEqual(metrics.INBOUND_FIELDS, tuple(contract.INBOUND_FIELDS))

    def test_metrics_module_imports_nothing_from_tests(self) -> None:
        source = Path(metrics.__file__).read_text(encoding="utf-8")
        for lineno, line in enumerate(source.splitlines(), start=1):
            stripped = line.strip()
            if stripped.startswith("import contract") or stripped.startswith("from contract"):
                self.fail(f"metrics.py:{lineno} imports tests/contract.py: {stripped!r}")


class ContractShapeTests(MetricsFixtureTestCase):
    def test_validates_with_symbols(self) -> None:
        errors = contract.validate(self.doc)
        self.assertEqual(errors, [])

    def test_validates_without_symbols(self) -> None:
        doc = metrics.compute(self.model, self.graph, self.types, None, generated="2026-09-23T00:00:00Z")
        errors = contract.validate(doc)
        self.assertEqual(errors, [])
        self.assertFalse(doc["meta"]["symbols"])

    def test_header_only_run_has_no_link_data(self) -> None:
        doc = metrics.compute(self.model, self.graph, self.types, None, generated="2026-09-23T00:00:00Z")
        self.assertEqual(doc["meta"]["health"]["missing_link_inputs"], [])
        for lib_doc in doc["libs"].values():
            self.assertEqual(lib_doc["inbound"]["link_symbols"], 0)
            self.assertEqual(lib_doc["symbols"], [])
            self.assertEqual(lib_doc["unreferenced"], [])
            self.assertEqual(lib_doc["outbound"]["symbols_by_target"], {})
        self.assertFalse(any("link-only" in e["classes"] for e in doc["edges"]))

    def test_validates_with_self_baseline_and_zero_deltas(self) -> None:
        doc2 = metrics.compute(
            self.model,
            self.graph,
            self.types,
            self.symtab,
            baseline=self.doc,
            baseline_path="tests/fixtures/sample_baseline.json",
            generated="2026-09-23T01:00:00Z",
        )
        errors = contract.validate(doc2)
        self.assertEqual(errors, [])
        self.assertEqual(doc2["meta"]["delta_added"], [])
        self.assertEqual(doc2["meta"]["delta_removed"], [])
        self.assertIsNotNone(doc2["delta"])
        for lib_name, fields in doc2["delta"].items():
            for field, value in fields.items():
                self.assertEqual(value, 0, f"{lib_name}.{field} expected 0, got {value}")


# ---------------------------------------------------------------------------
# 2. Edge-class scenarios (README's scenario map).
# ---------------------------------------------------------------------------


class EdgeClassTests(MetricsFixtureTestCase):
    def test_libB_to_libA_declared_used(self) -> None:
        e = _edge(self.doc, "LibB", "LibA")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["declared+used"])

    def test_libC_to_libA_used_undeclared_no_allowlist(self) -> None:
        e = _edge(self.doc, "LibC", "LibA")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["used-undeclared"])
        self.assertIsNone(e["allowlist_kind"])

    def test_libB_to_libC_used_undeclared_allowlisted_in_cycle(self) -> None:
        e = _edge(self.doc, "LibB", "LibC")
        self.assertIsNotNone(e)
        self.assertEqual(set(e["classes"]), {"used-undeclared", "allowlisted", "in-cycle"})
        self.assertIsNone(e["allowlist_kind"])  # plain layering_allowlist.txt line

    def test_libC_to_libB_declared_used_in_cycle(self) -> None:
        e = _edge(self.doc, "LibC", "LibB")
        self.assertIsNotNone(e)
        self.assertEqual(set(e["classes"]), {"declared+used", "in-cycle"})

    def test_libA_to_libC_test_only(self) -> None:
        e = _edge(self.doc, "LibA", "LibC")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["test-only"])

    def test_libA_to_libB_test_only_scenario_24(self) -> None:
        e = _edge(self.doc, "LibA", "LibB")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["test-only"])

    def test_libC_to_midlib_declared_unused(self) -> None:
        e = _edge(self.doc, "LibC", "MidLib")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["declared-unused"])
        self.assertEqual(e["files"], [])
        self.assertEqual(e["symbols"], [])

    def test_libB_to_midlib_declared_used_scenario_25(self) -> None:
        e = _edge(self.doc, "LibB", "MidLib")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["declared+used"])

    def test_zenith_to_libC_link_only(self) -> None:
        # The aggregate declares every lib (via_aggregate=True), but with
        # no normal include line reaching LibC -- per the via_aggregate
        # refinement that does NOT count as "declared" for classification,
        # so the only evidence (the LibC_ExportAll link symbol) makes this
        # link-only, not declared+used.
        e = _edge(self.doc, "Zenith", "LibC")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["link-only"])

    def test_zenith_to_libA_allowlisted_encapsulation(self) -> None:
        e = _edge(self.doc, "Zenith", "LibA")
        self.assertIsNotNone(e)
        self.assertIn("declared+used", e["classes"])
        self.assertIn("allowlisted", e["classes"])
        self.assertEqual(e["allowlist_kind"], "encapsulation")

    def test_midlib_to_liba_pch_base_scenario_26(self) -> None:
        e = _edge(self.doc, "MidLib", "LibA")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["pch"])
        self.assertEqual(len(e["files"]), 1)
        self.assertEqual(e["files"][0]["tags"], ["pch"])

    def test_midlib_to_libb_link_only_scenario_27(self) -> None:
        e = _edge(self.doc, "MidLib", "LibB")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["link-only"])
        self.assertEqual(e["files"], [])
        self.assertTrue(any("LibB_Widget_Poke" in s["demangled"] for s in e["symbols"]))

    def test_liba_owns_pch_root_no_pch_base_edges_elsewhere_unexpected(self) -> None:
        # Sanity check that a genuine edge merely INSIDE the pch closure
        # (FxPch.h -> MidLib/MidLib.h) is attributed as a real edge of its
        # OWN src owner (LibA), not folded into some other pair -- this is
        # the "Core -> Maths" example from CONTRACT.md's Include-line
        # categories section, realised here as LibA -> MidLib.
        e = _edge(self.doc, "LibA", "MidLib")
        self.assertIsNotNone(e)
        self.assertEqual(e["classes"], ["used-undeclared"])


# ---------------------------------------------------------------------------
# 3. Header roles.
# ---------------------------------------------------------------------------


class HeaderRoleTests(MetricsFixtureTestCase):
    def test_liba_h_entry(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/LibA.h")
        self.assertEqual(h["role"], "entry")

    def test_liba_math_h_exposed(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/LibA_Math.h")
        self.assertEqual(h["role"], "exposed")
        self.assertEqual(h["hint"], "exposed transitively: forward-declare to cut")

    def test_liba_unused_h_unused_public(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/LibA_Unused.h")
        self.assertEqual(h["role"], "unused-public")
        self.assertEqual(h["hint"], "no outside users: move to Internal/")

    def test_liba_private_h_internal(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/Internal/LibA_Private.h")
        self.assertEqual(h["role"], "internal")
        self.assertTrue(h["declared_internal"])

    def test_liba_detail_h_entry_declared_internal_encapsulation_hint(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/Internal/LibA_Detail.h")
        self.assertEqual(h["role"], "entry")
        self.assertTrue(h["declared_internal"])
        self.assertEqual(h["hint"], "encapsulation break: Internal/ header included from outside")

    def test_liba_solo_h_one_consumer_hint(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/LibA_Solo.h")
        self.assertEqual(h["role"], "entry")
        self.assertEqual(len(h["consumers"]), 1)
        self.assertIn("1 consumer (LibC)", h["hint"])

    def test_pch_root_header_hint_override(self) -> None:
        h = _header(_lib(self.doc, "LibA"), "Zenith/LibA/FxPch.h")
        self.assertEqual(h["hint"], "precompiled header: leave public")


# ---------------------------------------------------------------------------
# 4. Types.
# ---------------------------------------------------------------------------


class TypeTests(MetricsFixtureTestCase):
    def test_liba_mode_and_count_zero_use(self) -> None:
        for name in ("LibA_Mode", "LibA_Count"):
            t = _type(_lib(self.doc, "LibA"), name)
            self.assertEqual((t["embed"], t["ref"], t["mention"]), (0, 0, 0))
            self.assertEqual(t["hint"], "unused outside: make internal")

    def test_liba_data_has_embed(self) -> None:
        t = _type(_lib(self.doc, "LibA"), "LibA_Data")
        self.assertGreater(t["embed"], 0)

    def test_liba_thing_embed_via_base_class(self) -> None:
        # LibB_Api.h uses LibA_Thing BOTH as a base class (embed) and as a
        # bare pointer (ref) -- cxxtypes.py classifies one MODE per
        # (consumer file, type) pair, strongest wins, so this single
        # consumer file collapses to "embed" only (documented cxxtypes.py
        # blind spot: a per-file, not per-occurrence, use classifier).
        # LibA_Thing has no OTHER consumer file, so ref stays 0 here even
        # though the source genuinely contains a ref-shaped use too.
        t = _type(_lib(self.doc, "LibA"), "LibA_Thing")
        self.assertGreater(t["embed"], 0)
        self.assertIn({"node": "LibB", "mode": "embed"}, t["consumers"])

    def test_liba_handle_embedded_by_gamex(self) -> None:
        t = _type(_lib(self.doc, "LibA"), "LibA_Handle")
        self.assertGreater(t["embed"], 0)
        self.assertIn({"node": "GameX", "mode": "embed"}, t["consumers"])


# ---------------------------------------------------------------------------
# 5. Leakage.
# ---------------------------------------------------------------------------


class LeakageTests(MetricsFixtureTestCase):
    def test_liba_leaks_via_libB_api_h(self) -> None:
        leakage = _lib(self.doc, "LibA")["leakage"]
        entries = [l for l in leakage if l["header"] == "Zenith/Shared/LibB_Api.h"]
        self.assertEqual(len(entries), 1)
        entry = entries[0]
        self.assertEqual(entry["via_node"], "LibB")
        self.assertIn("GameX", entry["downstream_via_header"])
        self.assertIn("GameY", entry["downstream_via_cpp_only"])
        self.assertNotIn("GameX", entry["downstream_via_cpp_only"])
        self.assertNotIn("GameY", entry["downstream_via_header"])

    def test_only_real_headers_count_as_leaking(self) -> None:
        # A .cpp that directly includes an L-owned header (e.g. LibC_Impl.cpp,
        # Composition.cpp) cannot itself "leak" LibA onward -- nothing else
        # can #include a .cpp. Only genuine header files may appear here.
        leakage = _lib(self.doc, "LibA")["leakage"]
        for entry in leakage:
            self.assertTrue(
                entry["header"].endswith((".h", ".hpp", ".inl")),
                f"non-header leakage entry: {entry['header']}",
            )


# ---------------------------------------------------------------------------
# 6. Symbols.
# ---------------------------------------------------------------------------


class SymbolTests(MetricsFixtureTestCase):
    def test_liba_thing_group_referenced_by_libB(self) -> None:
        libA = _lib(self.doc, "LibA")
        groups = {s["cls"]: s for s in libA["symbols"]}
        self.assertIn("LibA_Thing", groups)
        self.assertIn("LibB", groups["LibA_Thing"]["referenced_by"])

    def test_liba_unreferenced_external_listed(self) -> None:
        libA = _lib(self.doc, "LibA")
        demangled_unref = {u["demangled"] for u in libA["unreferenced"]}
        self.assertTrue(any("StaticHelper" in d for d in demangled_unref))
        self.assertTrue(any("LibA_UnrefFunc" in d for d in demangled_unref))

    def test_comdats_appear_nowhere_as_owned(self) -> None:
        # LibA_OnlyInline (single-lib COMDAT) and LibA_SharedInline
        # (two-lib COMDAT, COMDAT in both LibA.txt and LibB.txt) must never
        # surface anywhere in the document as an owned symbol: not in any
        # lib's unreferenced[], not in any edge's symbols[], and not in any
        # lib's outbound.symbols_by_target.
        inline_names = ("LibA_OnlyInline", "LibA_SharedInline")

        for lib_name in ("LibA", "LibB", "LibC", "Zenith"):
            lib_doc = _lib(self.doc, lib_name)
            unreferenced = {u["demangled"] for u in lib_doc["unreferenced"]}
            for inline_name in inline_names:
                self.assertFalse(
                    any(inline_name in d for d in unreferenced),
                    f"{inline_name} leaked into {lib_name}.unreferenced",
                )
            for targets in lib_doc["outbound"]["symbols_by_target"].values():
                for s in targets:
                    for inline_name in inline_names:
                        self.assertNotIn(inline_name, s["demangled"])

        for e in self.doc["edges"]:
            for s in e["symbols"]:
                for inline_name in inline_names:
                    self.assertNotIn(inline_name, s["demangled"])

        # Confirmed at the source too: symbols.py itself must keep both
        # names out of `owned` (inline-only) -- the direct COMDAT-rule
        # check this metrics-level assertion is protecting.
        self.assertNotIn("?LibA_OnlyInline@@YAXXZ", self.symtab.owned.get("LibA", {}))
        self.assertNotIn("?LibA_SharedInline@@YAXXZ", self.symtab.owned.get("LibA", {}))
        self.assertNotIn("?LibA_SharedInline@@YAXXZ", self.symtab.owned.get("LibB", {}))


# ---------------------------------------------------------------------------
# 7. Sentinel lower bound.
# ---------------------------------------------------------------------------


class SentinelTests(MetricsFixtureTestCase):
    def test_libB_sentinel_and_lower_bound(self) -> None:
        libB = _lib(self.doc, "LibB")
        self.assertEqual(libB["sentinel"], "SentinelLibB")
        self.assertEqual(libB["outbound"]["lower_bound"], ["LibA", "MidLib"])

    def test_lower_bound_flag_on_libB_edges(self) -> None:
        self.assertTrue(_edge(self.doc, "LibB", "LibA")["lower_bound"])
        self.assertTrue(_edge(self.doc, "LibB", "MidLib")["lower_bound"])
        self.assertFalse(_edge(self.doc, "LibB", "LibC")["lower_bound"])

    def test_sentinel_present_for_fx_debug(self) -> None:
        self.assertNotIn("SentinelLibB", self.model.absent_nodes)


# ---------------------------------------------------------------------------
# 8. Layers.
# ---------------------------------------------------------------------------


class LayerTests(MetricsFixtureTestCase):
    def _layer_of(self, name: str) -> int:
        for n in self.doc["nodes"]:
            if n["name"] == name:
                return n["layer"]
        raise AssertionError(f"node {name!r} not found")

    def test_layers_match_declared_intent(self) -> None:
        # LibA/MidLib/glmx are leaves of the DECLARED graph (layer 0).
        self.assertEqual(self._layer_of("LibA"), 0)
        self.assertEqual(self._layer_of("MidLib"), 0)
        # LibB declares LibA + MidLib (both layer 0) -> layer 1.
        self.assertEqual(self._layer_of("LibB"), 1)
        # LibC declares MidLib (0) + LibB (1) -> layer 2.
        self.assertEqual(self._layer_of("LibC"), 2)
        # LibB and LibC nonetheless share an in-cycle SCC (real use) even
        # though they sit on DIFFERENT layers -- exactly the divergence
        # CONTRACT.md's "How layer is computed" documents.
        in_cycle = {n["name"] for n in self.doc["nodes"] if n["in_cycle"]}
        self.assertEqual(in_cycle, {"LibB", "LibC"})

    def test_contract_recomputes_same_layers(self) -> None:
        # contract.py's own best-effort recomputation must not disagree
        # with what metrics.py produced (covered indirectly by
        # test_validates_with_symbols, asserted explicitly here too).
        errors = contract.validate(self.doc)
        layer_errors = [e for e in errors if "layer is" in e]
        self.assertEqual(layer_errors, [])


# ---------------------------------------------------------------------------
# 9. Determinism.
# ---------------------------------------------------------------------------


class DeterminismTests(MetricsFixtureTestCase):
    def test_two_runs_identical_apart_from_generated(self) -> None:
        import copy
        import json

        doc1 = metrics.compute(self.model, self.graph, self.types, self.symtab, generated="2026-09-23T00:00:00Z")
        doc2 = metrics.compute(self.model, self.graph, self.types, self.symtab, generated="2026-09-23T05:00:00Z")

        d1 = copy.deepcopy(doc1)
        d2 = copy.deepcopy(doc2)
        d1["meta"]["generated"] = ""
        d2["meta"]["generated"] = ""

        self.assertEqual(json.dumps(d1, sort_keys=True), json.dumps(d2, sort_keys=True))
        self.assertNotEqual(doc1["meta"]["generated"], doc2["meta"]["generated"])

    def test_default_generated_is_iso8601_utc(self) -> None:
        doc = metrics.compute(self.model, self.graph, self.types, self.symtab)
        gen = doc["meta"]["generated"]
        self.assertTrue(gen.endswith("Z"))
        # Round-trippable as an ISO-8601 timestamp (minus the trailing Z).
        import datetime

        datetime.datetime.strptime(gen, "%Y-%m-%dT%H:%M:%SZ")


# ---------------------------------------------------------------------------
# Orchestrator-directed refinements (2026-09-23 review): leakage excludes
# game-owned `via_node`s, and an unused-public header whose class is still
# linked from outside gets an honest hint. Neither condition occurs
# anywhere in tests/fixtures/repo (no fixture game header is ever the SOLE
# leaking header for a lib, and no fixture header is both unused-public
# and link-referenced) -- fixtures/repo is owned by W1, so rather than ask
# for a fixture change these are exercised directly against small,
# hand-built in-memory inputs using the real dataclasses from
# model.py/includes.py/cxxtypes.py/symbols.py. Reported to the orchestrator
# as such.
# ---------------------------------------------------------------------------


class SyntheticInputTests(unittest.TestCase):
    def test_game_owned_leaking_header_excluded(self) -> None:
        # GameQ's own header directly includes an L-owned header -- if
        # games weren't excluded this would show up as a "leaking header"
        # entry, but a game is a leaf (nothing else can #include a
        # game-owned header), so it must not.
        nodes = {
            "LibZ": model_mod.Node(name="LibZ", kind="lib"),
            "GameQ": model_mod.Node(name="GameQ", kind="game"),
        }
        files = {
            "Root/LibZ.h": model_mod.FileOwner(path="Root/LibZ.h", node="LibZ", how="listed"),
            "Root/GameQ.h": model_mod.FileOwner(path="Root/GameQ.h", node="GameQ", how="listed"),
        }
        m = model_mod.Model(
            config="synthetic",
            repo_root=".",
            nodes=nodes,
            files=files,
            declared=[],
            orphans=[],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        edge = includes.IncludeEdge(
            src_file="Root/GameQ.h", line=1, raw="LibZ.h", dst_file="Root/LibZ.h", tags=set()
        )
        graph = includes.IncludeGraph(edges=[edge], unresolved=[], ambiguous=[], pch_root=None)
        types = cxxtypes.TypeIndex(decls={}, forward_decls=[], collisions={}, uses=[])

        doc = metrics.compute(m, graph, types, None, generated="2026-09-23T00:00:00Z")
        self.assertEqual(contract.validate(doc), [])

        libZ = _lib(doc, "LibZ")
        self.assertEqual(libZ["leakage"], [])
        self.assertEqual(libZ["inbound"]["leaking_headers"], 0)

    def test_leakage_sorted_by_downstream_count_desc(self) -> None:
        # Two lib-owned leaking headers for the same L: HBig has 2
        # downstream consumers, HSmall has 1. HBig must sort first despite
        # its path sorting alphabetically AFTER HSmall's.
        nodes = {
            "LibZ": model_mod.Node(name="LibZ", kind="lib"),
            "LibHSmall": model_mod.Node(name="LibHSmall", kind="lib"),
            "LibHBig": model_mod.Node(name="LibHBig", kind="lib"),
            "D1": model_mod.Node(name="D1", kind="lib"),
            "D2": model_mod.Node(name="D2", kind="lib"),
        }
        files = {
            "Root/LibZ.h": model_mod.FileOwner(path="Root/LibZ.h", node="LibZ", how="listed"),
            "Root/A_Small.h": model_mod.FileOwner(path="Root/A_Small.h", node="LibHSmall", how="listed"),
            "Root/Z_Big.h": model_mod.FileOwner(path="Root/Z_Big.h", node="LibHBig", how="listed"),
            "Root/D1.cpp": model_mod.FileOwner(path="Root/D1.cpp", node="D1", how="listed"),
            "Root/D2.cpp": model_mod.FileOwner(path="Root/D2.cpp", node="D2", how="listed"),
        }
        edges = [
            includes.IncludeEdge(src_file="Root/A_Small.h", line=1, raw="LibZ.h", dst_file="Root/LibZ.h", tags=set()),
            includes.IncludeEdge(src_file="Root/Z_Big.h", line=1, raw="LibZ.h", dst_file="Root/LibZ.h", tags=set()),
            includes.IncludeEdge(src_file="Root/D1.cpp", line=1, raw="A_Small.h", dst_file="Root/A_Small.h", tags=set()),
            includes.IncludeEdge(src_file="Root/D1.cpp", line=2, raw="Z_Big.h", dst_file="Root/Z_Big.h", tags=set()),
            includes.IncludeEdge(src_file="Root/D2.cpp", line=1, raw="Z_Big.h", dst_file="Root/Z_Big.h", tags=set()),
        ]
        m = model_mod.Model(
            config="synthetic",
            repo_root=".",
            nodes=nodes,
            files=files,
            declared=[],
            orphans=[],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        graph = includes.IncludeGraph(edges=edges, unresolved=[], ambiguous=[], pch_root=None)
        types = cxxtypes.TypeIndex(decls={}, forward_decls=[], collisions={}, uses=[])

        doc = metrics.compute(m, graph, types, None, generated="2026-09-23T00:00:00Z")
        self.assertEqual(contract.validate(doc), [])

        leakage = _lib(doc, "LibZ")["leakage"]
        self.assertEqual([l["header"] for l in leakage], ["Root/Z_Big.h", "Root/A_Small.h"])

    def test_unused_public_with_linked_symbols_hint(self) -> None:
        # Nothing #includes Widget.h directly (zero consumers -> role
        # unused-public), yet LibM references Widget::Foo at link level
        # only -- the real-repo shape is an AI *.Tests.inl compiled into
        # EntityComponent's own .cpp, invisible to consumers[] (which only
        # counts #include lines) but very visible to symbols_out_of_line.
        nodes = {
            "LibZ": model_mod.Node(name="LibZ", kind="lib"),
            "LibM": model_mod.Node(name="LibM", kind="lib"),
        }
        files = {
            "Root/Widget.h": model_mod.FileOwner(path="Root/Widget.h", node="LibZ", how="listed"),
            "Root/M.cpp": model_mod.FileOwner(path="Root/M.cpp", node="LibM", how="listed"),
        }
        m = model_mod.Model(
            config="synthetic",
            repo_root=".",
            nodes=nodes,
            files=files,
            declared=[],
            orphans=[],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        graph = includes.IncludeGraph(edges=[], unresolved=[], ambiguous=[], pch_root=None)
        decl = cxxtypes.TypeDecl(name="Widget", header="Root/Widget.h", node="LibZ", kind="class", forward=False)
        types = cxxtypes.TypeIndex(decls={"Widget": decl}, forward_decls=[], collisions={}, uses=[])

        mangled = "?Foo@Widget@@QEAAXXZ"
        sym = symbols.Symbol(
            mangled=mangled,
            demangled="public: void __cdecl Widget::Foo(void)",
            scopes=["Widget"],
            cls="Widget",
            is_data=False,
            generated=False,
        )
        symtab = symbols.SymbolTable(
            owned={"LibZ": {mangled: sym}},
            inline={},
            undef={"LibM": {mangled}},
            conflicts={},
            stats={},
            missing=[],
        )

        doc = metrics.compute(m, graph, types, symtab, generated="2026-09-23T00:00:00Z")
        self.assertEqual(contract.validate(doc), [])

        h = _header(_lib(doc, "LibZ"), "Root/Widget.h")
        self.assertEqual(h["role"], "unused-public")
        self.assertEqual(h["consumers"], [])
        self.assertEqual(h["symbols_out_of_line"], 1)
        self.assertEqual(
            h["hint"],
            "no outside #include, but 1 out-of-line symbols are linked from outside "
            "(test code or extern declarations): check before moving",
        )


if __name__ == "__main__":
    unittest.main()
