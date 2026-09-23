"""Unit tests for Tools/libgraph/cxxtypes.py.

Run with:
    python -m unittest discover Tools/libgraph/tests -v
    # or, from this directory:
    python -m unittest test_cxxtypes

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

import cxxtypes  # noqa: E402
import includes  # noqa: E402
import model  # noqa: E402

FIXTURE_REPO = _HERE / "fixtures" / "repo"


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(content), encoding="utf-8")


class FixtureTypesTests(unittest.TestCase):
    """Tests against the shared fixtures/repo tree (fx_debug config)."""

    @classmethod
    def setUpClass(cls):
        cls.model = model.build_model(FIXTURE_REPO, "fx_debug")
        cls.graph = includes.build(cls.model)
        cls.index = cxxtypes.build(cls.model, cls.graph)

    def _use(self, consumer_file, type_name):
        matches = [
            u
            for u in self.index.uses
            if u.consumer_file == consumer_file and u.type == type_name
        ]
        self.assertEqual(len(matches), 1, f"expected exactly one use of {type_name} in {consumer_file}, got {matches}")
        return matches[0]

    # -- Declarations ----------------------------------------------------

    def test_decls_include_class_struct_enum_alias_typedef(self):
        d = self.index.decls
        self.assertIn("LibA_Thing", d)
        self.assertEqual(d["LibA_Thing"].kind, "class")
        self.assertIn("LibA_Data", d)
        self.assertEqual(d["LibA_Data"].kind, "struct")
        self.assertIn("LibA_Mode", d)
        self.assertEqual(d["LibA_Mode"].kind, "enum")
        self.assertIn("LibA_Handle", d)
        self.assertEqual(d["LibA_Handle"].kind, "alias")
        self.assertIn("LibA_Count", d)
        self.assertEqual(d["LibA_Count"].kind, "alias")
        for name in ("LibA_Thing", "LibA_Data", "LibA_Mode", "LibA_Handle", "LibA_Count"):
            self.assertEqual(d[name].header, "Zenith/LibA/LibA.h")
            self.assertEqual(d[name].node, "LibA")
            self.assertFalse(d[name].forward)

    def test_libb_widget_declared_with_base(self):
        self.assertIn("LibB_Widget", self.index.decls)
        self.assertEqual(self.index.decls["LibB_Widget"].node, "LibB")
        self.assertEqual(self.index.decls["LibB_Widget"].kind, "class")

    def test_zero_cross_node_uses_for_libamode_and_libacount(self):
        self.assertEqual(self.index.uses_of("LibA_Mode"), [])
        self.assertEqual(self.index.uses_of("LibA_Count"), [])

    def test_midlib_middleware_header_contributes_no_decls(self):
        # MidLib.h ("void MidLib_Init();") is owned by a kind=="middleware"
        # node -- it must not appear as a decl source at all. (It declares
        # no class/struct/enum/alias anyway, but this pins the NODE-KIND
        # filter itself, independent of what the header happens to contain.)
        for decl in self.index.decls.values():
            self.assertNotEqual(self.model.nodes[decl.node].kind, "middleware")
        for decl in self.index.forward_decls:
            self.assertNotEqual(self.model.nodes[decl.node].kind, "middleware")

    # -- Uses --------------------------------------------------------------

    def test_embed_via_member(self):
        u = self._use("Zenith/Shared/LibB_Api.h", "LibA_Data")
        self.assertEqual(u.mode, "embed")
        self.assertEqual(u.header, "Zenith/LibA/LibA.h")

    def test_embed_via_base_class(self):
        u = self._use("Zenith/Shared/LibB_Api.h", "LibA_Thing")
        self.assertEqual(u.mode, "embed")  # base class beats the ref pointer member

    def test_ref_pointer_member_in_gamex(self):
        u = self._use("Games/GameX/Source/GameX_Component.h", "LibB_Widget")
        self.assertEqual(u.mode, "ref")

    def test_gamex_header_embeds_libahandle(self):
        u = self._use("Games/GameX/Source/GameX_Component.h", "LibA_Handle")
        self.assertEqual(u.mode, "embed")

    def test_mention_sizeof_and_by_value_param(self):
        u = self._use("Zenith/Shared/LibC_Impl.cpp", "LibA_Data")
        self.assertEqual(u.mode, "mention")

    def test_forward_decl_recorded_separately_real_repo_shape(self):
        # The shared fixture has no forward declarations at all (verified by
        # the W1 fixture's own README/scenario map) -- exercised with a
        # hand-built temp tree below instead. This is just a guard that the
        # fixture-driven index doesn't fabricate any.
        self.assertEqual(self.index.forward_decls, [])


class ForwardDeclAndCollisionTests(unittest.TestCase):
    """Hand-built temp trees for cases the shared fixture doesn't carry:
    forward declarations (plain and template), a name collision, the
    ZENITH_FINAL/final macro-suffix form (binding amendment 6/7), and the
    lib/game-only decl restriction (middleware excluded, orchestrator
    review point 2)."""

    def _build(self, files: dict[str, str]):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        td_path = Path(td.name)
        for rel, content in files.items():
            _write(td_path / rel, content)
        return td_path

    def _index(self, td_path, nodes, files):
        m = model.Model(
            config="fx_debug",
            repo_root=str(td_path),
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
        graph = includes.build(m)
        return cxxtypes.build(m, graph)

    def test_plain_forward_decl(self):
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\nclass A_Thing;\n",
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=[], source_roots=["Zenith/A"])
        files = {"Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed")}
        idx = self._index(td_path, {"A": a}, files)
        self.assertNotIn("A_Thing", idx.decls)
        self.assertEqual(len(idx.forward_decls), 1)
        self.assertEqual(idx.forward_decls[0].name, "A_Thing")
        self.assertTrue(idx.forward_decls[0].forward)
        self.assertEqual(idx.forward_decls[0].kind, "class")

    def test_template_forward_decl(self):
        # Binding amendment 7: template<typename T> class Zenith_Vector;
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\ntemplate<typename T> class A_Vector;\n",
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=[], source_roots=["Zenith/A"])
        files = {"Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed")}
        idx = self._index(td_path, {"A": a}, files)
        self.assertNotIn("A_Vector", idx.decls)
        names = [d.name for d in idx.forward_decls]
        self.assertIn("A_Vector", names)
        fwd = next(d for d in idx.forward_decls if d.name == "A_Vector")
        self.assertTrue(fwd.forward)
        self.assertEqual(fwd.kind, "class")

    def test_full_template_class_definition_is_a_normal_decl(self):
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\ntemplate<typename T>\nclass A_Vector\n{\n\tT m_xValue;\n};\n",
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=[], source_roots=["Zenith/A"])
        files = {"Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed")}
        idx = self._index(td_path, {"A": a}, files)
        self.assertIn("A_Vector", idx.decls)
        self.assertFalse(idx.decls["A_Vector"].forward)
        # And the template parameter "T" must never be mistaken for a type.
        self.assertNotIn("T", idx.decls)

    def test_zenith_final_macro_suffix_after_name(self):
        # Binding amendment 6: "class X ZENITH_FINAL : public Y" and
        # "class X ZENITH_FINAL" (no base) both parse.
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\nclass A_Base {\n\tint m_i = 0;\n};\n",
                "Zenith/B/B.h": (
                    '#pragma once\n#include "A/A.h"\n'
                    "class B_Thing ZENITH_FINAL : public A_Base\n{\n\tint m_j = 0;\n};\n"
                    "class B_Solo ZENITH_FINAL\n{\n\tint m_k = 0;\n};\n"
                ),
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=["Zenith"], source_roots=["Zenith/A"])
        b = model.Node(name="B", kind="lib", include_dirs=["Zenith"], source_roots=["Zenith/B"])
        files = {
            "Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed"),
            "Zenith/B/B.h": model.FileOwner(path="Zenith/B/B.h", node="B", how="listed"),
        }
        idx = self._index(td_path, {"A": a, "B": b}, files)
        self.assertIn("B_Thing", idx.decls)
        self.assertIn("B_Solo", idx.decls)
        # B_Thing's base list must be exactly A_Base (not "ZENITH_FINAL").
        u = idx.uses_in("Zenith/B/B.h")
        base_use = [x for x in u if x.type == "A_Base"]
        self.assertEqual(len(base_use), 1)
        self.assertEqual(base_use[0].mode, "embed")

    def test_name_collision_keeps_first_by_path_lists_rest(self):
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\nstruct Shared_Thing { int m_i = 0; };\n",
                "Zenith/B/B.h": "#pragma once\nstruct Shared_Thing { int m_j = 0; };\n",
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=[], source_roots=["Zenith/A"])
        b = model.Node(name="B", kind="lib", include_dirs=[], source_roots=["Zenith/B"])
        files = {
            "Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed"),
            "Zenith/B/B.h": model.FileOwner(path="Zenith/B/B.h", node="B", how="listed"),
        }
        idx = self._index(td_path, {"A": a, "B": b}, files)
        self.assertEqual(idx.decls["Shared_Thing"].header, "Zenith/A/A.h")  # first by path
        self.assertEqual(idx.collisions.get("Shared_Thing"), ["Zenith/B/B.h"])

    def test_middleware_header_excluded_from_decls_but_still_a_use_consumer(self):
        # Orchestrator review point 2: a kind=="middleware" header must
        # contribute NO TypeDecl, even though it declares a real
        # class/struct at namespace scope -- and even though the SAME
        # header, as a CONSUMER, can still register a use of a lib's type.
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\nstruct A_Thing { int m_i = 0; };\n",
                "Middleware/Vendor/Vendor.h": (
                    '#pragma once\n#include "A/A.h"\n'
                    "struct Vendor_Internal { int m_j = 0; };\n"
                    "struct Vendor_User\n{\n\tA_Thing m_xThing;\n};\n"
                ),
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=["Zenith"], source_roots=["Zenith/A"])
        vendor = model.Node(
            name="Vendor", kind="middleware", include_dirs=["Zenith"], source_roots=["Middleware/Vendor"]
        )
        files = {
            "Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed"),
            "Middleware/Vendor/Vendor.h": model.FileOwner(
                path="Middleware/Vendor/Vendor.h", node="Vendor", how="listed"
            ),
        }
        idx = self._index(td_path, {"A": a, "Vendor": vendor}, files)
        # The middleware header's OWN types never become decls...
        self.assertNotIn("Vendor_Internal", idx.decls)
        self.assertNotIn("Vendor_User", idx.decls)
        # ...but the lib's type is still a decl, ...
        self.assertIn("A_Thing", idx.decls)
        # ...and the middleware header still shows up as a USE consumer of it.
        uses = idx.uses_in("Middleware/Vendor/Vendor.h")
        self.assertEqual(len(uses), 1)
        self.assertEqual(uses[0].type, "A_Thing")
        self.assertEqual(uses[0].mode, "embed")

    def test_vendored_tree_under_a_lib_source_root_still_excluded(self):
        # The assimp case: a vendored header bundled directly into a real
        # kind=="lib" node's OWN file listing (mirroring ZenithTools/Tools
        # owning Tools/Middleware/assimp/**) must still be excluded, because
        # it sits under a Middleware/ path segment -- the node-kind check
        # alone is not enough here.
        td_path = self._build(
            {
                "Tools/Middleware/vendor/vendor.h": "#pragma once\nstruct Vendor_Thing { int m_i = 0; };\n",
            }
        )
        tools = model.Node(
            name="ZTools", kind="lib", include_dirs=[], source_roots=["Tools"]
        )
        files = {
            "Tools/Middleware/vendor/vendor.h": model.FileOwner(
                path="Tools/Middleware/vendor/vendor.h", node="ZTools", how="listed"
            )
        }
        idx = self._index(td_path, {"ZTools": tools}, files)
        self.assertNotIn("Vendor_Thing", idx.decls)

    def test_game_header_still_contributes_decls(self):
        # "plus games": a kind=="game" node's headers still declare types.
        td_path = self._build(
            {
                "Games/G/Source/G_Thing.h": "#pragma once\nstruct G_Thing { int m_i = 0; };\n",
            }
        )
        g = model.Node(name="G", kind="game", include_dirs=[], source_roots=["Games/G"])
        files = {
            "Games/G/Source/G_Thing.h": model.FileOwner(
                path="Games/G/Source/G_Thing.h", node="G", how="listed"
            )
        }
        idx = self._index(td_path, {"G": g}, files)
        self.assertIn("G_Thing", idx.decls)
        self.assertEqual(idx.decls["G_Thing"].node, "G")

    def test_lib_decl_wins_over_game_decl_regardless_of_path_order(self):
        # Real-repo shape (orchestrator review follow-up): Zenith_Maths::Quat
        # (Zenith/Maths/Zenith_Maths.h, a LIB) collides with DevilsPlayground's
        # own global-scope re-export `using Quat = Zenith_Maths::Quat;`
        # (Games/DevilsPlayground/Source/DPCommonTypes.h, a GAME). "Games/..."
        # sorts alphabetically BEFORE "Zenith/..." -- a plain path-order
        # tie-break would wrongly hand the canonical decl to the game. The
        # lib must win regardless.
        td_path = self._build(
            {
                "Games/AGame/Source/AGame_Types.h": "#pragma once\nusing Quat = int;\n",
                "Zenith/Maths/Zenith_Maths.h": "#pragma once\nusing Quat = float;\n",
            }
        )
        game = model.Node(name="AGame", kind="game", include_dirs=[], source_roots=["Games/AGame"])
        maths = model.Node(name="ZenithMaths", kind="lib", include_dirs=[], source_roots=["Zenith/Maths"])
        files = {
            "Games/AGame/Source/AGame_Types.h": model.FileOwner(
                path="Games/AGame/Source/AGame_Types.h", node="AGame", how="listed"
            ),
            "Zenith/Maths/Zenith_Maths.h": model.FileOwner(
                path="Zenith/Maths/Zenith_Maths.h", node="ZenithMaths", how="listed"
            ),
        }
        idx = self._index(td_path, {"AGame": game, "ZenithMaths": maths}, files)
        # "Games/AGame/Source/AGame_Types.h" < "Zenith/Maths/Zenith_Maths.h"
        # by plain path sort -- confirms this test actually exercises the
        # tie-break rather than agreeing with path order by accident.
        self.assertLess("Games/AGame/Source/AGame_Types.h", "Zenith/Maths/Zenith_Maths.h")
        self.assertEqual(idx.decls["Quat"].header, "Zenith/Maths/Zenith_Maths.h")
        self.assertEqual(idx.decls["Quat"].node, "ZenithMaths")
        self.assertEqual(idx.collisions.get("Quat"), ["Games/AGame/Source/AGame_Types.h"])

    def test_lib_vs_lib_tie_break_is_still_pure_path_order(self):
        # The kind-rank tie-break must not disturb lib-vs-lib (or
        # game-vs-game) ordering -- those stay resolved by path alone.
        td_path = self._build(
            {
                "Zenith/A/A.h": "#pragma once\nstruct Shared_Thing { int m_i = 0; };\n",
                "Zenith/B/B.h": "#pragma once\nstruct Shared_Thing { int m_j = 0; };\n",
            }
        )
        a = model.Node(name="A", kind="lib", include_dirs=[], source_roots=["Zenith/A"])
        b = model.Node(name="B", kind="lib", include_dirs=[], source_roots=["Zenith/B"])
        files = {
            "Zenith/A/A.h": model.FileOwner(path="Zenith/A/A.h", node="A", how="listed"),
            "Zenith/B/B.h": model.FileOwner(path="Zenith/B/B.h", node="B", how="listed"),
        }
        idx = self._index(td_path, {"A": a, "B": b}, files)
        self.assertEqual(idx.decls["Shared_Thing"].header, "Zenith/A/A.h")  # first by path, as before
        self.assertEqual(idx.collisions.get("Shared_Thing"), ["Zenith/B/B.h"])


class ModeClassifierInlineTests(unittest.TestCase):
    """Small inline-string tests for the declaration scanner and mode
    classifier building blocks, independent of any Model/IncludeGraph."""

    def test_namespace_braces_do_not_count_as_class_scope(self):
        text = "namespace NS\n{\nclass Inner\n{\n\tint m_i = 0;\n};\n}\n"
        decls = cxxtypes._scan_declarations(text)
        names = [d["name"] for d in decls]
        self.assertIn("Inner", names)

    def test_class_nested_declaration_excluded(self):
        text = "class Outer\n{\n\tstruct Nested { int m_i = 0; };\n};\n"
        decls = cxxtypes._scan_declarations(text)
        names = [d["name"] for d in decls]
        self.assertIn("Outer", names)
        self.assertNotIn("Nested", names)

    def test_enum_class_not_double_counted_as_a_class(self):
        text = "enum class Mode\n{\n\tA,\n\tB,\n};\n"
        decls = cxxtypes._scan_declarations(text)
        kinds = {d["name"]: d["kind"] for d in decls}
        self.assertEqual(kinds.get("Mode"), "enum")

    def test_base_identifiers_strip_qualifiers(self):
        result = cxxtypes._extract_base_identifiers(" public virtual Base1, private Base2")
        self.assertEqual(result, ["Base1", "Base2"])

    def test_template_param_blanking_prevents_class_t_confusion(self):
        text = "template<class T> class Vector\n{\n\tT m_x;\n};\n"
        blanked = cxxtypes._blank_template_param_lists(text)
        decls = cxxtypes._scan_declarations(blanked)
        names = [d["name"] for d in decls]
        self.assertIn("Vector", names)
        self.assertNotIn("T", names)

    def test_typedef_function_pointer_extracts_the_alias_not_a_param_name(self):
        # Real-repo shape: Zenith/SaveData/Zenith_SaveData.h:92 --
        # `typedef void(*SaveWriteCallback)(Zenith_DataStream&, void*
        # pxUserData);` -- the alias is "SaveWriteCallback", not the
        # trailing parameter name "pxUserData".
        body = " void(*SaveWriteCallback)(Zenith_DataStream& xStream, void* pxUserData)"
        self.assertEqual(cxxtypes._typedef_alias_name(body), "SaveWriteCallback")

    def test_typedef_function_pointer_no_args(self):
        body = " int(*Callback)()"
        self.assertEqual(cxxtypes._typedef_alias_name(body), "Callback")

    def test_typedef_array_extracts_name_not_size(self):
        body = " int Buffer[16]"
        self.assertEqual(cxxtypes._typedef_alias_name(body), "Buffer")

    def test_typedef_simple_form_still_takes_last_identifier(self):
        body = " int LibA_Count"
        self.assertEqual(cxxtypes._typedef_alias_name(body), "LibA_Count")

    def test_typedef_function_pointer_scanned_end_to_end_via_scan_declarations(self):
        text = (
            "#pragma once\n"
            "typedef void(*SaveWriteCallback)(int xStream, void* pxUserData);\n"
        )
        decls = cxxtypes._scan_declarations(text)
        names = [d["name"] for d in decls]
        self.assertIn("SaveWriteCallback", names)
        self.assertNotIn("pxUserData", names)


if __name__ == "__main__":
    unittest.main()
