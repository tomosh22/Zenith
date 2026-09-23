"""Unit tests for Tools/libgraph/symbols.py.

Run with:
    python -m unittest discover Tools/libgraph/tests -p "test_symbols.py" -v
    # or, from this directory:
    python -m unittest test_symbols

Pure stdlib (unittest). Mirrors Tools/tests/test_complexity.py's sys.path
setup so the flat sibling modules (model/vcxproj/sharpmake/symbols, no
package, no __init__.py) import cleanly regardless of CWD.
"""

import sys
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))

import model  # noqa: E402
import symbols  # noqa: E402

FIXTURE_REPO = _HERE / "fixtures" / "repo"
FIXTURE_DUMPBIN = _HERE / "fixtures" / "dumpbin"
REAL_EXCERPT = FIXTURE_DUMPBIN / "_real_excerpt.txt"


# ---------------------------------------------------------------------------
# Parser tests: the real excerpt.
# ---------------------------------------------------------------------------


class RealExcerptTests(unittest.TestCase):
    """tests/fixtures/dumpbin/_real_excerpt.txt is a genuine, unmodified
    slice of `dumpbin /symbols` run on the real
    Build/output/.../vulkan_vs2022_debug_win64_true/zenithai.lib (one
    complete COFF SYMBOL TABLE block, trimmed at the block boundary)."""

    @classmethod
    def setUpClass(cls):
        text = REAL_EXCERPT.read_text(encoding="utf-8")
        cls.table = symbols.parse_dumpbin_text("zenithai", text)
        cls.stats = cls.table.stats["zenithai"]

    def test_sane_counts(self):
        # external_defs == owned + comdat_lines, always.
        self.assertEqual(
            self.stats["external_defs"], self.stats["owned"] + self.stats["comdat_lines"]
        )
        self.assertGreater(self.stats["owned"], 0)
        self.assertGreater(self.stats["comdat_lines"], 0)
        self.assertGreater(self.stats["undef"], 0)
        # inline_unique is a dedup of comdat_lines, so <= it.
        self.assertLessEqual(self.stats["inline_unique"], self.stats["comdat_lines"])

    def test_known_real_owned_symbol(self):
        owned = self.table.owned["zenithai"]
        sym = owned["?Update@Zenith_AI@@YAXM@Z"]
        self.assertEqual(sym.demangled, "void __cdecl Zenith_AI::Update(float)")
        self.assertEqual(sym.scopes, ["Zenith_AI"])
        self.assertEqual(sym.cls, "Zenith_AI")
        self.assertFalse(sym.is_data)
        self.assertFalse(sym.generated)

    def test_known_real_undef(self):
        self.assertIn("__CheckForDebuggerJustMyCode", self.table.undef["zenithai"])

    def test_known_real_comdat_ctor_not_owned(self):
        # A ctor/dtor pair defined via an in-class definition -- COMDAT, so
        # it must show up as inline, not owned.
        owned = self.table.owned["zenithai"]
        inline = self.table.inline["zenithai"]
        self.assertIn("??0Zenith_AIManagerTestScope@@QEAA@XZ", inline)
        self.assertNotIn("??0Zenith_AIManagerTestScope@@QEAA@XZ", owned)

    def test_generated_vftable_like_symbols_skipped_or_inline(self):
        # Whatever generated symbols this block carries must never appear
        # owned (COMDAT keeps almost all of them out, per the critic's
        # finding referenced in the brief).
        owned = self.table.owned["zenithai"]
        for mangled in owned:
            self.assertFalse(
                mangled.startswith(("??_7", "??_R", "??_C@", "__real@", "??__E", "??__F")),
                mangled,
            )


# ---------------------------------------------------------------------------
# Parser tests: inline synthetic dumpbin-format text.
# ---------------------------------------------------------------------------


def _block(*record_lines: str, header: str = "COFF SYMBOL TABLE") -> str:
    return header + "\n" + "\n".join(record_lines) + "\n"


class ComdatPerBlockTests(unittest.TestCase):
    def test_same_sect_number_comdat_in_one_block_not_next(self):
        text = (
            "COFF SYMBOL TABLE\n"
            "003 00000000 SECT5  notype       Static       | .text$mn\n"
            "    Section length   30, #relocs    0, #linenums    0, checksum 11111111, selection    2 (pick any)\n"
            "004 00000000 SECT5  notype ()    External     | ?Foo@@YAXXZ (void __cdecl Foo(void))\n"
            "\n"
            "String Table Size = 0x10 bytes\n"
            "\n"
            "COFF SYMBOL TABLE\n"
            "003 00000000 SECT5  notype       Static       | .text$mn\n"
            "    Section length   40, #relocs    0, #linenums    0, checksum 22222222\n"
            "004 00000000 SECT5  notype ()    External     | ?Bar@@YAXXZ (void __cdecl Bar(void))\n"
        )
        t = symbols.parse_dumpbin_text("n", text)
        self.assertIn("?Foo@@YAXXZ", t.inline["n"])
        self.assertNotIn("?Foo@@YAXXZ", t.owned["n"])
        self.assertIn("?Bar@@YAXXZ", t.owned["n"])
        self.assertNotIn("?Bar@@YAXXZ", t.inline["n"])


class OwnedInlineUndefTests(unittest.TestCase):
    def test_basic_classification(self):
        text = _block(
            "003 00000000 SECT1  notype       Static       | .text$mn",
            "    Section length   30, #relocs    0, #linenums    0, checksum 11111111",
            "004 00000000 SECT1  notype ()    External     | ?Owned@@YAXXZ (void __cdecl Owned(void))",
            "005 00000000 SECT2  notype       Static       | .text$mn",
            "    Section length   30, #relocs    0, #linenums    0, checksum 22222222, selection    2 (pick any)",
            "006 00000000 SECT2  notype ()    External     | ?Inline@@YAXXZ (void __cdecl Inline(void))",
            "007 00000000 UNDEF  notype ()    External     | ?Undef@@YAXXZ (void __cdecl Undef(void))",
        )
        t = symbols.parse_dumpbin_text("n", text)
        self.assertIn("?Owned@@YAXXZ", t.owned["n"])
        self.assertIn("?Inline@@YAXXZ", t.inline["n"])
        self.assertIn("?Undef@@YAXXZ", t.undef["n"])
        st = t.stats["n"]
        self.assertEqual(st["owned"], 1)
        self.assertEqual(st["comdat_lines"], 1)
        self.assertEqual(st["inline_unique"], 1)
        self.assertEqual(st["undef"], 1)
        self.assertEqual(st["external_defs"], 2)


class NoParenthesisNameTests(unittest.TestCase):
    def test_demangled_equals_mangled(self):
        text = _block(
            "003 00000000 UNDEF  notype ()    External     | atexit",
            "004 00000000 UNDEF  notype       External     | __security_cookie",
        )
        t = symbols.parse_dumpbin_text("n", text)
        self.assertIn("atexit", t.undef["n"])
        self.assertIn("__security_cookie", t.undef["n"])


class WidthVarianceTests(unittest.TestCase):
    def test_differing_sect_column_widths_parse_by_whitespace(self):
        text = _block(
            "003 00000000 SECT5  notype       Static       | .text$mn",
            "    Section length   30, #relocs    0, #linenums    0, checksum 11111111",
            "004 00000000 SECT5  notype ()    External     | ?Narrow@@YAXXZ (void __cdecl Narrow(void))",
            "141C 00000000 SECT23B notype       Static       | .text$mn",
            "    Section length   40, #relocs    0, #linenums    0, checksum 22222222",
            "141D 00000000 SECT23B notype ()    External    | ?Wide@@YAXXZ (void __cdecl Wide(void))",
        )
        t = symbols.parse_dumpbin_text("n", text)
        self.assertIn("?Narrow@@YAXXZ", t.owned["n"])
        self.assertIn("?Wide@@YAXXZ", t.owned["n"])


class SkipOtherSectionKindsTests(unittest.TestCase):
    def test_abs_and_label_and_debug_skipped(self):
        text = _block(
            "000 0105899A ABS    notype       Static       | @comp.id",
            "005 00000000 SECT3  notype       Label        | $LN3",
            "006 00000000 DEBUG  notype       Static       | .debug$S",
        )
        t = symbols.parse_dumpbin_text("n", text)
        self.assertEqual(t.owned["n"], {})
        self.assertEqual(t.inline["n"], set())
        self.assertEqual(t.undef["n"], set())
        self.assertEqual(t.stats["n"]["external_defs"], 0)


# ---------------------------------------------------------------------------
# Demangled -> scopes, the tricky forms.
# ---------------------------------------------------------------------------


class ScopeExtractionTests(unittest.TestCase):
    def _owned_sym(self, mangled: str, demangled: str, is_func: bool = True):
        type_field = "notype ()" if is_func else "notype      "
        text = _block(
            "003 00000000 SECT1  notype       Static       | .text$mn",
            "    Section length   30, #relocs    0, #linenums    0, checksum 11111111",
            f"004 00000000 SECT1  {type_field}    External     | {mangled} ({demangled})",
        )
        t = symbols.parse_dumpbin_text("n", text)
        return t.owned["n"][mangled]

    def test_constructor(self):
        sym = self._owned_sym("??0X@@QEAA@XZ", "public: __cdecl X::X(void)")
        self.assertEqual(sym.scopes, ["X"])
        self.assertEqual(sym.cls, "X")

    def test_destructor(self):
        sym = self._owned_sym("??1X@@QEAA@XZ", "public: __cdecl X::~X(void)")
        self.assertEqual(sym.scopes, ["X"])
        self.assertEqual(sym.cls, "X")

    def test_operator_assign(self):
        sym = self._owned_sym(
            "??4X@@QEAAAEAV0@AEBV0@@Z",
            "public: class X & __cdecl X::operator=(class X const &)",
        )
        self.assertEqual(sym.scopes, ["X"])
        self.assertEqual(sym.cls, "X")

    def test_template_scope_strips_args(self):
        sym = self._owned_sym(
            "?Bar@?$Foo@H@@QEAAXXZ",
            "public: void __cdecl Foo<int>::Bar(void)",
        )
        self.assertEqual(sym.scopes, ["Foo"])
        self.assertEqual(sym.cls, "Foo")

    def test_static_data_member(self):
        sym = self._owned_sym(
            "?s_iCount@X@@2HA", "static int X::s_iCount", is_func=False
        )
        self.assertEqual(sym.scopes, ["X"])
        self.assertEqual(sym.cls, "X")

    def test_free_function_no_scope(self):
        sym = self._owned_sym("?SomeFreeFunction@@YAXXZ", "void __cdecl SomeFreeFunction(void)")
        self.assertEqual(sym.scopes, [])
        self.assertIsNone(sym.cls)

    def test_nested_namespace_free_function(self):
        sym = self._owned_sym(
            "?f@Inner@Outer@NS@@YAXXZ", "void __cdecl NS::Outer::Inner::f(void)"
        )
        self.assertEqual(sym.scopes, ["NS", "Outer", "Inner"])
        self.assertEqual(sym.cls, "Inner")

    def test_anonymous_namespace_real(self):
        sym = self._owned_sym(
            "?EngineTickEnabledRef@?A0x70b5e259@@YAAEA_NXZ",
            "bool & __cdecl `anonymous namespace'::EngineTickEnabledRef(void)",
        )
        self.assertEqual(sym.scopes, ["`anonymous namespace'"])
        self.assertIsNone(sym.cls)

    def test_lambda_operator_call_real(self):
        sym = self._owned_sym(
            "??R<lambda_1>@?1??Update@Zenith_TacticalPointSystem@@SAXXZ@QEBAIXZ",
            "public: unsigned int __cdecl `public: static void __cdecl "
            "Zenith_TacticalPointSystem::Update(void)'::`2'::<lambda_1>::operator()(void)const ",
        )
        self.assertEqual(sym.cls, "Zenith_TacticalPointSystem")

    def test_dynamic_initializer_is_generated_not_demangle_parsed(self):
        sym = self._owned_sym(
            "??__Eg_xSomeStaticInit@@YAXXZ",
            "void __cdecl `dynamic initializer for 'g_xSomeStaticInit''(void)",
        )
        self.assertTrue(sym.generated)
        self.assertEqual(sym.scopes, [])
        self.assertIsNone(sym.cls)

    def test_generated_rtti_prefix_skips_parsing(self):
        sym = self._owned_sym(
            "??_R0?AVexception@std@@@8",
            "class std::exception `RTTI Type Descriptor'",
        )
        self.assertTrue(sym.generated)
        self.assertEqual(sym.scopes, [])
        self.assertIsNone(sym.cls)

    def test_generated_vftable_prefix_skips_parsing(self):
        sym = self._owned_sym(
            "??_7X@@6B@",
            "const X::`vftable'",
        )
        self.assertTrue(sym.generated)
        self.assertIsNone(sym.cls)


# ---------------------------------------------------------------------------
# build() against the fixtures/repo + fixtures/dumpbin.
# ---------------------------------------------------------------------------


class BuildFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = model.build_model(FIXTURE_REPO, "fx_debug")
        cls.table = symbols.build(cls.model, dumpbin_dir=FIXTURE_DUMPBIN)

    def test_expected_owners(self):
        t = self.table
        self.assertEqual(t.owner_of("?LibA_Init@@YAXXZ"), "LibA")
        self.assertEqual(t.owner_of("?Tick@LibA_Thing@@QEAAXXZ"), "LibA")
        self.assertEqual(t.owner_of("?DoSomething@LibB_Widget@@QEAAXXZ"), "LibB")
        self.assertEqual(t.owner_of("?LibC_ExportAll@@YAXXZ"), "LibC")
        self.assertEqual(t.owner_of("?LibC_Api_Touch@@YAXXZ"), "LibC")
        self.assertEqual(t.owner_of("?MidLib_Init@@YAXXZ"), "MidLib")

    def test_no_conflicts(self):
        self.assertEqual(self.table.conflicts, {})

    def test_two_lib_comdat_not_owned_by_anyone(self):
        # LibA_SharedInline is COMDAT-defined in BOTH LibA.txt and LibB.txt.
        self.assertIsNone(self.table.owner_of("?LibA_SharedInline@@YAXXZ"))
        self.assertIn("?LibA_SharedInline@@YAXXZ", self.table.inline["LibA"])
        self.assertIn("?LibA_SharedInline@@YAXXZ", self.table.inline["LibB"])
        self.assertNotIn("?LibA_SharedInline@@YAXXZ", self.table.owned["LibA"])
        self.assertNotIn("?LibA_SharedInline@@YAXXZ", self.table.owned["LibB"])

    def test_single_lib_comdat_not_owned(self):
        # LibA_OnlyInline is COMDAT-defined only in LibA.txt -- still must
        # be `inline`, never `owned`.
        self.assertIsNone(self.table.owner_of("?LibA_OnlyInline@@YAXXZ"))
        self.assertIn("?LibA_OnlyInline@@YAXXZ", self.table.inline["LibA"])
        self.assertNotIn("?LibA_OnlyInline@@YAXXZ", self.table.owned["LibA"])

    def test_dropped_unresolvable_undefs(self):
        # LibA.txt UNDEFs "atexit" and "__security_cookie" -- CRT noise that
        # resolves to no node -- must not survive into the merged undef set.
        self.assertNotIn("atexit", self.table.undef.get("LibA", set()))
        self.assertNotIn("__security_cookie", self.table.undef.get("LibA", set()))

    def test_cross_node_undefs_kept(self):
        self.assertIn("?Tick@LibA_Thing@@QEAAXXZ", self.table.undef["LibB"])
        self.assertIn("?LibC_Api_Touch@@YAXXZ", self.table.undef["LibB"])
        self.assertIn("?LibC_ExportAll@@YAXXZ", self.table.undef["Zenith"])

    def test_game_undefs_resolve_to_expected_owners(self):
        # Amendment 12: GameX/GameY are pinned to this exact shape.
        self.assertEqual(self.table.owner_of("?LibA_Init@@YAXXZ"), "LibA")
        self.assertEqual(self.table.owner_of("?DoSomething@LibB_Widget@@QEAAXXZ"), "LibB")
        self.assertIn("?LibA_Init@@YAXXZ", self.table.undef["GameX"])
        self.assertIn("?DoSomething@LibB_Widget@@QEAAXXZ", self.table.undef["GameX"])
        self.assertIn("?DoSomething@LibB_Widget@@QEAAXXZ", self.table.undef["GameY"])
        self.assertIn("?GameX_Main@@YAHXZ", self.table.owned["GameX"])
        self.assertIn("?GameY_Main@@YAHXZ", self.table.owned["GameY"])

    def test_scenario_27_link_only_libb_widget_poke(self):
        # Scenario 27: MidLib.cpp declares+calls LibB_Widget_Poke with no
        # include of any LibB header and no declared MidLib->LibB
        # dependency; LibB_Impl.cpp defines it out of line. dumpbin:
        # MidLib.txt UNDEFs it, LibB.txt owns it.
        self.assertEqual(self.table.owner_of("?LibB_Widget_Poke@@YAXXZ"), "LibB")
        self.assertIn("?LibB_Widget_Poke@@YAXXZ", self.table.undef["MidLib"])

    def test_scenario_26_midlib_has_no_undef_into_liba(self):
        # Scenario 26: MidLib's only reach to LibA is the PCH header itself
        # (MidLib->LibA stays pch-only) -- so MidLib must carry no UNDEF
        # resolving to any LibA-owned symbol.
        liba_owned = set(self.table.owned["LibA"])
        self.assertEqual(self.table.undef["MidLib"] & liba_owned, set())

    def test_data_symbol_and_static_member_function(self):
        sym = self.table.owned["LibA"]["?s_iInstanceCount@LibA_Thing@@2HA"]
        self.assertTrue(sym.is_data)
        self.assertEqual(sym.cls, "LibA_Thing")

        static_fn = self.table.owned["LibA"]["?StaticHelper@LibA_Thing@@SAXXZ"]
        self.assertFalse(static_fn.is_data)
        self.assertEqual(static_fn.cls, "LibA_Thing")

    def test_unreferenced_external_still_owned(self):
        # LibA_UnrefFunc: owned by LibA, referenced (UNDEF) by nobody.
        self.assertEqual(self.table.owner_of("?LibA_UnrefFunc@@YAXXZ"), "LibA")
        for node, undefs in self.table.undef.items():
            self.assertNotIn("?LibA_UnrefFunc@@YAXXZ", undefs)

    def test_stats_present_per_node(self):
        st = self.table.stats["LibA"]
        self.assertEqual(
            st["external_defs"], st["owned"] + st["comdat_lines"]
        )
        self.assertGreater(st["owned"], 0)


# ---------------------------------------------------------------------------
# Refusal tests.
# ---------------------------------------------------------------------------


class RefusalTests(unittest.TestCase):
    def test_function_level_linking_raises(self):
        m = model.build_model(FIXTURE_REPO, "fx_release")
        with self.assertRaises(symbols.SymbolsError) as ctx:
            symbols.build(m, dumpbin_dir=FIXTURE_DUMPBIN)
        msg = str(ctx.exception)
        self.assertIn("FunctionLevelLinking", msg)
        self.assertIn("fx_release", msg)

    def test_missing_dumpbin_fixture_raises_naming_path(self):
        m = model.build_model(FIXTURE_REPO, "fx_debug")
        with self.assertRaises(symbols.SymbolsError) as ctx:
            symbols.build(m, dumpbin_dir=Path("Tools/libgraph/tests/fixtures/does_not_exist"))
        msg = str(ctx.exception)
        self.assertIn("LibA.txt", msg)

    def test_missing_lib_no_dumpbin_dir_raises_naming_path(self):
        m = model.build_model(FIXTURE_REPO, "fx_debug")
        with self.assertRaises(symbols.SymbolsError) as ctx:
            symbols.build(m, dumpbin_dir=None)
        msg = str(ctx.exception)
        self.assertIn("liba.lib", msg.lower())
        self.assertIn(str(Path(m.repo_root)), msg)


# ---------------------------------------------------------------------------
# vswhere version selection (amendment 2).
# ---------------------------------------------------------------------------


class VswhereVersionSortTests(unittest.TestCase):
    def test_highest_toolset_version_wins(self):
        candidates = [
            r"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe",
            r"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe",
        ]
        candidates.sort(key=symbols._version_sort_key)
        self.assertTrue(candidates[-1].endswith("14.51.36231\\bin\\Hostx64\\x64\\dumpbin.exe"))


if __name__ == "__main__":
    unittest.main()
