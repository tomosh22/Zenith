"""
Fixture tests for analyze_code_complexity.py.

Run with:
    python -m unittest Tools.tests.test_complexity
    # or from the Tools/ dir:
    python -m unittest tests.test_complexity

Tests are pure stdlib (unittest) so they run anywhere Python runs. They pair
inline C++ snippets with expected metric values, locking current behavior and
catching regressions when the parser changes.
"""

import hashlib
import json
import os
import sys
import tempfile
import textwrap
import unittest
import unittest.mock as mock
from pathlib import Path

# Make the parent directory importable regardless of CWD.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))

import analyze_code_complexity as ac  # noqa: E402


def _analyze_source(code: str) -> ac.CppAnalyzer:
    """Write `code` to a temp .cpp file and run full analysis on its directory."""
    tmpdir = tempfile.mkdtemp(prefix='complex_test_')
    try:
        path = Path(tmpdir) / 'test.cpp'
        path.write_text(code, encoding='utf-8')
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        # Force serial for deterministic output.
        analyzer.analyze(workers=1)
        return analyzer
    finally:
        # Callers inspect the analyzer; cleanup is best-effort.
        pass


class TestCyclomaticComplexity(unittest.TestCase):
    def test_baseline_is_one(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        self.assertEqual(analyzer.calculate_cyclomatic_complexity('int main() { return 0; }'), 1)

    def test_single_if_adds_one(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        code = 'void f(int x) { if (x > 0) return; }'
        self.assertEqual(analyzer.calculate_cyclomatic_complexity(code), 2)

    def test_multiple_branches(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        # 1 (base) + if + for + while + && + || = 6
        code = '''
            void f(int x) {
                if (x > 0 && x < 10) {
                    for (int i = 0; i < x; ++i) {
                        while (i-- || x > 5) { }
                    }
                }
            }
        '''
        self.assertEqual(analyzer.calculate_cyclomatic_complexity(code), 6)


class TestCognitiveComplexity(unittest.TestCase):
    def test_nested_adds_depth(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        # Flat if: +1 each; nested if inside it: +2 (1 + nesting of 1).
        # Nested case must be multi-line: the line-based algorithm computes cost
        # per keyword using the line-start nesting, so single-line nested control
        # flow would undercount. Real-world C++ uses multi-line, so this is fine.
        flat = 'void f(int x) { if (x) {} if (x) {} }'
        nested = textwrap.dedent('''
            void f(int x) {
                if (x) {
                    if (x) {
                    }
                }
            }
        ''')
        cog_flat, _ = analyzer._analyze_control_flow(flat)
        cog_nested, _ = analyzer._analyze_control_flow(nested)
        self.assertEqual(cog_flat, 2)
        self.assertEqual(cog_nested, 3)

    def test_max_nesting_depth(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        code = '''
            void f(int x) {
                if (x) {
                    if (x) {
                        if (x) {
                            for (int i = 0; i < 10; ++i) { }
                        }
                    }
                }
            }
        '''
        _, max_nesting = analyzer._analyze_control_flow(code)
        self.assertEqual(max_nesting, 4)


class TestRawStringAndMacroHandling(unittest.TestCase):
    def test_raw_string_not_counted(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        code = 'const char* s = R"(if (x) for (y))";'
        cleaned, _ = analyzer.remove_comments_and_strings(code)
        # Branches inside the raw string must not appear in cleaned code.
        self.assertNotIn('if', cleaned)
        self.assertNotIn('for', cleaned)

    def test_ignore_macro_strips_assert_args(self):
        analyzer = ac.CppAnalyzer(
            '.', exclude_dirs=[], ignore_macros=['ZENITH_ASSERT']
        )
        code = 'void f(int x) { ZENITH_ASSERT(x > 0 && x < 10, "bad"); }'
        cleaned, _ = analyzer.remove_comments_and_strings(code)
        stripped = analyzer._strip_ignored_macros(cleaned)
        # The assert's logical operators should have been blanked.
        self.assertNotIn('&&', stripped)
        self.assertEqual(analyzer.calculate_cyclomatic_complexity(stripped), 1)


class TestFunctionDetection(unittest.TestCase):
    def test_finds_simple_function(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        code = 'int Add(int a, int b) {\n    return a + b;\n}'
        funcs = analyzer._find_function_boundaries(code)
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'Add')

    def test_finds_qualified_method(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        code = 'int Foo::Bar(int x) {\n    return x;\n}'
        funcs = analyzer._find_function_boundaries(code)
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'Foo::Bar')

    def test_rejects_control_flow_masquerading_as_function(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        # `if (cond) { ... }` at file scope shouldn't be counted as a function.
        # The regex requires a preceding return type or qualified name typically;
        # but this guards against regression of the _CONTROL_FLOW_NAMES filter.
        code = '''
            int main() {
                if (argc > 0) {
                    return 1;
                }
                return 0;
            }
        '''
        funcs = analyzer._find_function_boundaries(code)
        names = [f['name'] for f in funcs]
        self.assertIn('main', names)
        self.assertNotIn('if', names)


class TestPriorityScore(unittest.TestCase):
    def test_clean_function_scores_low(self):
        score, _ = ac.compute_function_priority_score(
            cognitive_complexity=2, cyclomatic_complexity=2,
            max_nesting_depth=1, code_lines=10, param_count=2,
        )
        self.assertLess(score, 30)

    def test_bad_function_scores_high(self):
        score, _ = ac.compute_function_priority_score(
            cognitive_complexity=100, cyclomatic_complexity=50,
            max_nesting_depth=10, code_lines=500, param_count=10,
        )
        self.assertGreater(score, 90)

    def test_factors_sum_to_100(self):
        _, factors = ac.compute_function_priority_score(
            cognitive_complexity=20, cyclomatic_complexity=10,
            max_nesting_depth=4, code_lines=60, param_count=3,
        )
        self.assertAlmostEqual(sum(factors.values()), 100.0, places=0)

    def test_factors_zero_when_score_zero(self):
        score, factors = ac.compute_function_priority_score(
            cognitive_complexity=0, cyclomatic_complexity=0,
            max_nesting_depth=0, code_lines=0, param_count=0,
        )
        self.assertEqual(score, 0.0)
        self.assertTrue(all(v == 0.0 for v in factors.values()))


class TestDuplicateDetection(unittest.TestCase):
    def test_tokenize_normalizes_identifiers(self):
        tokens = ac._tokenize_for_dedup('int foo(int bar) { return bar + 1; }')
        # Identifiers become _ID except for kept tokens (`int`-like keywords are
        # not in the keep set — we only preserve control-flow/structure keywords).
        # `return` IS kept.
        self.assertIn('return', tokens)
        self.assertIn('_ID', tokens)
        self.assertIn('_N', tokens)

    def test_finds_copy_paste_across_files(self):
        body_a = '''
            void ComputeA(int n) {
                int sum_a = 0;
                for (int i = 0; i < n; ++i) {
                    if (i % 2 == 0) {
                        sum_a += i;
                    } else {
                        sum_a -= i;
                    }
                }
                return;
            }
        '''
        body_b = '''
            void ComputeB(int count) {
                int total = 0;
                for (int k = 0; k < count; ++k) {
                    if (k % 2 == 0) {
                        total += k;
                    } else {
                        total -= k;
                    }
                }
                return;
            }
        '''
        analyzer = _analyze_source(body_a + '\n' + body_b)
        self.assertEqual(len(analyzer.duplicate_clusters), 1)
        members = analyzer.duplicate_clusters[0]
        names = {m['name'] for m in members}
        self.assertEqual(names, {'ComputeA', 'ComputeB'})

    def test_distinct_functions_no_cluster(self):
        body = '''
            int Add(int a, int b) { return a + b; }
            int Sub(int a, int b) { return a - b; }
            // Both too short (<30 tokens), so shingles are empty → no dedup attempt.
        '''
        analyzer = _analyze_source(body)
        self.assertEqual(analyzer.duplicate_clusters, [])


class TestIncludeGraph(unittest.TestCase):
    def test_captures_local_includes_only(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_inc_')
        a_path = Path(tmpdir) / 'a.h'
        b_path = Path(tmpdir) / 'b.h'
        a_path.write_text('#pragma once\nint a_val;\n', encoding='utf-8')
        b_path.write_text(
            '#include "a.h"\n#include <vector>\n#include <cstdio>\nint b_val = 0;\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        by_name = {Path(f.filepath).name: f for f in analyzer.file_metrics}
        self.assertEqual(by_name['b.h'].includes, ['a.h'])
        self.assertEqual(by_name['b.h'].fan_out, 1)
        self.assertEqual(by_name['a.h'].fan_in, 1)

    def test_detects_cycle(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_cycle_')
        (Path(tmpdir) / 'a.h').write_text('#include "b.h"\n', encoding='utf-8')
        (Path(tmpdir) / 'b.h').write_text('#include "a.h"\n', encoding='utf-8')
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        self.assertEqual(len(analyzer.include_cycles), 1)
        self.assertTrue(all(f.in_cycle for f in analyzer.file_metrics))

    def test_detects_three_node_cycle(self):
        # a -> b -> c -> a: the back-edge closes onto an ANCESTOR (not the immediate
        # parent). All three must land in ONE cycle. Regression guard for the Tarjan
        # parent-lowlink fix (the old call_stack logic split this into {b,c} + {a}).
        tmpdir = tempfile.mkdtemp(prefix='complex_test_cycle3_')
        (Path(tmpdir) / 'a.h').write_text('#include "b.h"\n', encoding='utf-8')
        (Path(tmpdir) / 'b.h').write_text('#include "c.h"\n', encoding='utf-8')
        (Path(tmpdir) / 'c.h').write_text('#include "a.h"\n', encoding='utf-8')
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        self.assertEqual(len(analyzer.include_cycles), 1)
        self.assertEqual(len(analyzer.include_cycles[0]), 3)
        self.assertTrue(all(f.in_cycle for f in analyzer.file_metrics))


class TestTarjanSCC(unittest.TestCase):
    """Direct tests of the shared SCC helper (file- and module-cycle backbone)."""

    def test_three_node_cycle_is_one_scc(self):
        sccs = ac._tarjan_scc([[1], [2], [0]])          # 0 -> 1 -> 2 -> 0
        self.assertEqual(len(sccs), 1)
        self.assertEqual(sorted(sccs[0]), [0, 1, 2])

    def test_dag_is_all_singletons(self):
        sccs = ac._tarjan_scc([[1], [2], []])           # 0 -> 1 -> 2 (acyclic)
        self.assertEqual(sorted(len(s) for s in sccs), [1, 1, 1])

    def test_two_disjoint_cycles(self):
        sccs = ac._tarjan_scc([[1], [0], [3], [4], [2]])  # {0,1} and {2,3,4}
        self.assertEqual(sorted(len(s) for s in sccs), [2, 3])

    def test_cycle_back_to_ancestor_with_tail(self):
        # 0 -> 1 -> 2 -> 3 -> 1: SCC {1,2,3}; 0 stands alone outside it.
        sccs = ac._tarjan_scc([[1], [2], [3], [1]])
        comps = sorted((sorted(s) for s in sccs), key=len)
        self.assertEqual(comps[-1], [1, 2, 3])
        self.assertIn([0], comps)


class TestDirectoryAggregates(unittest.TestCase):
    def test_percentiles_populated(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_dir_')
        # Two files with very different shapes.
        (Path(tmpdir) / 'clean.cpp').write_text(
            'int trivial() { return 0; }\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'messy.cpp').write_text(textwrap.dedent('''
            int messy(int x) {
                for (int i = 0; i < x; ++i) {
                    if (i > 0) {
                        if (i % 2) {
                            if (i % 3) {
                                if (i % 5) {
                                    for (int j = 0; j < i; ++j) {
                                        if (j > i/2) return j;
                                    }
                                }
                            }
                        }
                    }
                }
                return 0;
            }
        '''), encoding='utf-8')
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        self.assertTrue(analyzer.directory_metrics)
        dm = next(iter(analyzer.directory_metrics.values()))
        self.assertGreater(dm.max_priority, dm.p50_priority)
        self.assertTrue(dm.worst_file.endswith('messy.cpp'))


class TestLCOM(unittest.TestCase):
    def test_high_cohesion_class_gets_low_lcom(self):
        # All three methods touch both fields → strong cohesion, LCOM5 should be low.
        code = '''
            class Tight {
                int m_a;
                int m_b;
                void F() { m_a = 1; m_b = 2; }
                void G() { m_a += m_b; }
                void H() { m_b -= m_a; }
            };
        '''
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        cleaned, _ = analyzer.remove_comments_and_strings(code)
        classes = analyzer._compute_class_lcom(cleaned)
        self.assertEqual(len(classes), 1)
        self.assertLess(classes[0]['lcom5'], 0.2)

    def test_low_cohesion_class_gets_high_lcom(self):
        # Three methods, three fields, each method touches one field.
        code = '''
            class Loose {
                int m_a;
                int m_b;
                int m_c;
                void F() { m_a = 1; }
                void G() { m_b = 2; }
                void H() { m_c = 3; }
            };
        '''
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        cleaned, _ = analyzer.remove_comments_and_strings(code)
        classes = analyzer._compute_class_lcom(cleaned)
        self.assertEqual(len(classes), 1)
        self.assertGreater(classes[0]['lcom5'], 0.6)
        self.assertEqual(classes[0]['method_count'], 3)
        self.assertEqual(classes[0]['field_count'], 3)


@unittest.skipUnless(ac.HAS_TREE_SITTER, 'tree-sitter / tree-sitter-cpp not installed')
class TestTreeSitterParser(unittest.TestCase):
    def test_finds_simple_function(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        code = 'int Add(int a, int b) {\n    return a + b;\n}'
        funcs = analyzer._find_function_boundaries(code)
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'Add')

    def test_finds_nested_template_return_type(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        # The regex path is documented to miss this (nested template args).
        code = 'std::vector<std::pair<int, int>> BuildPairs() {\n    return {};\n}'
        funcs = analyzer._find_function_boundaries(code)
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'BuildPairs')

    def test_finds_operator_overload(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        code = 'bool Vec3::operator==(const Vec3& rhs) const {\n    return true;\n}'
        funcs = analyzer._find_function_boundaries(code)
        self.assertEqual(len(funcs), 1)
        self.assertIn('operator', funcs[0]['name'])

    def test_skips_lambda_as_separate_function(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        code = '''
            void Host() {
                auto f = [&](int x) { return x + 1; };
                f(3);
            }
        '''
        funcs = analyzer._find_function_boundaries(code)
        # Only the enclosing Host() is reported; lambda body is absorbed.
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'Host')

    def test_skips_deleted_function(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        code = '''
            class Foo {
                Foo() = default;
                Foo(const Foo&) = delete;
                void Bar() { return; }
            };
        '''
        funcs = analyzer._find_function_boundaries(code)
        # Only Bar() has a body. =default / =delete have no compound_statement.
        self.assertEqual(len(funcs), 1)
        self.assertEqual(funcs[0]['name'], 'Bar')

    def test_fallback_when_disabled(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='regex')
        self.assertEqual(analyzer.parser_backend, 'regex')


class TestPercentile(unittest.TestCase):
    def test_median(self):
        self.assertEqual(ac._percentile([1, 2, 3, 4, 5], 50), 3.0)

    def test_p90(self):
        self.assertAlmostEqual(ac._percentile([1, 2, 3, 4, 5], 90), 4.6)

    def test_empty(self):
        self.assertEqual(ac._percentile([], 50), 0.0)

    def test_single(self):
        self.assertEqual(ac._percentile([7], 75), 7.0)


class TestLLMCache(unittest.TestCase):
    _FIXTURE = textwrap.dedent('''\
        int Messy(int x) {
            for (int i = 0; i < x; ++i) {
                if (i > 0) {
                    if (i % 2) {
                        if (i % 3) { return i; }
                    }
                }
            }
            return 0;
        }
    ''')

    def _make_analyzer(self) -> tuple:
        tmpdir = tempfile.mkdtemp(prefix='llm_cache_test_')
        (Path(tmpdir) / 'hotspot.cpp').write_text(self._FIXTURE, encoding='utf-8')
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        return analyzer, Path(tmpdir)

    def test_cache_hit_skips_api_call(self):
        """Pre-populated cache entry must be returned without calling messages.create."""
        analyzer, root = self._make_analyzer()
        if not analyzer.function_metrics:
            self.skipTest('no functions detected in fixture')

        fm = max(analyzer.function_metrics, key=lambda f: f.priority_score)
        model = 'test-model'

        source = ac._read_function_source(root, fm)
        self.assertIsNotNone(source, 'fixture source must be readable')
        source_hash = hashlib.sha256(
            (fm.filepath + '\n' + source).encode('utf-8', errors='ignore')
        ).hexdigest()
        cache_key = f'{model}:{source_hash}'

        cached_entry = {
            'function': fm.name, 'file': fm.filepath,
            'start_line': fm.start_line,
            'summary': 'pre-cached suggestion',
            'recommendations': [],
        }
        cache_file = root / 'llm_cache.json'
        cache_file.write_text(json.dumps({cache_key: cached_entry}), encoding='utf-8')

        fake_anthropic = mock.MagicMock()
        with mock.patch.dict(os.environ, {'ANTHROPIC_API_KEY': 'fake-key'}):
            with mock.patch.dict(sys.modules, {'anthropic': fake_anthropic}):
                results = ac.generate_llm_suggestions(
                    analyzer, root, max_functions=10, model=model, cache_path=cache_file,
                )

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]['summary'], 'pre-cached suggestion')
        # Client is created but messages.create must not fire for a full cache-hit run.
        fake_anthropic.Anthropic.return_value.messages.create.assert_not_called()

    def test_corrupt_cache_does_not_crash(self):
        """Corrupt JSON in the cache file must fall back to empty cache, not raise."""
        analyzer, root = self._make_analyzer()
        cache_file = root / 'llm_cache.json'
        cache_file.write_text('not valid json {{{', encoding='utf-8')

        fake_anthropic = mock.MagicMock()
        # Force zero hotspots so no API call is attempted after the corrupt-cache fallback.
        with mock.patch.dict(os.environ, {'ANTHROPIC_API_KEY': 'fake-key'}):
            with mock.patch.dict(sys.modules, {'anthropic': fake_anthropic}):
                results = ac.generate_llm_suggestions(
                    analyzer, root, max_functions=0, model='test-model', cache_path=cache_file,
                )

        self.assertIsInstance(results, list)

    def test_missing_api_key_returns_empty(self):
        """No ANTHROPIC_API_KEY env var must return [] without touching the cache or network."""
        analyzer, root = self._make_analyzer()
        env = {k: v for k, v in os.environ.items() if k != 'ANTHROPIC_API_KEY'}
        with mock.patch.dict(os.environ, env, clear=True):
            results = ac.generate_llm_suggestions(
                analyzer, root, max_functions=5, model='test-model', cache_path=None,
            )
        self.assertEqual(results, [])


class TestExports(unittest.TestCase):
    def _make_analyzer(self) -> tuple:
        tmpdir = tempfile.mkdtemp(prefix='export_test_')
        (Path(tmpdir) / 'simple.cpp').write_text(
            'int Add(int a, int b) { return a + b; }\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        return analyzer, Path(tmpdir)

    def test_html_export_creates_nonempty_file(self):
        analyzer, tmpdir = self._make_analyzer()
        ac.export_html(analyzer, tmpdir)
        out = tmpdir / 'analysis_report.html'
        self.assertTrue(out.exists(), 'HTML report was not created')
        self.assertGreater(out.stat().st_size, 1000, 'HTML report is suspiciously small')

    def test_sarif_export_creates_valid_json(self):
        analyzer, tmpdir = self._make_analyzer()
        ac.export_sarif(analyzer, tmpdir)
        out = tmpdir / 'analysis_report.sarif'
        self.assertTrue(out.exists(), 'SARIF report was not created')
        data = json.loads(out.read_text(encoding='utf-8'))
        self.assertEqual(data.get('version'), '2.1.0')
        self.assertIn('runs', data)


class TestProfileLoading(unittest.TestCase):
    """Profiles let CheckComplexity.bat / RunAnalysis.bat hand off all configuration
    to a JSON file. These tests check the loader and value-merge semantics."""

    def _write_profile_file(self, body: dict) -> Path:
        tmpdir = tempfile.mkdtemp(prefix='profile_test_')
        path = Path(tmpdir) / 'profiles.json'
        path.write_text(json.dumps(body), encoding='utf-8')
        return path

    def test_resolve_profile_returns_named_section(self):
        path = self._write_profile_file({
            'engine-ci': {'exclude_dirs': ['Games', 'Tools'], 'fail_on': {'max-cc': 100}},
        })
        data = ac._resolve_profile('engine-ci', path)
        self.assertEqual(data['exclude_dirs'], ['Games', 'Tools'])
        self.assertEqual(data['fail_on'], {'max-cc': 100})

    def test_resolve_profile_unknown_name_raises(self):
        path = self._write_profile_file({'engine-ci': {}})
        with self.assertRaises(KeyError):
            ac._resolve_profile('does-not-exist', path)

    def test_resolve_profile_missing_file_raises(self):
        with self.assertRaises(FileNotFoundError):
            ac._resolve_profile('any', Path('/nonexistent/path/profiles.json'))

    def test_shipped_profile_file_has_engine_ci_and_full_snapshot(self):
        # Sanity check on the file shipped next to the analyzer so a typo can't
        # silently break both wrappers at once.
        shipped = Path(ac.__file__).resolve().parent / 'complexity_profiles.json'
        self.assertTrue(shipped.exists(), 'complexity_profiles.json should ship with analyzer')
        engine_ci = ac._resolve_profile('engine-ci', shipped)
        full_snap = ac._resolve_profile('full-snapshot', shipped)
        self.assertIn('Games', engine_ci['exclude_dirs'])
        self.assertIn('Tools', engine_ci['exclude_dirs'])
        # Ceiling, not a target: bumped to 105 (2026-05-12) to cover the current worst-case
        # engine function; keep this in sync with complexity_profiles.json.
        self.assertEqual(engine_ci['fail_on']['max-cc'], 105)
        self.assertEqual(full_snap['fail_on'], {})


class TestExcludeGlob(unittest.TestCase):
    def test_glob_excludes_matching_files(self):
        tmpdir = tempfile.mkdtemp(prefix='glob_test_')
        (Path(tmpdir) / 'real.cpp').write_text(
            'int Add(int a, int b) { return a + b; }\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'fake.Tests.inl').write_text(
            'int FakeAdd(int a, int b) { return a + b; }\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[], exclude_globs=['**/*.Tests.inl'])
        analyzer.analyze(workers=1)
        paths = [Path(fm.filepath).name for fm in analyzer.file_metrics]
        self.assertIn('real.cpp', paths)
        self.assertNotIn('fake.Tests.inl', paths)

    def test_glob_no_match_keeps_file(self):
        tmpdir = tempfile.mkdtemp(prefix='glob_test_')
        (Path(tmpdir) / 'real.cpp').write_text(
            'int Add(int a, int b) { return a + b; }\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[], exclude_globs=['**/*.unrelated'])
        analyzer.analyze(workers=1)
        self.assertEqual(len(analyzer.file_metrics), 1)


class TestParserAutoMode(unittest.TestCase):
    def test_auto_picks_tree_sitter_when_available(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='auto')
        if ac.HAS_TREE_SITTER:
            self.assertEqual(analyzer.parser_backend, 'tree-sitter')
        else:
            self.assertEqual(analyzer.parser_backend, 'regex')
        self.assertEqual(analyzer.parser_backend_requested, 'auto')

    def test_explicit_tree_sitter_falls_back_to_regex_with_message(self):
        if ac.HAS_TREE_SITTER:
            self.skipTest('tree-sitter is installed; cannot exercise fallback path')
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='tree-sitter')
        self.assertEqual(analyzer.parser_backend, 'regex')
        self.assertEqual(analyzer.parser_backend_requested, 'tree-sitter')

    def test_explicit_regex_stays_regex(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[], parser_backend='regex')
        self.assertEqual(analyzer.parser_backend, 'regex')
        self.assertEqual(analyzer.parser_backend_requested, 'regex')


class TestExpandedFailOn(unittest.TestCase):
    def test_max_function_priority_flags_high_score(self):
        # A function with high CC + nesting + length will earn a high priority score.
        code = textwrap.dedent('''
            void f(int x) {
                if (x > 0) {
                    for (int i = 0; i < 100; ++i) {
                        if (i % 2 == 0) {
                            for (int j = 0; j < 50; ++j) {
                                if (j > 10) {
                                    while (j-- > 0) { if (j == 5) break; }
                                }
                            }
                        }
                    }
                }
            }
        ''')
        analyzer = _analyze_source(code)
        # Force a low ceiling so even a small synthetic function trips it.
        violations = ac.check_fail_on(analyzer, {'max-function-priority': 1.0})
        self.assertTrue(any('priority=' in v for v in violations))

    def test_max_include_cycles_flags_when_exceeded(self):
        # Stub: file_metrics with one fake cycle.
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        analyzer.include_cycles = [['a.h', 'b.h']]
        violations = ac.check_fail_on(analyzer, {'max-include-cycles': 0})
        self.assertEqual(len(violations), 1)
        self.assertIn('include cycles=1', violations[0])

    def test_max_duplicate_clusters_flags_when_exceeded(self):
        analyzer = ac.CppAnalyzer('.', exclude_dirs=[])
        analyzer.duplicate_clusters = [
            [{'file': 'a.cpp', 'line': 1, 'name': 'f', 'code_lines': 5, 'priority_score': 10}],
            [{'file': 'b.cpp', 'line': 2, 'name': 'g', 'code_lines': 5, 'priority_score': 10}],
        ]
        violations = ac.check_fail_on(analyzer, {'max-duplicate-clusters': 1})
        self.assertEqual(len(violations), 1)
        self.assertIn('duplicate clusters=2', violations[0])

    def test_unknown_key_raises(self):
        with self.assertRaises(ValueError):
            ac._parse_fail_on('max-frobnicate=10')

    def test_known_keys_parse(self):
        out = ac._parse_fail_on('max-cc=30,max-function-priority=70,max-include-cycles=0')
        self.assertEqual(out['max-cc'], 30.0)
        self.assertEqual(out['max-function-priority'], 70.0)
        self.assertEqual(out['max-include-cycles'], 0.0)


class TestIncludeResolution(unittest.TestCase):
    def test_relative_path_match_preferred_over_basename(self):
        tmpdir = tempfile.mkdtemp(prefix='inc_test_')
        # Two files with the same basename in different subdirs. Without
        # relative-path resolution, basename matching would link to both.
        (Path(tmpdir) / 'A').mkdir()
        (Path(tmpdir) / 'B').mkdir()
        (Path(tmpdir) / 'A' / 'Foo.h').write_text('// A/Foo.h\n', encoding='utf-8')
        (Path(tmpdir) / 'B' / 'Foo.h').write_text('// B/Foo.h\n', encoding='utf-8')
        # Caller uses the qualified path — should resolve to exactly one.
        (Path(tmpdir) / 'Caller.cpp').write_text(
            '#include "A/Foo.h"\nvoid f() {}\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        caller = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('Caller.cpp'))
        # fan_out should be 1 (the exact-match A/Foo.h), not 2 (both basenames).
        self.assertEqual(caller.fan_out, 1)
        # Resolution counter should record the relative-path match.
        self.assertGreaterEqual(analyzer.include_edge_relative_count, 1)

    def test_basename_fallback_used_when_no_relative_match(self):
        tmpdir = tempfile.mkdtemp(prefix='inc_test_')
        (Path(tmpdir) / 'Foo.h').write_text('// Foo.h\n', encoding='utf-8')
        (Path(tmpdir) / 'Caller.cpp').write_text(
            '#include "Foo.h"\nvoid f() {}\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        # Foo.h is at the root, so it matches via the rel-path index too. This
        # primarily exercises the no-cycle case; the counters should sum to fan_out.
        caller = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('Caller.cpp'))
        self.assertEqual(caller.fan_out, 1)


class TestIncludeRedundancy(unittest.TestCase):
    def test_duplicate_include_flagged(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'x.h').write_text('#pragma once\n', encoding='utf-8')
        # Same raw include string on lines 1 and 3.
        (Path(tmpdir) / 'a.cpp').write_text(
            '#include "x.h"\nvoid f() {}\n#include "x.h"\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_a = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('a.cpp'))
        self.assertEqual(len(fm_a.duplicate_includes), 1)
        rec = fm_a.duplicate_includes[0]
        self.assertEqual(rec['line'], 3)
        self.assertEqual(rec['first_line'], 1)
        self.assertEqual(rec['include'], 'x.h')

    def test_transitive_include_flagged(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        # top.cpp directly includes both — leaf.h is redundant because mid.h pulls it in.
        (Path(tmpdir) / 'top.cpp').write_text(
            '#include "mid.h"\n#include "leaf.h"\nvoid f() {}\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_top = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('top.cpp'))
        self.assertEqual(len(fm_top.transitive_includes), 1)
        rec = fm_top.transitive_includes[0]
        self.assertEqual(rec['include'], 'leaf.h')
        self.assertTrue(rec['covered_by'].endswith('mid.h'))

    def test_unresolved_include_flagged(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'solo.cpp').write_text(
            '#include "does_not_exist.h"\nvoid f() {}\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_solo = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('solo.cpp'))
        self.assertEqual(len(fm_solo.unresolved_includes), 1)
        rec = fm_solo.unresolved_includes[0]
        self.assertEqual(rec['include'], 'does_not_exist.h')
        self.assertEqual(rec['line'], 1)
        # Genuine errors must NOT count toward externals.
        self.assertEqual(len(fm_solo.external_includes), 0)

    def test_conditional_cover_skipped_in_transitive(self):
        # If the only cover path goes through a `#ifdef`-wrapped include, the analyzer
        # must NOT report transitive coverage — removing the redundant include would
        # break builds that don't define the macro.
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        # mid.h is included only inside `#ifdef ZENITH_TOOLS`, so its coverage of
        # leaf.h is conditional. The unconditional include of leaf.h must NOT be
        # flagged as transitive.
        (Path(tmpdir) / 'top.cpp').write_text(
            '#ifdef ZENITH_TOOLS\n#include "mid.h"\n#endif\n#include "leaf.h"\nvoid f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_top = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('top.cpp'))
        self.assertEqual(len(fm_top.transitive_includes), 0)
        # The conditional flag on the include should reflect the `#ifdef` wrap.
        self.assertEqual(fm_top.include_conditional, [True, False])

    def test_conditional_source_include_not_flagged_transitive(self):
        # The redundant include itself is conditional. Even if a cover exists,
        # don't flag — the user might be intentionally re-asserting the include
        # under a different macro context.
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'top.cpp').write_text(
            '#include "mid.h"\n#ifdef ZENITH_TOOLS\n#include "leaf.h"\n#endif\nvoid f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_top = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('top.cpp'))
        self.assertEqual(len(fm_top.transitive_includes), 0)

    def test_transitive_skipped_when_cover_appears_after_redundant(self):
        # If the only cover path is included AFTER the redundant include, code
        # between the two might use the type and rely on the explicit include.
        # Removing the early one would break parse order.
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        # Order: leaf.h FIRST (line 1), mid.h LATER (line 2). leaf.h must NOT be
        # flagged transitive — mid.h's coverage only kicks in after line 2.
        (Path(tmpdir) / 'top.cpp').write_text(
            '#include "leaf.h"\n#include "mid.h"\nvoid f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_top = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('top.cpp'))
        self.assertEqual(len(fm_top.transitive_includes), 0)

    def test_unconditional_transitive_still_flagged(self):
        # Sanity: when both candidate and cover are unconditional, transitive
        # detection still works.
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'top.cpp').write_text(
            '#include "mid.h"\n#include "leaf.h"\nvoid f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_top = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('top.cpp'))
        self.assertEqual(len(fm_top.transitive_includes), 1)
        self.assertEqual(fm_top.include_conditional, [False, False])

    def test_external_include_routed_to_external_not_unresolved(self):
        # A header that lives under an excluded directory (e.g. Middleware/) should
        # surface as external (informational), NOT as an unresolved error.
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'Middleware').mkdir()
        (Path(tmpdir) / 'Middleware' / 'imgui').mkdir()
        (Path(tmpdir) / 'Middleware' / 'imgui' / 'imgui.h').write_text(
            '// imgui header\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'caller.cpp').write_text(
            '#include "imgui.h"\n#include "missing.h"\nvoid f() {}\n', encoding='utf-8'
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=['Middleware'])
        analyzer.analyze(workers=1)
        fm = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('caller.cpp'))
        # imgui.h resolves to Middleware/imgui/imgui.h via basename → external.
        self.assertEqual(len(fm.external_includes), 1)
        self.assertEqual(fm.external_includes[0]['include'], 'imgui.h')
        self.assertTrue(fm.external_includes[0]['resolved_to'].endswith('imgui.h'))
        # missing.h has no match anywhere → unresolved error.
        self.assertEqual(len(fm.unresolved_includes), 1)
        self.assertEqual(fm.unresolved_includes[0]['include'], 'missing.h')
        # Externals don't contribute to optimisation score; only the four cleanup
        # categories do.
        self.assertEqual(
            fm.include_optimisation_score,
            len(fm.duplicate_includes) + len(fm.transitive_includes)
            + len(fm.unresolved_includes) + len(fm.conflicting_includes),
        )

    def test_conflicting_basename_includes_flagged(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'Net').mkdir()
        (Path(tmpdir) / 'Audio').mkdir()
        (Path(tmpdir) / 'Net' / 'Foo.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'Audio' / 'Foo.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'caller.cpp').write_text(
            '#include "Net/Foo.h"\n#include "Audio/Foo.h"\nvoid f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm_caller = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('caller.cpp'))
        self.assertEqual(len(fm_caller.conflicting_includes), 1)
        rec = fm_caller.conflicting_includes[0]
        self.assertEqual(rec['conflicts_with'], 'Net/Foo.h')
        self.assertEqual(rec['first_line'], 1)
        self.assertEqual(rec['line'], 2)
        self.assertFalse(rec['same_target'])

    def test_optimisation_score_aggregates_all_four(self):
        tmpdir = tempfile.mkdtemp(prefix='complex_test_redundancy_')
        (Path(tmpdir) / 'leaf.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'mid.h').write_text(
            '#pragma once\n#include "leaf.h"\n', encoding='utf-8'
        )
        (Path(tmpdir) / 'Net').mkdir()
        (Path(tmpdir) / 'Audio').mkdir()
        (Path(tmpdir) / 'Net' / 'Foo.h').write_text('#pragma once\n', encoding='utf-8')
        (Path(tmpdir) / 'Audio' / 'Foo.h').write_text('#pragma once\n', encoding='utf-8')
        # Lines (1-based):
        #   1: #include "mid.h"
        #   2: #include "leaf.h"      -> transitive (covered by mid.h)
        #   3: #include "Net/Foo.h"
        #   4: #include "Audio/Foo.h" -> conflicting basename
        #   5: #include "leaf.h"      -> duplicate
        #   6: #include "missing.h"   -> unresolved
        (Path(tmpdir) / 'all.cpp').write_text(
            '#include "mid.h"\n#include "leaf.h"\n#include "Net/Foo.h"\n'
            '#include "Audio/Foo.h"\n#include "leaf.h"\n#include "missing.h"\n'
            'void f() {}\n',
            encoding='utf-8',
        )
        analyzer = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        analyzer.analyze(workers=1)
        fm = next(fm for fm in analyzer.file_metrics if fm.filepath.endswith('all.cpp'))
        self.assertGreaterEqual(len(fm.duplicate_includes), 1)
        self.assertGreaterEqual(len(fm.transitive_includes), 1)
        self.assertGreaterEqual(len(fm.unresolved_includes), 1)
        self.assertGreaterEqual(len(fm.conflicting_includes), 1)
        self.assertEqual(
            fm.include_optimisation_score,
            len(fm.duplicate_includes) + len(fm.transitive_includes)
            + len(fm.unresolved_includes) + len(fm.conflicting_includes),
        )
        self.assertGreaterEqual(fm.include_optimisation_score, 4)
        h = ac.get_health_summary(analyzer)
        self.assertGreaterEqual(h['total_include_optimisation_score'], 4)


class TestHealthSummary(unittest.TestCase):
    def test_health_summary_populated(self):
        code = 'int Add(int a, int b) { return a + b; }\n'
        analyzer = _analyze_source(code)
        h = ac.get_health_summary(analyzer)
        self.assertIn('parser_backend', h)
        self.assertIn('include_cycle_count', h)
        self.assertIn('duplicate_cluster_count', h)
        self.assertIn('worst_function', h)
        self.assertIn('worst_file', h)
        self.assertEqual(h['profile'], '(none)')

    def test_health_summary_appears_in_markdown(self):
        code = 'int Add(int a, int b) { return a + b; }\n'
        analyzer = _analyze_source(code)
        tmpdir = Path(tempfile.mkdtemp(prefix='md_health_'))
        ac.export_markdown(analyzer, tmpdir)
        md = (tmpdir / 'analysis_report.md').read_text(encoding='utf-8')
        self.assertIn('## Health summary', md)
        self.assertIn('Parser:', md)
        self.assertIn('Include cycles:', md)


class TestDominantSignal(unittest.TestCase):
    def test_dominant_signal_picks_largest_factor(self):
        # cognitive=50, cyclomatic=30 -> dominant is cognitive.
        self.assertEqual(
            ac._dominant_signal({'cognitive': 50.0, 'cyclomatic': 30.0, 'nesting': 20.0}),
            'cognitive',
        )

    def test_dominant_signal_none_for_empty(self):
        self.assertIsNone(ac._dominant_signal({}))
        self.assertIsNone(ac._dominant_signal({'cognitive': 0.0, 'cyclomatic': 0.0}))


class TestDuplicateFiltering(unittest.TestCase):
    def test_test_only_filter_drops_tests_inl_dupes(self):
        # Two near-identical functions, one in *.Tests.inl and one in a real .cpp.
        # With filter on, the cluster should not appear because the test-only one
        # is dropped from candidates and we only have one real candidate left.
        body = 'for (int i = 0; i < 10; ++i) { if (i % 2 == 0) { sum += i; } else { sum -= i; } }'
        tests_inl = f'void TestF(int& sum) {{ {body} }}\n'
        real_cpp = f'void RealF(int& sum) {{ {body} }}\n'
        tmpdir = tempfile.mkdtemp(prefix='dup_filter_')
        (Path(tmpdir) / 'real.cpp').write_text(real_cpp, encoding='utf-8')
        (Path(tmpdir) / 'fake.Tests.inl').write_text(tests_inl, encoding='utf-8')
        # Without filter: cluster should appear.
        a = ac.CppAnalyzer(tmpdir, exclude_dirs=[])
        a.analyze(workers=1)
        self.assertGreaterEqual(len(a.duplicate_clusters), 1)
        # With test-only filter: cluster gone (only one non-test candidate remains).
        b = ac.CppAnalyzer(tmpdir, exclude_dirs=[], filter_test_only_duplicates=True)
        b.analyze(workers=1)
        self.assertEqual(len(b.duplicate_clusters), 0)


class TestLCOMCaveat(unittest.TestCase):
    def test_lcom_record_has_header_inline_caveat(self):
        code = textwrap.dedent('''
            class C {
            public:
                void A() { m_x = 1; }
                void B() { m_y = 2; }
                void C2() { m_z = 3; }
                int m_x; int m_y; int m_z;
            };
        ''')
        analyzer = _analyze_source(code)
        # Find the file we just analyzed.
        target = next((fm for fm in analyzer.file_metrics if fm.classes), None)
        self.assertIsNotNone(target, 'no class records found — fixture broken')
        for cls in target.classes:
            self.assertEqual(cls.get('caveat'), 'header-inline only')


def _analyze_tree(files: dict, architecture=None, lints=None, workers=1, exclude_dirs=None):
    """Write a {relpath: content} tree to a temp dir and run full analysis on it."""
    tmpdir = tempfile.mkdtemp(prefix='arch_test_')
    for rel, content in files.items():
        p = Path(tmpdir) / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(textwrap.dedent(content), encoding='utf-8')
    an = ac.CppAnalyzer(tmpdir, exclude_dirs=exclude_dirs or [],
                        architecture=architecture or {}, lints=lints or {})
    an.analyze(workers=workers)
    return an


# A reusable two-layer architecture: low=L0, high=L1, leaf=L1 (leaf+public surface).
_ARCH = {
    'modules': [
        {'name': 'low', 'layer': 0, 'globs': ['low/**']},
        {'name': 'high', 'layer': 1, 'globs': ['high/**']},
        {'name': 'leaf', 'layer': 1, 'globs': ['leaf/**'], 'leaf': True,
         'public_headers': ['leaf/*.h']},
    ],
    'rules': {
        'forbid_up_edges': True,
        'forbidden_edges': [{'from': 'leaf', 'to': ['high']}],
        'allowed_edges': [],
        'ignore_module_cycles': [],
    },
    'fail_on_basename_resolved': False,
}


def _kinds(an):
    out = {}
    for v in an.architecture_violations:
        out.setdefault(v['kind'], []).append(v)
    return out


class TestArchitectureMapping(unittest.TestCase):
    def test_files_map_to_modules_and_unmapped(self):
        an = _analyze_tree({
            'low/a.h': '#pragma once\nint a;\n',
            'high/b.h': '#pragma once\nint b;\n',
            'orphan/c.h': '#pragma once\nint c;\n',
        }, architecture=_ARCH)
        by = {Path(f.filepath).as_posix(): f for f in an.file_metrics}
        self.assertEqual(by['low/a.h'].module, 'low')
        self.assertEqual(by['high/b.h'].module, 'high')
        self.assertEqual(by['orphan/c.h'].module, '')
        self.assertIn('orphan/c.h', [Path(p).as_posix() for p in an.unmapped_files])
        self.assertEqual(an.gate_counts.get('max-unmapped-files'), 1)


class TestArchitectureDirection(unittest.TestCase):
    def test_up_edge_flagged_down_edge_clean(self):
        an = _analyze_tree({
            'low/a.cpp': '#include "high/b.h"\nint a;\n',   # low(L0) -> high(L1): UP
            'high/b.h': '#pragma once\nint b;\n',
            'high/c.cpp': '#include "low/d.h"\nint c;\n',    # high(L1) -> low(L0): down, ok
            'low/d.h': '#pragma once\nint d;\n',
        }, architecture=_ARCH)
        ups = _kinds(an).get('layer-up', [])
        self.assertEqual(len(ups), 1)
        self.assertIn('low/a.cpp => high/b.h', ups[0]['key'])
        self.assertFalse(ups[0]['basename_resolved'])
        self.assertEqual(an.gate_counts['max-architecture-violations'], 1)

    def test_forbidden_same_layer_edge(self):
        an = _analyze_tree({
            'leaf/e.cpp': '#include "high/f.h"\nint e;\n',   # leaf -> high: forbidden
            'high/f.h': '#pragma once\nint f;\n',
        }, architecture=_ARCH)
        fe = _kinds(an).get('forbidden-edge', [])
        self.assertEqual(len(fe), 1)
        self.assertEqual(an.gate_counts['max-architecture-violations'], 1)

    def test_basename_resolved_edge_excluded_from_gate(self):
        # Two files named dup.h -> a bare "dup.h" include resolves by basename (low confidence).
        an = _analyze_tree({
            'low/a.cpp': '#include "dup.h"\nint a;\n',
            'high/dup.h': '#pragma once\nint d1;\n',
            'other/dup.h': '#pragma once\nint d2;\n',
            'other/x.h': '#pragma once\n',
            'high/x.h': '#pragma once\n',
        }, architecture={**_ARCH, 'modules': _ARCH['modules'] + [
            {'name': 'other', 'layer': 1, 'globs': ['other/**']}]})
        ups = _kinds(an).get('layer-up', [])
        # The low->high edge exists but is AMBIGUOUS basename (2 dup.h) -> recorded, not gated.
        self.assertTrue(any(u['basename_resolved'] for u in ups))
        self.assertEqual(an.gate_counts['max-architecture-violations'], 0)

    def test_unique_basename_edge_is_gated(self):
        # A non-matching subpath whose BASENAME is unique resolves unambiguously, so it is
        # high-confidence and DOES gate (only genuinely ambiguous basenames are suppressed).
        an = _analyze_tree({
            'low/a.cpp': '#include "bogus/realname.h"\nint a;\n',
            'high/realname.h': '#pragma once\nint r;\n',
        }, architecture=_ARCH)
        ups = _kinds(an).get('layer-up', [])
        self.assertEqual(len(ups), 1)
        self.assertFalse(ups[0]['basename_resolved'])
        self.assertEqual(an.gate_counts['max-architecture-violations'], 1)

    def test_ignore_module_cycles_pair_suppresses(self):
        arch = {**_ARCH, 'rules': {**_ARCH['rules'],
                                   'ignore_module_cycles': [['low', 'high']]}}
        an = _analyze_tree({
            'low/a.cpp': '#include "high/b.h"\nint a;\n',
            'high/b.h': '#pragma once\nint b;\n',
        }, architecture=arch)
        self.assertEqual(an.gate_counts['max-architecture-violations'], 0)


class TestArchitectureSubsumedRatchets(unittest.TestCase):
    """EC<->Flux + ECS-leaf raw-string ratchets, byte-parity with the retired PS1."""

    def _arch(self, allow_path=None):
        a = {
            'modules': [
                {'name': 'EntityComponent', 'layer': 2, 'globs': ['Zenith/EntityComponent/**']},
                {'name': 'Flux', 'layer': 2, 'globs': ['Zenith/Flux/**']},
            ],
            'rules': {'forbid_up_edges': True},
            'ec_flux_ratchet': {'ec_glob': 'Zenith/EntityComponent/**',
                                'flux_glob': 'Zenith/Flux/**',
                                'allowlist_file': allow_path or 'nope.txt'},
            'leaf_ratchet': {'glob': 'Zenith/ZenithECS/**',
                             'forbidden_prefixes': ['Flux/', 'Physics/'],
                             'forbidden_exact': ['Core/Zenith_Engine.h'],
                             'allowlist_file': 'nope_leaf.txt'},
        }
        return a

    def test_ec_flux_edge_detected_and_deduped(self):
        an = _analyze_tree({
            'Zenith/EntityComponent/X.cpp': '#include "Flux/R.h"\n#include "Flux/R.h"\nint x;\n',
            'Zenith/Flux/R.h': '#pragma once\n',
        }, architecture=self._arch())
        ec = _kinds(an).get('ec-flux', [])
        self.assertEqual(len(ec), 1)   # de-duped despite two identical includes
        self.assertEqual(an.gate_counts['max-ec-flux-edges'], 1)

    def test_ec_flux_allowlist_suppresses(self):
        tmp = Path(tempfile.mkdtemp(prefix='allow_')) / 'a.txt'
        tmp.write_text('Zenith/EntityComponent/X.cpp => Flux/R.h\n', encoding='utf-8')
        an = _analyze_tree({
            'Zenith/EntityComponent/X.cpp': '#include "Flux/R.h"\nint x;\n',
            'Zenith/Flux/R.h': '#pragma once\n',
        }, architecture=self._arch(allow_path=str(tmp)))
        self.assertEqual(an.gate_counts['max-ec-flux-edges'], 0)

    def test_leaf_forbidden_include(self):
        an = _analyze_tree({
            'Zenith/ZenithECS/S.cpp': '#include "Flux/R.h"\nint s;\n',
            'Zenith/Flux/R.h': '#pragma once\n',
        }, architecture=self._arch())
        leaf = _kinds(an).get('leaf-include', [])
        self.assertEqual(len(leaf), 1)
        self.assertEqual(an.gate_counts['max-leaf-violations'], 1)

    def test_commented_include_ignored(self):
        an = _analyze_tree({
            'Zenith/EntityComponent/X.cpp': '// #include "Flux/R.h"\nint x;\n',
            'Zenith/Flux/R.h': '#pragma once\n',
        }, architecture=self._arch())
        self.assertEqual(an.gate_counts['max-ec-flux-edges'], 0)


class TestArchitectureChurn(unittest.TestCase):
    """P1: per-finding allowlist + threshold 0 — a NEW finding fails even when an old one
    is removed (a count-ratchet set to 'current count' would wrongly pass)."""

    def test_remove_old_add_new_still_fails(self):
        allow = Path(tempfile.mkdtemp(prefix='churn_')) / 'arch.txt'
        allow.write_text('layer-up: low/a.cpp => high/b.h\n', encoding='utf-8')
        arch = {**_ARCH, 'allowlist_file': str(allow)}
        # State 1: only the allow-listed edge A -> gate green (count 0).
        an1 = _analyze_tree({
            'low/a.cpp': '#include "high/b.h"\nint a;\n',
            'high/b.h': '#pragma once\n',
        }, architecture=arch)
        self.assertEqual(an1.gate_counts['max-architecture-violations'], 0)
        # State 2: edge A removed, NEW edge B added (not allow-listed) -> count 1, fails.
        an2 = _analyze_tree({
            'low/c.cpp': '#include "high/b.h"\nint c;\n',
            'high/b.h': '#pragma once\n',
        }, architecture=arch)
        self.assertEqual(an2.gate_counts['max-architecture-violations'], 1)
        viol = ac.check_fail_on(an2, {'max-architecture-violations': 0})
        self.assertTrue(any('max-architecture-violations' in v for v in viol))


class TestEncapsulation(unittest.TestCase):
    def test_internal_header_flagged_public_clean(self):
        an = _analyze_tree({
            'high/u.cpp': '#include "leaf/api.h"\n#include "leaf/Internal/secret.h"\nint u;\n',
            'leaf/api.h': '#pragma once\n',
            'leaf/Internal/secret.h': '#pragma once\n',
        }, architecture=_ARCH)
        enc = _kinds(an).get('encapsulation', [])
        self.assertEqual(len(enc), 1)
        self.assertIn('Internal/secret.h', enc[0]['key'])
        self.assertEqual(an.gate_counts['max-encapsulation-violations'], 1)


class TestHubFiles(unittest.TestCase):
    def test_hub_needs_high_in_and_out(self):
        arch = {**_ARCH, 'thresholds': {'hub_fan_in': 2, 'hub_fan_out': 2}}
        # hub.h is included by 2 files AND includes 2 files -> hub.
        an = _analyze_tree({
            'low/hub.h': '#include "low/x.h"\n#include "low/y.h"\n',
            'low/x.h': '#pragma once\n',
            'low/y.h': '#pragma once\n',
            'low/p.cpp': '#include "low/hub.h"\n',
            'low/q.cpp': '#include "low/hub.h"\n',
        }, architecture=arch)
        hubs = [h['file'] for h in an.hub_files]
        self.assertTrue(any(Path(h).as_posix() == 'low/hub.h' for h in hubs))
        # x.h has fan_in 1 (only hub) and fan_out 0 -> not a hub.
        self.assertFalse(any(Path(h).as_posix() == 'low/x.h' for h in hubs))


class TestModuleCoupling(unittest.TestCase):
    def test_instability_chain(self):
        # a(L2) -> b(L1) -> c(L0): all down edges. Ce/Ca/I at the ends.
        arch = {'modules': [
            {'name': 'a', 'layer': 2, 'globs': ['a/**']},
            {'name': 'b', 'layer': 1, 'globs': ['b/**']},
            {'name': 'c', 'layer': 0, 'globs': ['c/**']},
        ], 'rules': {'forbid_up_edges': True}}
        an = _analyze_tree({
            'a/f.cpp': '#include "b/g.h"\n',
            'b/g.h': '#pragma once\n#include "c/h.h"\n',
            'c/h.h': '#pragma once\n',
        }, architecture=arch)
        m = an.module_metrics
        self.assertEqual(m['a'].efferent, 1)
        self.assertEqual(m['a'].afferent, 0)
        self.assertEqual(m['a'].instability, 1.0)
        self.assertEqual(m['c'].efferent, 0)
        self.assertEqual(m['c'].afferent, 1)
        self.assertEqual(m['c'].instability, 0.0)
        self.assertEqual(m['b'].instability, 0.5)

    def test_abstractness_scanner(self):
        an = ac.CppAnalyzer('.', exclude_dirs=[])
        c1, _ = an.remove_comments_and_strings('class I { virtual void f() = 0; };')
        self.assertEqual(an._scan_abstract_types(c1), (True, True))
        c2, _ = an.remove_comments_and_strings('class C { void f() {} };')
        self.assertEqual(an._scan_abstract_types(c2), (True, False))
        c3, _ = an.remove_comments_and_strings('int x = 0;')
        self.assertEqual(an._scan_abstract_types(c3), (False, False))


class TestLints(unittest.TestCase):
    def test_forbidden_token_in_code_not_comment(self):
        an = _analyze_tree({
            'low/a.cpp': '#include "Zenith.h"\nstd::function<void()> g;\n',
            'low/b.cpp': '#include "Zenith.h"\n// std::function in a comment\nint b;\n',
        }, architecture=_ARCH, lints={})
        toks = [v for v in an.lint_findings if v['kind'] == 'token']
        files = {Path(v['file']).as_posix() for v in toks}
        self.assertIn('low/a.cpp', files)
        self.assertNotIn('low/b.cpp', files)
        self.assertEqual(an.gate_counts['max-forbidden-tokens'], 1)

    def test_missing_pragma_once(self):
        an = _analyze_tree({
            'low/good.h': '#pragma once\nint g;\n',
            'low/bad.h': 'int b;\n',
        }, architecture=_ARCH, lints={})
        prag = {Path(v['file']).as_posix() for v in an.lint_findings if v['kind'] == 'pragma'}
        self.assertEqual(prag, {'low/bad.h'})

    def test_missing_pch_first_line(self):
        an = _analyze_tree({
            'low/good.cpp': '#include "Zenith.h"\nint g;\n',
            'low/lic.cpp': '/* licence\n banner */\n#include "Zenith.h"\nint l;\n',  # block comment ok
            'low/bad.cpp': '#include "other.h"\nint b;\n',
        }, architecture=_ARCH, lints={})
        pch = {Path(v['file']).as_posix() for v in an.lint_findings if v['kind'] == 'pch'}
        self.assertEqual(pch, {'low/bad.cpp'})

    def test_gxengine_only_flagged_in_leaf(self):
        arch = {**_ARCH, 'leaf_ratchet': {'glob': 'leaf/**', 'forbidden_prefixes': [],
                                          'allowlist_file': 'nope.txt'}}
        an = _analyze_tree({
            'leaf/s.cpp': '#include "Zenith.h"\nvoid f(){ g_xEngine.X(); }\n',   # in leaf -> gated
            'high/h.cpp': '#include "Zenith.h"\nvoid f(){ g_xEngine.X(); }\n',   # outside leaf -> ok
            'leaf/c.cpp': '#include "Zenith.h"\n// g_xEngine in comment only\nint c;\n',
        }, architecture=arch, lints={})
        gx = [v for v in an.lint_findings
              if v['kind'] == 'engine-singleton' and v.get('in_scope')]
        files = {Path(v['file']).as_posix() for v in gx}
        self.assertEqual(files, {'leaf/s.cpp'})
        self.assertEqual(an.gate_counts['max-leaf-violations'], 1)

    def test_statics_carveout_skipped(self):
        an = _analyze_tree({
            'low/a.cpp': ('#include "Zenith.h"\n'
                          'static int g_counter = 0;\n'                 # flagged
                          'static std::vector<int> s_xBuf;\n'           # flagged
                          'static Zenith_Window s_win;\n'               # carve-out -> skipped
                          'static thread_local int tl_x = 0;\n'         # TLS carve-out -> skipped
                          'void f(){ static int local = 0; }\n'         # function-local -> skipped
                          'static const int kC = 3;\n'),                # const -> skipped
        }, architecture=_ARCH, lints={})
        st = [v for v in an.lint_findings if v['kind'] == 'static']
        keys = {v['key'] for v in st}
        # Symbol-stable keys (not line-based).
        self.assertIn('static:low/a.cpp => g_counter', keys)
        self.assertIn('static:low/a.cpp => s_xBuf', keys)
        joined = ' '.join(v['detail'] for v in st)
        self.assertNotIn('s_win', joined)
        self.assertNotIn('tl_x', joined)
        self.assertNotIn('local', joined)
        self.assertNotIn('kC', joined)
        # Now gated by default (threshold 0) once they're in scope and not allow-listed.
        self.assertEqual(an.gate_counts['max-static-violations'], 2)


class TestParallelLints(unittest.TestCase):
    """P5 guard: lints (computed in analyze_file) must survive the process-pool worker."""

    def test_lints_present_under_workers_2(self):
        an = _analyze_tree({
            'low/a.cpp': '#include "Zenith.h"\nstd::function<void()> g;\n',
            'low/b.h': 'int b;\n',
        }, architecture=_ARCH, lints={}, workers=2)
        kinds = {v['kind'] for v in an.lint_findings}
        self.assertIn('token', kinds)
        self.assertIn('pragma', kinds)


class TestExpandedFailOnKeys(unittest.TestCase):
    def test_new_arch_lint_keys_parse(self):
        out = ac._parse_fail_on(
            'max-architecture-violations=0,max-leaf-violations=0,max-forbidden-tokens=0')
        self.assertEqual(out['max-architecture-violations'], 0.0)
        self.assertEqual(out['max-leaf-violations'], 0.0)
        self.assertEqual(out['max-forbidden-tokens'], 0.0)

    def test_gate_groups_cover_all_keys(self):
        grouped = set()
        for keys in ac._GATE_GROUPS.values():
            grouped.update(keys)
        self.assertEqual(grouped, set(ac._FAIL_ON_KEYS))


if __name__ == '__main__':
    unittest.main()
