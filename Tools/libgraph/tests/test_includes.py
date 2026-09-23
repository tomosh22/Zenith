"""Unit tests for Tools/libgraph/includes.py.

Run with:
    python -m unittest discover Tools/libgraph/tests -v
    # or, from this directory:
    python -m unittest test_includes

Pure stdlib (unittest). Mirrors Tools/tests/test_complexity.py's sys.path
setup (see test_model.py) so the flat sibling modules import cleanly
regardless of CWD.
"""

from __future__ import annotations

import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))

import includes  # noqa: E402
import model  # noqa: E402

FIXTURE_REPO = _HERE / "fixtures" / "repo"


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(content), encoding="utf-8")


class FixtureIncludesTests(unittest.TestCase):
    """Tests against the shared fixtures/repo tree (fx_debug config)."""

    @classmethod
    def setUpClass(cls):
        cls.model = model.build_model(FIXTURE_REPO, "fx_debug")
        cls.graph = includes.build(cls.model)

    def _edge(self, src, dst):
        for e in self.graph.out_edges(src):
            if e.dst_file == dst:
                return e
        return None

    def test_parent_stack_resolves(self):
        # Scenario 10: LibB_Sibling.h only exists at Zenith/Shared/, which is
        # in no project's include dirs -- resolvable only via the parent
        # stack (LibB_Api.h's own directory, on the stack because
        # LibB_Inner.h was opened from it).
        e = self._edge("Zenith/Shared/detail/LibB_Inner.h", "Zenith/Shared/LibB_Sibling.h")
        self.assertIsNotNone(e)
        self.assertEqual(e.raw, "LibB_Sibling.h")

    def test_ambiguous_fxconfig_reported(self):
        # Scenario 16.
        matches = [
            a
            for a in self.graph.ambiguous
            if a["file"] == "Zenith/Shared/LibC_Impl.cpp" and a["raw"] == "FxConfig.h"
        ]
        self.assertEqual(len(matches), 1)
        self.assertEqual(
            matches[0]["candidates"],
            ["Zenith/FxConfig.h", "Zenith/LibA/FxConfig.h"],
        )
        e = self._edge("Zenith/Shared/LibC_Impl.cpp", "Zenith/FxConfig.h")
        self.assertIsNotNone(e)  # first candidate wins

    def test_conditional_tag_set(self):
        # Scenario 15: genuine #ifdef FX_TOOLS conditional use.
        e = self._edge("Zenith/Shared/LibB_Impl.cpp", "Zenith/LibA/LibA_Extra.h")
        self.assertIsNotNone(e)
        self.assertIn("conditional", e.tags)

    def test_non_conditional_edge_has_no_tag(self):
        e = self._edge("Zenith/Shared/LibB_Impl.cpp", "Zenith/Shared/LibC_Api.h")
        self.assertIsNotNone(e)
        self.assertNotIn("conditional", e.tags)

    def test_test_tag_src_side(self):
        # Scenario 13: LibA.Tests.inl (src) -> LibC_Api.h.
        e = self._edge("Zenith/LibA/LibA.Tests.inl", "Zenith/Shared/LibC_Api.h")
        self.assertIsNotNone(e)
        self.assertIn("test", e.tags)

    def test_test_tag_dst_side(self):
        # Scenario 24 (binding amendment 1): LibA.cpp -> LibB_Api.Tests.inl
        # (dst side), the mirror image of scenario 13.
        e = self._edge("Zenith/LibA/LibA.cpp", "Zenith/Shared/LibB_Api.Tests.inl")
        self.assertIsNotNone(e)
        self.assertIn("test", e.tags)

    def test_pch_tag_direct_include(self):
        e = self._edge("Zenith/LibA/LibA.cpp", "Zenith/LibA/FxPch.h")
        self.assertIsNotNone(e)
        self.assertIn("pch", e.tags)

    def test_pch_tag_games_include_pch_literally(self):
        # Games use PrecompiledHeader=NotUsing but still #include "FxPch.h".
        e = self._edge("Games/GameX/Source/GameX.cpp", "Zenith/LibA/FxPch.h")
        self.assertIsNotNone(e)
        self.assertIn("pch", e.tags)

    def test_pch_tag_internal_pch_edge(self):
        # The PCH's own internal edge (FxPch.h -> MidLib/MidLib.h) is tagged
        # pch too, because src_file == pch_root.
        e = self._edge("Zenith/LibA/FxPch.h", "Middleware/MidLib/MidLib.h")
        self.assertIsNotNone(e)
        self.assertIn("pch", e.tags)

    def test_orphans_includes_parsed(self):
        # Scenario 19: the orphan is in scope and parses cleanly (it has no
        # #include lines of its own).
        self.assertEqual(self.graph.out_edges("Zenith/LibA/LibA_Orphan.h"), [])
        # And it's still reachable as a dst if anything included it (nothing
        # does here) -- just confirm no crash / no stray unresolved entries
        # naming it as a src.
        for src, _line, _raw in self.graph.unresolved:
            self.assertNotEqual(src, "Zenith/LibA/LibA_Orphan.h")

    def test_closure_vs_closure_no_pch_differ_by_pch_subtree(self):
        # LibC_Impl.cpp has no INDEPENDENT reach into MidLib -- unlike
        # LibB_Impl.cpp (scenario 25), so the difference is exactly the PCH
        # root plus its own closure.
        f = "Zenith/Shared/LibC_Impl.cpp"
        full = self.graph.closure(f)
        no_pch = self.graph.closure_no_pch(f)
        pch_root = self.model.pch_root
        expected_removed = {pch_root} | self.graph.closure(pch_root)
        self.assertEqual(full - no_pch, expected_removed)
        self.assertEqual(no_pch | expected_removed, full)

    def test_scenario_25_direct_reach_survives_closure_no_pch(self):
        f = "Zenith/Shared/LibB_Impl.cpp"
        no_pch = self.graph.closure_no_pch(f)
        full = self.graph.closure(f)
        pch_root = self.model.pch_root
        # MidLib.h is reachable BOTH via the PCH and directly -- must
        # survive the no-pch traversal.
        self.assertIn("Middleware/MidLib/MidLib.h", full)
        self.assertIn("Middleware/MidLib/MidLib.h", no_pch)
        # But the PCH root itself, reached only via the blocked edge, must
        # not.
        self.assertIn(pch_root, full)
        self.assertNotIn(pch_root, no_pch)

    def test_glmx_third_party_resolves(self):
        # Scenario 21.
        e = self._edge("Zenith/LibA/LibA.h", "Middleware/glmx/glmx.hpp")
        self.assertIsNotNone(e)

    def test_split_source_root_parent_include_resolves_across_dir(self):
        # Scenario 9/10 combined smoke: LibB_Api.h -> detail/LibB_Inner.h
        # resolves via LibB_Api.h's OWN directory (step 1), no fallback
        # needed.
        e = self._edge("Zenith/Shared/LibB_Api.h", "Zenith/Shared/detail/LibB_Inner.h")
        self.assertIsNotNone(e)

    def test_no_edges_or_unresolved_reference_unknown_files(self):
        # Sanity: every resolved dst_file is a real file in the universe.
        universe = set(self.model.files.keys()) | set(self.model.orphans)
        for e in self.graph.edges:
            self.assertIn(e.dst_file, universe)


class HandBuiltModelTests(unittest.TestCase):
    """Synthetic Model instances for cases the shared fixture can't (or
    shouldn't) exercise: amendment 5 (owner-less orphans), amendment 3
    (case-insensitive resolution), amendment 4 (../ normalisation)."""

    def _build(self, files: dict[str, str]):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        td_path = Path(td.name)
        for rel, content in files.items():
            _write(td_path / rel, content)
        return td_path

    def test_amendment5_unowned_orphan_skips_node_include_dirs(self):
        # "Config.h" exists at Zenith/Elsewhere/ (only reachable through
        # SomeLib's own include_dirs) and NOT anywhere on Stray.h's own
        # directory or parent stack. Stray.h is an orphan with NO owner
        # (its directory matches no node's source_roots) -- so its
        # "#include \"Config.h\"" must NOT resolve via SomeLib's
        # include_dirs (step 3 is unavailable to an unowned orphan), and
        # must end up unresolved.
        td_path = self._build(
            {
                "Zenith/Elsewhere/Config.h": "#pragma once\n",
                "Zenith/Stray/Stray.h": '#include "Config.h"\n',
            }
        )
        some_lib = model.Node(
            name="SomeLib",
            kind="lib",
            include_dirs=["Zenith/Elsewhere"],
            source_roots=["Zenith/Elsewhere"],
        )
        files = {
            "Zenith/Elsewhere/Config.h": model.FileOwner(
                path="Zenith/Elsewhere/Config.h", node="SomeLib", how="listed"
            ),
        }
        m = model.Model(
            config="fx_debug",
            repo_root=str(td_path),
            nodes={"SomeLib": some_lib},
            files=files,
            declared=[],
            orphans=["Zenith/Stray/Stray.h"],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        self.assertNotIn("Zenith/Stray/Stray.h", m.files)  # truly unowned
        graph = includes.build(m)
        self.assertEqual(graph.out_edges("Zenith/Stray/Stray.h"), [])
        self.assertEqual(
            [(s, r) for (s, _l, r) in graph.unresolved],
            [("Zenith/Stray/Stray.h", "Config.h")],
        )

    def test_amendment5_unowned_orphan_own_directory_still_works(self):
        # Steps 1 (own dir) and 2 (parent stack) DO still apply to an
        # unowned orphan -- only step 3 (node include_dirs) is unavailable.
        td_path = self._build(
            {
                "Zenith/Stray/Stray.h": '#include "Other.h"\n',
                "Zenith/Stray/Other.h": "#pragma once\n",
            }
        )
        m = model.Model(
            config="fx_debug",
            repo_root=str(td_path),
            nodes={},
            files={},
            declared=[],
            orphans=["Zenith/Stray/Stray.h", "Zenith/Stray/Other.h"],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        graph = includes.build(m)
        edges = graph.out_edges("Zenith/Stray/Stray.h")
        self.assertEqual(len(edges), 1)
        self.assertEqual(edges[0].dst_file, "Zenith/Stray/Other.h")

    def test_amendment3_case_insensitive_resolution_canonical_case_recorded(self):
        td_path = self._build(
            {
                "Zenith/Foo/Bar.h": "#pragma once\n",
                "Zenith/Foo/Baz.h": '#include "bar.h"\n',
            }
        )
        foo = model.Node(name="Foo", kind="lib", include_dirs=[], source_roots=["Zenith/Foo"])
        files = {
            "Zenith/Foo/Bar.h": model.FileOwner(path="Zenith/Foo/Bar.h", node="Foo", how="listed"),
            "Zenith/Foo/Baz.h": model.FileOwner(path="Zenith/Foo/Baz.h", node="Foo", how="listed"),
        }
        m = model.Model(
            config="fx_debug",
            repo_root=str(td_path),
            nodes={"Foo": foo},
            files=files,
            declared=[],
            orphans=[],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        graph = includes.build(m)
        e = graph.out_edges("Zenith/Foo/Baz.h")[0]
        self.assertEqual(e.raw, "bar.h")
        self.assertEqual(e.dst_file, "Zenith/Foo/Bar.h")  # canonical on-disk case

    def test_amendment4_dotdot_include_normalised(self):
        td_path = self._build(
            {
                "Zenith/Editor/Panels/Panel.cpp": '#include "../../../Tools/Export.h"\n',
                "Tools/Export.h": "#pragma once\n",
            }
        )
        editor = model.Node(
            name="Editor", kind="lib", include_dirs=[], source_roots=["Zenith/Editor"]
        )
        files = {
            "Zenith/Editor/Panels/Panel.cpp": model.FileOwner(
                path="Zenith/Editor/Panels/Panel.cpp", node="Editor", how="listed"
            ),
            "Tools/Export.h": model.FileOwner(path="Tools/Export.h", node="Editor", how="dir-fallback"),
        }
        m = model.Model(
            config="fx_debug",
            repo_root=str(td_path),
            nodes={"Editor": editor},
            files=files,
            declared=[],
            orphans=[],
            duplicates=[],
            allowlist={},
            pch_root=None,
            unresolved_conditions=[],
            absent_nodes=[],
        )
        graph = includes.build(m)
        e = graph.out_edges("Zenith/Editor/Panels/Panel.cpp")[0]
        self.assertEqual(e.dst_file, "Tools/Export.h")


class CommentAndConditionalParserTests(unittest.TestCase):
    """Small inline-string tests for strip_comments()/
    compute_conditional_lines() -- the comment/guard/conditional parser."""

    def test_strip_line_comment(self):
        text = '#include "A.h" // not "B.h"\n'
        stripped = includes.strip_comments(text)
        self.assertIn('#include "A.h"', stripped)
        self.assertNotIn("B.h", stripped)

    def test_strip_block_comment_preserves_newlines(self):
        text = "before /* comment\nspanning\nlines */ after\n"
        stripped = includes.strip_comments(text)
        self.assertEqual(stripped.count("\n"), text.count("\n"))
        self.assertNotIn("spanning", stripped)
        self.assertIn("before", stripped)
        self.assertIn("after", stripped)

    def test_string_literal_not_treated_as_comment(self):
        text = 'const char* s = "// not a comment";\n#include "Real.h"\n'
        stripped = includes.strip_comments(text)
        self.assertIn('#include "Real.h"', stripped)
        self.assertIn("not a comment", stripped)

    def test_conditional_nesting_basic(self):
        text = "#pragma once\n#ifdef X\n#include \"A.h\"\n#endif\n#include \"B.h\"\n"
        lines = includes.compute_conditional_lines(text)
        self.assertIn(3, lines)  # the #include "A.h" line
        self.assertNotIn(5, lines)  # the #include "B.h" line, after #endif

    def test_top_of_file_include_guard_is_exempt(self):
        text = (
            "#ifndef FOO_H\n"
            "#define FOO_H\n"
            '#include "A.h"\n'
            "#endif\n"
        )
        lines = includes.compute_conditional_lines(text)
        self.assertNotIn(3, lines)  # inside the guard, but exempt

    def test_nested_conditional_inside_guard_still_counts(self):
        text = (
            "#ifndef FOO_H\n"
            "#define FOO_H\n"
            "#ifdef BAR\n"
            '#include "A.h"\n'
            "#endif\n"
            "#endif\n"
        )
        lines = includes.compute_conditional_lines(text)
        self.assertIn(4, lines)  # nested inside a REAL conditional, even
        # though it's also inside the exempt guard

    def test_ifndef_not_followed_by_matching_define_is_not_a_guard(self):
        text = (
            "#ifndef FOO_H\n"
            "#define BAR_H\n"  # different macro name -- not a guard pair
            '#include "A.h"\n'
            "#endif\n"
        )
        lines = includes.compute_conditional_lines(text)
        self.assertIn(3, lines)  # counts as a real conditional


if __name__ == "__main__":
    unittest.main()
