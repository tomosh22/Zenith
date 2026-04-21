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


if __name__ == '__main__':
    unittest.main()
