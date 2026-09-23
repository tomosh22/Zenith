"""Unit tests for Tools/libgraph/{model,vcxproj,sharpmake}.py.

Run with:
    python -m unittest discover Tools/libgraph/tests -v
    # or, from this directory:
    python -m unittest test_model

Pure stdlib (unittest). Mirrors Tools/tests/test_complexity.py's sys.path
setup so the flat sibling modules (model/vcxproj/sharpmake, no package,
no __init__.py) import cleanly regardless of CWD.
"""

import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

# Make Tools/libgraph importable regardless of CWD (mirrors
# Tools/tests/test_complexity.py:24-26).
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))

import model  # noqa: E402
import sharpmake  # noqa: E402
import vcxproj  # noqa: E402

FIXTURE_REPO = _HERE / "fixtures" / "repo"


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(content), encoding="utf-8")


class FixtureModelTests(unittest.TestCase):
    """Tests that run build_model() against the shared fixtures/repo tree."""

    @classmethod
    def setUpClass(cls):
        cls.model = model.build_model(FIXTURE_REPO, "fx_debug")

    def test_node_counts_per_kind(self):
        counts = {}
        for n in self.model.nodes.values():
            counts[n.kind] = counts.get(n.kind, 0) + 1
        self.assertEqual(counts.get("lib"), 4)  # LibA, LibB, LibC, Zenith
        self.assertEqual(counts.get("game"), 2)  # GameX, GameY
        self.assertEqual(counts.get("proof"), 1)  # SentinelLibB
        # MidLib (Entry-table) + glmx (synthesized third-party).
        self.assertEqual(counts.get("middleware"), 2)

    def test_node_fields_liba(self):
        n = self.model.nodes["LibA"]
        self.assertEqual(n.kind, "lib")
        self.assertEqual(n.vcxproj, "Build/liba_win64.vcxproj")
        self.assertEqual(n.out_lib, "Build/output/fx_debug/liba.lib")
        self.assertEqual(n.int_dir, "Build/obj/liba/fx_debug")
        self.assertEqual(n.include_dirs, ["Zenith", "Zenith/LibA", "Middleware"])
        self.assertEqual(n.pch_header, "FxPch.h")
        self.assertFalse(n.function_level_linking)
        self.assertEqual(n.source_roots, ["Zenith/LibA"])
        self.assertIsNone(n.subject)

    def test_node_fields_release_config_function_level_linking(self):
        m = model.build_model(FIXTURE_REPO, "fx_release")
        self.assertTrue(m.nodes["LibA"].function_level_linking)
        self.assertTrue(m.nodes["LibB"].function_level_linking)

    def test_config_match_is_case_insensitive(self):
        m_upper = model.build_model(FIXTURE_REPO, "FX_DEBUG")
        self.assertEqual(m_upper.nodes["LibA"].out_lib, "Build/output/fx_debug/liba.lib")
        self.assertEqual(m_upper.config, "fx_debug")

    def test_game_int_dir_and_no_out_lib(self):
        n = self.model.nodes["GameX"]
        self.assertEqual(n.kind, "game")
        self.assertIsNone(n.out_lib)
        self.assertEqual(n.int_dir, "Games/GameX/build/obj/fx_debug")
        self.assertEqual(
            n.include_dirs,
            ["Zenith", "Zenith/LibA", "Middleware", "Games/GameX/Source"],
        )
        # Scenario 17: games use PrecompiledHeader NotUsing.
        self.assertIsNone(n.pch_header)

    def test_middleware_no_pch_no_deps_node(self):
        n = self.model.nodes["MidLib"]
        self.assertEqual(n.kind, "middleware")
        self.assertIsNone(n.pch_header)
        self.assertEqual(n.source_roots, ["Middleware/MidLib"])

    def test_pch_root(self):
        self.assertEqual(self.model.pch_root, "Zenith/LibA/FxPch.h")

    def test_json_dump_carries_via_aggregate(self):
        doc = model._model_to_dict(self.model)
        edges = doc["declared"]
        self.assertTrue(edges)
        for e in edges:
            self.assertIn("via_aggregate", e)
        aggregate_edges = [e for e in edges if e["src"] == "Zenith"]
        self.assertTrue(aggregate_edges)
        self.assertTrue(all(e["via_aggregate"] is True for e in aggregate_edges))
        literal_edges = [e for e in edges if e["src"] == "LibB" and e["dst"] == "LibA"]
        self.assertEqual(len(literal_edges), 1)
        self.assertIs(literal_edges[0]["via_aggregate"], False)

    # -- Ownership -----------------------------------------------------

    def test_split_source_root_ownership(self):
        files = self.model.files
        for p in (
            "Zenith/Shared/LibB_Api.h",
            "Zenith/Shared/LibB_Api.Tests.inl",  # scenario 24
            "Zenith/Shared/detail/LibB_Inner.h",
            "Zenith/Shared/LibB_Sibling.h",
            "Zenith/Shared/LibB_Impl.cpp",
        ):
            self.assertEqual(files[p].node, "LibB", p)
            self.assertEqual(files[p].how, "listed")
        for p in ("Zenith/Shared/LibC_Api.h", "Zenith/Shared/LibC_Impl.cpp"):
            self.assertEqual(files[p].node, "LibC", p)

    def test_orphan(self):
        # Scenario 19: unlisted by any vcxproj, so it's always in orphans --
        # but it still lives inside LibA's own source root ("Zenith/LibA"),
        # so the dir-fallback prefix match finds LibA as its owner too (the
        # brief's rule: an orphan with no matching source root gets no
        # FileOwner entry at all; one whose path is under a real source root
        # gets a dir-fallback entry AND still counts as an orphan).
        self.assertIn("Zenith/LibA/LibA_Orphan.h", self.model.orphans)
        fo = self.model.files["Zenith/LibA/LibA_Orphan.h"]
        self.assertEqual(fo.node, "LibA")
        self.assertEqual(fo.how, "dir-fallback")

    def test_orphan_with_no_source_root_match_has_no_file_entry(self):
        # An orphan outside every node's source_roots gets no FileOwner at
        # all ("If there is none, it stays in orphans with no owner entry").
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Common.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                """,
            )
            _write(
                td_path / "Build" / "Sharpmake_LibA.cs",
                """\
                using Sharpmake;
                [Sharpmake.Generate]
                public class LibALibProject : ZenithBaseProject
                {
                    public LibALibProject() { Name = "LibA"; SourceRootPath = RootPath + "/Zenith/LibA"; }
                }
                """,
            )
            _write(
                td_path / "Zenith" / "Stray" / "Stray.h",
                "#pragma once\n",
            )
            m = model.build_model(td_path, "fx_debug")
            self.assertIn("Zenith/Stray/Stray.h", m.orphans)
            self.assertNotIn("Zenith/Stray/Stray.h", m.files)

    def test_duplicate_and_middleware_routing(self):
        path = "Middleware/MidLib/backends/mid_impl.h"
        self.assertIn(path, self.model.duplicates)
        fo = self.model.files[path]
        self.assertEqual(fo.node, "MidLib")
        self.assertEqual(sorted(fo.duplicate_of), ["LibB", "LibC"])

    def test_scenario_24_liba_only_reaches_libb_via_tests_inl(self):
        # LibA.cpp's only include of any LibB file is the .Tests.inl -- a
        # content-level guard so a future edit can't silently reintroduce a
        # non-test LibA->LibB include (which would collapse this scenario).
        liba_cpp = (FIXTURE_REPO / "Zenith" / "LibA" / "LibA.cpp").read_text()
        self.assertIn('#include "Shared/LibB_Api.Tests.inl"', liba_cpp)
        self.assertNotIn('#include "Shared/LibB_Api.h"', liba_cpp)
        # And the .Tests.inl's own link surface must stay header-only: no
        # out-of-line LibB call, so LibA.txt (W2b) carries no UNDEF into
        # libb.lib.
        inl = (FIXTURE_REPO / "Zenith" / "Shared" / "LibB_Api.Tests.inl").read_text()
        self.assertIn("LibB_Widget", inl)

    def test_scenario_25_direct_and_pch_reach_both_own_midlib_h(self):
        libb_impl = (FIXTURE_REPO / "Zenith" / "Shared" / "LibB_Impl.cpp").read_text()
        self.assertIn('#include "MidLib/MidLib.h"', libb_impl)
        edges = [e for e in self.model.declared if e.src == "LibB" and e.dst == "MidLib"]
        self.assertEqual(len(edges), 1)
        self.assertIsNone(edges[0].conditional)

    def test_scenario_26_midlib_only_reaches_liba_via_pch(self):
        midlib_cpp = (FIXTURE_REPO / "Middleware" / "MidLib" / "MidLib.cpp").read_text()
        self.assertIn('#include "FxPch.h"', midlib_cpp)
        # MidLib's ONLY include -- no direct LibA header, no "MidLib.h"
        # self-include either (the orchestrator's exact fixture shape).
        includes = [ln for ln in midlib_cpp.splitlines() if ln.strip().startswith("#include")]
        self.assertEqual(includes, ['#include "FxPch.h"'])
        # MidLib declares no dependency on LibA.
        edges = [e for e in self.model.declared if e.src == "MidLib" and e.dst == "LibA"]
        self.assertEqual(edges, [])

    def test_scenario_27_midlib_link_only_to_libb(self):
        midlib_cpp = (FIXTURE_REPO / "Middleware" / "MidLib" / "MidLib.cpp").read_text()
        self.assertIn("extern void LibB_Widget_Poke();", midlib_cpp)
        self.assertIn("LibB_Widget_Poke();", midlib_cpp)
        # No #include anywhere in MidLib.cpp names a LibB file -- the whole
        # point of scenario 27 is a link-only reach with zero include edge.
        includes = [ln.strip() for ln in midlib_cpp.splitlines() if ln.strip().startswith("#include")]
        self.assertTrue(all("LibB" not in ln and "Shared" not in ln for ln in includes), includes)
        libb_impl = (FIXTURE_REPO / "Zenith" / "Shared" / "LibB_Impl.cpp").read_text()
        self.assertIn("void LibB_Widget_Poke()\n{\n}", libb_impl)
        # MidLib declares no dependency on LibB either.
        edges = [e for e in self.model.declared if e.src == "MidLib" and e.dst == "LibB"]
        self.assertEqual(edges, [])

    def test_synthesized_third_party_node(self):
        self.assertIn("glmx", self.model.nodes)
        n = self.model.nodes["glmx"]
        self.assertEqual(n.kind, "middleware")
        self.assertIsNone(n.vcxproj)
        self.assertEqual(n.source_roots, ["Middleware/glmx"])
        fo = self.model.files["Middleware/glmx/glmx.hpp"]
        self.assertEqual(fo.node, "glmx")
        self.assertEqual(fo.how, "dir-fallback")

    def test_ambiguous_fxconfig_both_listed_by_liba(self):
        # Both candidates are owned by LibA (ownership doesn't care which
        # one an outside #include "FxConfig.h" resolves to -- that's
        # includes.py's job).
        self.assertEqual(self.model.files["Zenith/FxConfig.h"].node, "LibA")
        self.assertEqual(self.model.files["Zenith/LibA/FxConfig.h"].node, "LibA")

    # -- Declared edges --------------------------------------------------

    def test_declared_edge_order(self):
        edges = sorted(
            (e for e in self.model.declared if e.src == "LibC"),
            key=lambda e: e.order,
        )
        self.assertEqual([e.dst for e in edges], ["MidLib", "LibB"])
        self.assertEqual([e.order for e in edges], [0, 1])

    def test_aggregate_expansion(self):
        edges = [e for e in self.model.declared if e.src == "Zenith"]
        dsts = sorted(e.dst for e in edges)
        self.assertEqual(dsts, ["LibA", "LibB", "LibC", "MidLib"])
        for e in edges:
            self.assertIsNone(e.conditional)
            # Every aggregate-expansion edge carries via_aggregate=True.
            self.assertTrue(e.via_aggregate, e)

    def test_game_inherited_edge(self):
        for game in ("GameX", "GameY"):
            edges = [e for e in self.model.declared if e.src == game]
            self.assertEqual(len(edges), 1)
            self.assertEqual(edges[0].dst, "Zenith")
            self.assertEqual(edges[0].order, 0)
            self.assertIsNone(edges[0].conditional)
            # A literal inherited AddPublicDependency<ZenithProject> call,
            # not an aggregate-expansion edge -- via_aggregate=False.
            self.assertFalse(edges[0].via_aggregate)

    def test_comment_stripping_no_phantom_edge(self):
        # Sharpmake_LibB.cs has a commented-out
        # "// conf.AddPublicDependency<LibCLibProject>(target);" line -- LibC
        # must NOT appear. LibB declares LibA and (scenario 25) MidLib.
        edges = [e for e in self.model.declared if e.src == "LibB"]
        self.assertEqual([e.dst for e in edges], ["LibA", "MidLib"])
        # Both are literal AddPublicDependency calls -- via_aggregate=False.
        for e in edges:
            self.assertFalse(e.via_aggregate, e)

    def test_sentinel_subject(self):
        self.assertEqual(self.model.nodes["SentinelLibB"].subject, "LibB")

    def test_renderbackend_unresolved_for_fx_debug(self):
        conditional_edges = [
            e for e in self.model.declared if e.src == "SentinelLibB" and e.conditional
        ]
        self.assertEqual(len(conditional_edges), 3)
        for e in conditional_edges:
            self.assertIn(e, self.model.declared)
        unresolved_lines = self.model.unresolved_conditions
        self.assertEqual(len(unresolved_lines), 3)
        for line in unresolved_lines:
            self.assertTrue(line.startswith("Build/Sharpmake_SentinelLibB.cs:"))

    def test_absent_nodes_empty_for_fx_debug(self):
        self.assertEqual(self.model.absent_nodes, [])

    # -- Allowlist ---------------------------------------------------------

    def test_allowlist_all_formats(self):
        al = self.model.allowlist
        self.assertEqual(al.get("Zenith/Shared/LibB_Impl.cpp => Shared/LibC_Api.h"), None)
        self.assertEqual(
            al.get("Zenith/EngineComposition/Composition.cpp => LibA/Internal/LibA_Detail.h"),
            "encapsulation",
        )
        # module-fanout / unmapped lines (no " => ") must not appear as keys.
        self.assertNotIn("module-fanout", al)
        for key in al:
            self.assertNotIn("module-fanout", key)
        # The symbol-keyed file must be ignored entirely.
        self.assertNotIn("Zenith_SomeSingleton::Get", al)
        self.assertEqual(len(al), 2)


class RenderBackendResolutionTests(unittest.TestCase):
    """Inline-config tests for the RenderBackend conditional resolver."""

    def test_vulkan_config_resolves(self):
        m = model.build_model(FIXTURE_REPO, "vulkan_x")
        edges = [e for e in m.declared if e.src == "SentinelLibB" and e.dst == "LibC"]
        self.assertEqual(len(edges), 1)
        self.assertIsNone(edges[0].conditional)
        # The D3D12/Null branches must not fire.
        self.assertEqual(
            sorted(e.dst for e in m.declared if e.src == "SentinelLibB"),
            ["LibA", "LibB", "LibC", "MidLib"],
        )
        self.assertEqual(m.unresolved_conditions, [])

    def test_null_config_resolves(self):
        m = model.build_model(FIXTURE_REPO, "null_x")
        edges = [
            e
            for e in m.declared
            if e.src == "SentinelLibB" and e.conditional is None
        ]
        dsts = sorted(e.dst for e in edges)
        # LibB appears twice: the unconditional declaration and the
        # Null-resolved conditional one both resolve to LibB.
        self.assertEqual(dsts, ["LibA", "LibB", "LibB", "MidLib"])
        self.assertEqual(m.unresolved_conditions, [])

    def test_d3d12_config_resolves(self):
        m = model.build_model(FIXTURE_REPO, "d3d12_x")
        edges = [
            e
            for e in m.declared
            if e.src == "SentinelLibB" and e.conditional is None
        ]
        dsts = sorted(e.dst for e in edges)
        self.assertEqual(dsts, ["LibA", "LibA", "LibB", "MidLib"])

    def test_unrecognised_prefix_stays_unresolved(self):
        m = model.build_model(FIXTURE_REPO, "fx_debug")
        conditional_edges = [
            e for e in m.declared if e.src == "SentinelLibB" and e.conditional
        ]
        self.assertEqual(len(conditional_edges), 3)
        self.assertEqual(len(m.unresolved_conditions), 3)


class VcxprojParsingTests(unittest.TestCase):
    """Direct tests of vcxproj.py against the fixture's own .vcxproj files."""

    def test_discover_raises_on_nonexistent_repo_root(self):
        with tempfile.TemporaryDirectory() as td:
            missing = Path(td) / "does-not-exist"
            with self.assertRaises(ValueError) as ctx:
                vcxproj.discover_vcxproj_files(missing)
            self.assertIn("repo root not found", str(ctx.exception))
            self.assertIn(str(missing.resolve()), str(ctx.exception))
            with self.assertRaises(ValueError):
                vcxproj.discover_agde_vcxproj_files(missing)

    def test_discover_finds_fixture_vcxprojs(self):
        found = vcxproj.discover_vcxproj_files(FIXTURE_REPO)
        names = sorted(p.name for p in found)
        self.assertIn("liba_win64.vcxproj", names)
        self.assertIn("gamex_win64.vcxproj", names)
        self.assertIn("gamey_win64.vcxproj", names)
        self.assertIn("sentinellibb_win64.vcxproj", names)

    def test_discover_ignores_agde(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "foo_agde.vcxproj",
                "<Project></Project>",
            )
            _write(
                td_path / "Build" / "foo_win64.vcxproj",
                "<Project></Project>",
            )
            found = vcxproj.discover_vcxproj_files(td_path)
            names = sorted(p.name for p in found)
            self.assertEqual(names, ["foo_win64.vcxproj"])

    def test_discover_skips_noise_dirs(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(td_path / ".git" / "x_win64.vcxproj", "<Project></Project>")
            _write(td_path / "node_modules" / "x_win64.vcxproj", "<Project></Project>")
            _write(td_path / "Build" / "output" / "x_win64.vcxproj", "<Project></Project>")
            _write(td_path / "Build" / "artifacts" / "x_win64.vcxproj", "<Project></Project>")
            _write(td_path / "Middleware" / "vendor" / "x_win64.vcxproj", "<Project></Project>")
            found = vcxproj.discover_vcxproj_files(td_path)
            self.assertEqual(found, [])

    def test_per_config_fields_case_insensitive_and_fll(self):
        path = FIXTURE_REPO / "Build" / "liba_win64.vcxproj"
        parsed_debug = vcxproj.parse_vcxproj(path, "FX_DEBUG", FIXTURE_REPO)
        self.assertTrue(parsed_debug.has_config)
        self.assertEqual(parsed_debug.project_name, "LibA")
        self.assertEqual(parsed_debug.target_name, "liba")
        self.assertFalse(parsed_debug.function_level_linking)
        self.assertEqual(parsed_debug.pch_header, "FxPch.h")
        self.assertEqual(
            parsed_debug.include_dirs,
            ["Zenith", "Zenith/LibA", "Middleware"],
        )

        parsed_release = vcxproj.parse_vcxproj(path, "fx_release", FIXTURE_REPO)
        self.assertTrue(parsed_release.function_level_linking)

    def test_missing_config_reports_has_config_false(self):
        path = FIXTURE_REPO / "Build" / "liba_win64.vcxproj"
        parsed = vcxproj.parse_vcxproj(path, "vulkan_vs2022_debug_win64_true", FIXTURE_REPO)
        self.assertFalse(parsed.has_config)
        self.assertEqual(parsed.project_name, "LibA")  # <ProjectName> isn't config-scoped
        # File listing is global too -- still populated.
        self.assertTrue(any(f[0].endswith("LibA.cpp") for f in parsed.files))

    def test_excluded_from_build_respected(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            vcx = td_path / "Build" / "x_win64.vcxproj"
            _write(
                vcx,
                """\
                <?xml version="1.0" encoding="utf-8"?>
                <Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
                  <PropertyGroup Label="Globals"><ProjectName>X</ProjectName></PropertyGroup>
                  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'" Label="Configuration" />
                  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                    <TargetName>x</TargetName>
                    <OutDir>output\\</OutDir>
                    <IntDir>obj\\</IntDir>
                  </PropertyGroup>
                  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                    <ClCompile>
                      <FunctionLevelLinking>false</FunctionLevelLinking>
                    </ClCompile>
                  </ItemDefinitionGroup>
                  <ItemGroup>
                    <ClCompile Include="a.cpp" />
                    <ClCompile Include="b.cpp">
                      <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">true</ExcludedFromBuild>
                    </ClCompile>
                  </ItemGroup>
                </Project>
                """,
            )
            parsed = vcxproj.parse_vcxproj(vcx, "fx_debug", td_path)
            names = sorted(f[0] for f in parsed.files)
            self.assertEqual(names, ["Build/a.cpp"])


class SharpmakeCommentStrippingTests(unittest.TestCase):
    def test_line_comment_stripped(self):
        text = 'Name = "X"; // AddPublicDependency<YProject>(target);\n'
        stripped = sharpmake.strip_comments(text)
        self.assertNotIn("YProject", stripped)
        self.assertIn('Name = "X";', stripped)

    def test_block_comment_stripped(self):
        text = "before /* AddPublicDependency<YProject>(target); */ after\n"
        stripped = sharpmake.strip_comments(text)
        self.assertNotIn("YProject", stripped)
        self.assertIn("before", stripped)
        self.assertIn("after", stripped)

    def test_verbatim_string_not_treated_as_comment(self):
        text = r'SourceFilesExcludeRegex.Add(@"Shared\\LibC_Api\.h$");' + "\n"
        stripped = sharpmake.strip_comments(text)
        self.assertIn("LibC_Api", stripped)

    def test_line_numbers_preserved(self):
        text = "line1\nline2\n// comment on line3\nline4\n"
        stripped = sharpmake.strip_comments(text)
        self.assertEqual(stripped.count("\n"), text.count("\n"))


class HardErrorTests(unittest.TestCase):
    """Poison cases deliberately NOT in the shared fixture tree."""

    def test_unknown_class_in_add_public_dependency_raises(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Bad.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                [Sharpmake.Generate]
                public class BadLibProject : ZenithBaseProject
                {
                    public BadLibProject() { Name = "Bad"; SourceRootPath = RootPath + "/Zenith/Bad"; }
                    [Configure]
                    public void ConfigureAll(Configuration conf, ZenithTarget target)
                    {
                        conf.AddPublicDependency<UnknownXProject>(target);
                    }
                }
                """,
            )
            with self.assertRaises(ValueError) as ctx:
                model.build_model(td_path, "fx_debug")
            msg = str(ctx.exception)
            self.assertIn("UnknownXProject", msg)
            self.assertIn("Sharpmake_Bad.cs", msg)

    def test_vcxproj_project_name_with_no_class_raises(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Good.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                [Sharpmake.Generate]
                public class GoodLibProject : ZenithBaseProject
                {
                    public GoodLibProject() { Name = "Good"; SourceRootPath = RootPath + "/Zenith/Good"; }
                    [Configure]
                    public void ConfigureAll(Configuration conf, ZenithTarget target) {}
                }
                """,
            )
            _write(
                td_path / "Build" / "mystery_win64.vcxproj",
                """\
                <?xml version="1.0" encoding="utf-8"?>
                <Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
                  <PropertyGroup Label="Globals"><ProjectName>Mystery</ProjectName></PropertyGroup>
                </Project>
                """,
            )
            with self.assertRaises(ValueError) as ctx:
                model.build_model(td_path, "fx_debug")
            msg = str(ctx.exception)
            self.assertIn("Mystery", msg)
            self.assertIn("regen.ps1", msg)


class MissingVcxprojTests(unittest.TestCase):
    """A scanned Sharpmake class with no matching vcxproj at all (neither
    win64 nor agde) -- e.g. a new .cs added without running regen.ps1 --
    must be reported, not silently degraded to an empty node."""

    def test_reported_in_absent_nodes_and_missing_vcxproj_not_raised(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Common.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                """,
            )
            _write(
                td_path / "Build" / "Sharpmake_Ghost.cs",
                """\
                using Sharpmake;
                [Sharpmake.Generate]
                public class GhostLibProject : ZenithBaseProject
                {
                    public GhostLibProject() { Name = "Ghost"; SourceRootPath = RootPath + "/Zenith/Ghost"; }
                }
                """,
            )
            # No ghost_win64.vcxproj / ghost_agde.vcxproj anywhere -- this is
            # the "new .cs, forgot to regen" shape. Must not raise.
            m = model.build_model(td_path, "fx_debug")

            self.assertIn("Ghost", m.nodes)
            node = m.nodes["Ghost"]
            self.assertIsNone(node.vcxproj)
            self.assertIsNone(node.out_lib)
            self.assertEqual(node.include_dirs, [])

            self.assertIn("Ghost", m.absent_nodes)

            self.assertEqual(len(m.missing_vcxproj), 1)
            entry = m.missing_vcxproj[0]
            self.assertIn("GhostLibProject", entry)  # the class name
            self.assertIn('Name="Ghost"', entry)  # the Sharpmake Name
            self.assertIn("Sharpmake_Ghost.cs", entry)  # the .cs path
            self.assertIn("regen.ps1", entry)

    def test_synthesized_middleware_node_exempt(self):
        # A synthesized third-party node (no Sharpmake class at all) must
        # never appear in missing_vcxproj -- it was never scanned as a class
        # in the first place.
        m = model.build_model(FIXTURE_REPO, "fx_debug")
        self.assertIn("glmx", m.nodes)  # scenario 21's synthesized node
        for entry in m.missing_vcxproj:
            self.assertNotIn("glmx", entry)
        # The shared fixture itself has no missing-vcxproj classes at all.
        self.assertEqual(m.missing_vcxproj, [])


_AGDE_VCXPROJ_TEMPLATE = """\
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals"><ProjectName>{name}</ProjectName></PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Vulkan_arm64_v8a_vs2022_Debug_Agde_False|Android-arm64-v8a'" Label="Configuration" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Vulkan_arm64_v8a_vs2022_Debug_Agde_False|Android-arm64-v8a'">
    <TargetName>{target}</TargetName>
    <OutDir>output\\agde\\</OutDir>
    <IntDir>obj\\agde\\</IntDir>
  </PropertyGroup>
  <ItemGroup>
{items}
  </ItemGroup>
</Project>
"""


class AndroidAgdeOwnershipTests(unittest.TestCase):
    """Fix 2: *_agde.vcxproj is parsed for OWNERSHIP only, never per-config
    data, so an agde-only class's files don't show up as false orphans."""

    def _build(self, extra_files: dict[str, str] | None = None):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        td_path = Path(td.name)
        _write(
            td_path / "Build" / "Sharpmake_Common.cs",
            """\
            using Sharpmake;
            public abstract class ZenithBaseProject : Project {}
            """,
        )
        _write(
            td_path / "Build" / "Sharpmake_Android.cs",
            """\
            using Sharpmake;
            [Sharpmake.Generate]
            public class FxAndroidLibProject : ZenithBaseProject
            {
                public FxAndroidLibProject() { Name = "FxAndroid"; SourceRootPath = RootPath + "/Zenith/FxAndroid"; }
            }
            """,
        )
        _write(
            td_path / "Build" / "fxandroid_agde.vcxproj",
            _AGDE_VCXPROJ_TEMPLATE.format(
                name="FxAndroid",
                target="fxandroid",
                items='    <ClInclude Include="..\\Zenith\\FxAndroid\\FxAndroid_Window.h" />',
            ),
        )
        _write(td_path / "Zenith" / "FxAndroid" / "FxAndroid_Window.h", "#pragma once\n")
        if extra_files:
            for rel, content in extra_files.items():
                _write(td_path / rel, content)
        return td_path

    def test_agde_only_class_is_absent_not_orphan(self):
        td_path = self._build()
        m = model.build_model(td_path, "fx_debug")
        self.assertIn("FxAndroid", m.absent_nodes)
        self.assertNotIn("Zenith/FxAndroid/FxAndroid_Window.h", m.orphans)
        fo = m.files["Zenith/FxAndroid/FxAndroid_Window.h"]
        self.assertEqual(fo.node, "FxAndroid")
        self.assertEqual(fo.how, "listed")

    def test_agde_vcxproj_never_supplies_config_fields(self):
        td_path = self._build()
        m = model.build_model(td_path, "fx_debug")
        n = m.nodes["FxAndroid"]
        self.assertIsNone(n.out_lib)
        self.assertIsNone(n.int_dir)
        self.assertEqual(n.include_dirs, [])
        self.assertIsNone(n.pch_header)
        self.assertIsNone(n.function_level_linking)

    def test_same_node_win64_and_agde_not_a_duplicate(self):
        # FxAndroid also gets a win64 vcxproj listing the SAME header (e.g. a
        # cross-platform lib) -- must not be reported as a duplicate.
        td_path = self._build()
        _write(
            td_path / "Build" / "fxandroid_win64.vcxproj",
            """\
            <?xml version="1.0" encoding="utf-8"?>
            <Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
              <PropertyGroup Label="Globals"><ProjectName>FxAndroid</ProjectName></PropertyGroup>
              <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'" Label="Configuration" />
              <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                <TargetName>fxandroid</TargetName>
                <OutDir>output\\fx_debug\\</OutDir>
                <IntDir>obj\\fx_debug\\</IntDir>
              </PropertyGroup>
              <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                <ClCompile><FunctionLevelLinking>false</FunctionLevelLinking></ClCompile>
              </ItemDefinitionGroup>
              <ItemGroup>
                <ClInclude Include="..\\Zenith\\FxAndroid\\FxAndroid_Window.h" />
              </ItemGroup>
            </Project>
            """,
        )
        m = model.build_model(td_path, "fx_debug")
        self.assertNotIn("Zenith/FxAndroid/FxAndroid_Window.h", m.duplicates)
        fo = m.files["Zenith/FxAndroid/FxAndroid_Window.h"]
        self.assertEqual(fo.node, "FxAndroid")
        self.assertEqual(fo.duplicate_of, [])
        # And now that a win64 vcxproj exists with this config, it's present.
        self.assertNotIn("FxAndroid", m.absent_nodes)

    def test_different_nodes_win64_and_agde_is_a_duplicate(self):
        # A second, DIFFERENT class's win64 vcxproj lists the same header
        # FxAndroid's agde vcxproj owns -- that IS a genuine duplicate.
        td_path = self._build()
        _write(
            td_path / "Build" / "Sharpmake_Other.cs",
            """\
            using Sharpmake;
            [Sharpmake.Generate]
            public class FxOtherLibProject : ZenithBaseProject
            {
                public FxOtherLibProject() { Name = "FxOther"; SourceRootPath = RootPath + "/Zenith/FxOther"; }
            }
            """,
        )
        _write(
            td_path / "Build" / "fxother_win64.vcxproj",
            """\
            <?xml version="1.0" encoding="utf-8"?>
            <Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
              <PropertyGroup Label="Globals"><ProjectName>FxOther</ProjectName></PropertyGroup>
              <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'" Label="Configuration" />
              <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                <TargetName>fxother</TargetName>
                <OutDir>output\\fx_debug\\</OutDir>
                <IntDir>obj\\fx_debug\\</IntDir>
              </PropertyGroup>
              <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='fx_debug|x64'">
                <ClCompile><FunctionLevelLinking>false</FunctionLevelLinking></ClCompile>
              </ItemDefinitionGroup>
              <ItemGroup>
                <ClInclude Include="..\\Zenith\\FxAndroid\\FxAndroid_Window.h" />
              </ItemGroup>
            </Project>
            """,
        )
        m = model.build_model(td_path, "fx_debug")
        self.assertIn("Zenith/FxAndroid/FxAndroid_Window.h", m.duplicates)
        fo = m.files["Zenith/FxAndroid/FxAndroid_Window.h"]
        self.assertEqual(sorted([fo.node] + fo.duplicate_of), ["FxAndroid", "FxOther"])

    def test_agde_vcxproj_project_name_with_no_class_raises(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Common.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                """,
            )
            _write(
                td_path / "Build" / "mystery_agde.vcxproj",
                """\
                <?xml version="1.0" encoding="utf-8"?>
                <Project DefaultTargets="Build" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
                  <PropertyGroup Label="Globals"><ProjectName>Mystery</ProjectName></PropertyGroup>
                </Project>
                """,
            )
            with self.assertRaises(ValueError) as ctx:
                model.build_model(td_path, "fx_debug")
            self.assertIn("Mystery", str(ctx.exception))


class SharpmakeClassificationTests(unittest.TestCase):
    """Direct tests of sharpmake.scan()/classify() against small inline trees."""

    def test_abstract_and_solution_classes_ignored(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Mini.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                public abstract class GameProject : ZenithBaseProject {}
                public abstract class GameSolution : Solution {}
                public class ZenithEngineSolution : Solution {}
                [Sharpmake.Generate]
                public class XGameSolution : GameSolution {}
                """,
            )
            scan_result = sharpmake.scan(td_path)
            sharpmake.classify(scan_result)
            self.assertIsNone(scan_result.classes["ZenithBaseProject"].kind)
            self.assertIsNone(scan_result.classes["GameProject"].kind)
            self.assertIsNone(scan_result.classes["GameSolution"].kind)
            self.assertIsNone(scan_result.classes["ZenithEngineSolution"].kind)
            self.assertIsNone(scan_result.classes["XGameSolution"].kind)

    def test_direct_project_subclass_is_middleware(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Vendor.cs",
                """\
                using Sharpmake;
                [Sharpmake.Generate]
                public class VendorLibProject : Project
                {
                    public VendorLibProject() { Name = "Vendor"; }
                }
                """,
            )
            scan_result = sharpmake.scan(td_path)
            sharpmake.classify(scan_result)
            self.assertEqual(scan_result.classes["VendorLibProject"].kind, "middleware")

    def test_entry_table_class_with_tools_root_is_lib(self):
        # Mirrors the real repo's ZenithToolsProject: Entry-table membership
        # decides lib vs middleware ONLY by "is the source root under
        # Middleware/" -- a source root outside both Zenith/ and Middleware/
        # (e.g. the repo-root "Tools" dir) is still "lib", not "proof". The
        # plan's verification expects EngineComposition(Zenith) -> ZenithTools
        # to be an analysed lib->lib edge (link-only), which requires
        # ZenithTools to carry an out_lib like any other analysed lib.
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            _write(
                td_path / "Build" / "Sharpmake_Common.cs",
                """\
                using Sharpmake;
                public abstract class ZenithBaseProject : Project {}
                """,
            )
            _write(
                td_path / "Build" / "Sharpmake_FxTools.cs",
                """\
                using Sharpmake;
                [Sharpmake.Generate]
                public class FxToolsProject : ZenithBaseProject
                {
                    public FxToolsProject() { Name = "FxTools"; SourceRootPath = RootPath + "/Tools"; }
                }
                """,
            )
            _write(
                td_path / "Build" / "Sharpmake_EngineLibs.cs",
                """\
                using Sharpmake;
                public static class ZenithEngineLibs
                {
                    public struct Entry
                    {
                        public System.Type ProjectType;
                        public Entry(System.Type t) { ProjectType = t; }
                    }
                    public static readonly Entry[] All = new Entry[]
                    {
                        new Entry(typeof(FxToolsProject)),
                    };
                }
                """,
            )
            scan_result = sharpmake.scan(td_path)
            sharpmake.classify(scan_result)
            self.assertEqual(scan_result.classes["FxToolsProject"].kind, "lib")


if __name__ == "__main__":
    unittest.main()
