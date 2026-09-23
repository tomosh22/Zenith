"""End-to-end tests for Tools/libgraph/libgraph.py -- the real CLI, run as a
subprocess against tests/fixtures/repo (config `fx_debug` / `fx_release`)
and tests/fixtures/dumpbin, rather than calling libgraph's functions
in-process. This is what actually exercises the CLI's own argument parsing,
error handling and exit codes, none of which any other test file touches.

Run with:
    python -m unittest discover Tools/libgraph/tests -v
    # or, from this directory:
    python -m unittest test_e2e

Every subprocess call is list-form (`[sys.executable, str(script), ...]`,
each path its own element -- never a shell string) and passes
`encoding="utf-8", errors="replace"`. Every invocation, including the error
cases, passes `--dumpbin-dir` (so a run never shells out to a real
dumpbin.exe) and an `--out` pointed at a tempdir, so nothing this suite does
is ever written under `tests/fixtures/repo` itself -- see
`test_zzz_git_status_fixtures_tree_is_untouched` below, which checks that
directly.

Python 3.12 stdlib only.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent          # Tools/libgraph/tests
_LIBGRAPH_DIR = _HERE.parent                      # Tools/libgraph
_REPO_ROOT = _HERE.parents[2]                     # repo root
_SCRIPT = _LIBGRAPH_DIR / "libgraph.py"
_FIXTURE_REPO = _HERE / "fixtures" / "repo"
_DUMPBIN_DIR = _HERE / "fixtures" / "dumpbin"

sys.path.insert(0, str(_HERE))  # tests/ -> import contract

import contract  # noqa: E402

_JSON_BLOB_RE = re.compile(
    r'<script type="application/json" id="libgraph-data">(.*?)</script>',
    re.DOTALL,
)


def _edge(doc: dict, src: str, dst: str) -> dict | None:
    for e in doc["edges"]:
        if e["src"] == src and e["dst"] == dst:
            return e
    return None


class LibgraphE2ETests(unittest.TestCase):
    """Runs `python Tools/libgraph/libgraph.py ...` as a real subprocess
    against the shared fixture repo. Each test method gets its own --out
    subdirectory under one class-level tempdir, so tests are independent of
    each other and nothing is ever written under tests/fixtures/repo."""

    @classmethod
    def setUpClass(cls) -> None:
        cls._tmp = tempfile.TemporaryDirectory()
        cls.tmp_path = Path(cls._tmp.name)

        # Snapshot git's view of tests/fixtures BEFORE any test runs, so the
        # final test can assert nothing changed -- not that the snapshot is
        # literally empty, since Tools/libgraph is itself still untracked
        # (`?? Tools/libgraph/`) in this working tree at the time this suite
        # was written, which collapses an entirely-untracked directory to
        # one "??" status line regardless of what's inside it. Comparing
        # before vs. after catches a real regression (a subprocess call
        # writing into the fixture tree) either way, and reduces to exactly
        # "empty after the suite" once Tools/libgraph is committed and the
        # before-snapshot is genuinely clean.
        cls._git_available = True
        cls._fixtures_status_before: str | None = None
        try:
            before = subprocess.run(
                ["git", "status", "--porcelain", "--", "Tools/libgraph/tests/fixtures"],
                cwd=str(_REPO_ROOT), capture_output=True, encoding="utf-8", errors="replace",
            )
        except OSError:
            cls._git_available = False
        else:
            if before.returncode != 0:
                cls._git_available = False
            else:
                cls._fixtures_status_before = before.stdout

    @classmethod
    def tearDownClass(cls) -> None:
        cls._tmp.cleanup()

    def _run(
        self,
        extra_args: list[str],
        out_dir: Path,
        repo_root: Path | None = None,
    ) -> subprocess.CompletedProcess:
        """Runs libgraph.py as a subprocess. Always passes --repo-root,
        --dumpbin-dir and --out explicitly (binding amendment 10) -- callers
        only ever add config/baseline/--no-symbols/etc via extra_args."""
        cmd = [
            sys.executable, str(_SCRIPT),
            "--repo-root", str(repo_root if repo_root is not None else _FIXTURE_REPO),
            "--dumpbin-dir", str(_DUMPBIN_DIR),
            "--out", str(out_dir),
            *extra_args,
        ]
        return subprocess.run(cmd, capture_output=True, encoding="utf-8", errors="replace")

    # -- 1: exit 0, both files exist, contract.validate() == [] -------------

    def test_full_run_exit0_writes_a_conforming_report(self) -> None:
        out = self.tmp_path / "full_run"
        res = self._run(["--config", "fx_debug", "--quiet"], out)
        self.assertEqual(res.returncode, 0, msg=res.stderr)

        json_path = out / "libgraph.json"
        html_path = out / "index.html"
        self.assertTrue(json_path.is_file(), f"{json_path} was not written")
        self.assertTrue(html_path.is_file(), f"{html_path} was not written")

        doc = json.loads(json_path.read_text(encoding="utf-8"))
        errors = contract.validate(doc)
        self.assertEqual(errors, [], errors)

    # -- 2: the HTML embeds the same JSON, parsed back from the script tag --

    def test_html_embeds_the_same_json_as_the_json_file(self) -> None:
        out = self.tmp_path / "html_embed"
        res = self._run(["--config", "fx_debug", "--quiet"], out)
        self.assertEqual(res.returncode, 0, msg=res.stderr)

        json_doc = json.loads((out / "libgraph.json").read_text(encoding="utf-8"))
        html = (out / "index.html").read_text(encoding="utf-8")

        m = _JSON_BLOB_RE.search(html)
        self.assertIsNotNone(m, "libgraph-data script tag not found in rendered HTML")
        embedded_doc = json.loads(m.group(1))

        self.assertEqual(embedded_doc, json_doc)

    # -- 3: a few key end-to-end classifications ----------------------------

    def test_key_edge_classifications_survive_end_to_end(self) -> None:
        out = self.tmp_path / "classifications"
        res = self._run(["--config", "fx_debug", "--quiet"], out)
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        doc = json.loads((out / "libgraph.json").read_text(encoding="utf-8"))

        e = _edge(doc, "LibB", "LibC")
        self.assertIsNotNone(e, "no LibB -> LibC edge in the report")
        self.assertEqual(e["classes"][0], "used-undeclared")
        self.assertIn("allowlisted", e["classes"])
        self.assertIn("in-cycle", e["classes"])

        e = _edge(doc, "MidLib", "LibA")
        self.assertIsNotNone(e, "no MidLib -> LibA edge in the report")
        self.assertEqual(e["classes"][0], "pch")

        e = _edge(doc, "MidLib", "LibB")
        self.assertIsNotNone(e, "no MidLib -> LibB edge in the report")
        self.assertEqual(e["classes"][0], "link-only")

        e = _edge(doc, "Zenith", "LibC")
        self.assertIsNotNone(e, "no Zenith -> LibC edge in the report")
        self.assertEqual(e["classes"][0], "link-only")

        e = _edge(doc, "LibA", "LibB")
        self.assertIsNotNone(e, "no LibA -> LibB edge in the report")
        self.assertEqual(e["classes"][0], "test-only")

    # -- 4: --no-symbols ------------------------------------------------------

    def test_no_symbols_gives_exit0_meta_symbols_false_and_validates(self) -> None:
        out = self.tmp_path / "no_symbols"
        res = self._run(["--config", "fx_debug", "--no-symbols", "--quiet"], out)
        self.assertEqual(res.returncode, 0, msg=res.stderr)

        doc = json.loads((out / "libgraph.json").read_text(encoding="utf-8"))
        self.assertIs(doc["meta"]["symbols"], False)

        errors = contract.validate(doc)
        self.assertEqual(errors, [], errors)

    # -- 5: fx_release (FunctionLevelLinking=true) refusal -------------------

    def test_fx_release_function_level_linking_gives_exit2(self) -> None:
        out = self.tmp_path / "release"
        res = self._run(["--config", "fx_release", "--quiet"], out)
        self.assertEqual(res.returncode, 2, msg=res.stderr)
        self.assertIn("FunctionLevelLinking", res.stderr)
        self.assertIn("--no-symbols", res.stderr)

    # -- 6: --baseline against itself gives zero deltas -----------------------

    def test_baseline_against_itself_gives_zero_deltas(self) -> None:
        out1 = self.tmp_path / "baseline_first"
        res1 = self._run(["--config", "fx_debug", "--quiet"], out1)
        self.assertEqual(res1.returncode, 0, msg=res1.stderr)
        baseline_json = out1 / "libgraph.json"
        self.assertTrue(baseline_json.is_file())

        out2 = self.tmp_path / "baseline_second"
        res2 = self._run(
            ["--config", "fx_debug", "--baseline", str(baseline_json), "--quiet"], out2
        )
        self.assertEqual(res2.returncode, 0, msg=res2.stderr)
        doc2 = json.loads((out2 / "libgraph.json").read_text(encoding="utf-8"))

        self.assertIsNotNone(doc2["delta"])
        for lib_name, fields in doc2["delta"].items():
            for field, value in fields.items():
                self.assertEqual(value, 0, f"delta['{lib_name}']['{field}'] = {value}, expected 0")
        self.assertEqual(doc2["meta"]["delta_added"], [])
        self.assertEqual(doc2["meta"]["delta_removed"], [])

    # -- 7: a garbage --baseline file gives exit 3 -----------------------------

    def test_garbage_baseline_gives_exit3(self) -> None:
        out = self.tmp_path / "garbage_baseline"
        garbage = self.tmp_path / "garbage_baseline.json"
        garbage.write_text("this is not json at all {{{", encoding="utf-8")

        res = self._run(
            ["--config", "fx_debug", "--baseline", str(garbage), "--quiet"], out
        )
        self.assertEqual(res.returncode, 3, msg=res.stderr)
        self.assertTrue(res.stderr.strip(), "expected a one-line error on stderr")

    # -- 8: a nonexistent --repo-root gives exit 1 -----------------------------

    def test_nonexistent_repo_root_gives_exit1(self) -> None:
        out = self.tmp_path / "bad_repo_root"
        bogus = self.tmp_path / "this_directory_does_not_exist"
        res = self._run([], out, repo_root=bogus)
        self.assertEqual(res.returncode, 1, msg=res.stderr)
        self.assertIn("repo root not found", res.stderr)
        self.assertIn(bogus.name, res.stderr)

    # -- fixtures/ left untouched (binding amendment 10) -----------------------

    def test_zzz_git_status_fixtures_tree_is_untouched(self) -> None:
        """Named to sort last (unittest's default loader runs a class's test
        methods in alphabetical order) so it observes the tree state after
        every other test in this file has run. See setUpClass for why this
        compares against a BEFORE snapshot rather than asserting the status
        is literally empty."""
        if not self._git_available:
            self.skipTest("git is not available")

        after = subprocess.run(
            ["git", "status", "--porcelain", "--", "Tools/libgraph/tests/fixtures"],
            cwd=str(_REPO_ROOT), capture_output=True, encoding="utf-8", errors="replace",
        )
        if after.returncode != 0:
            self.skipTest(f"git status failed: {after.stderr}")

        self.assertEqual(
            after.stdout, self._fixtures_status_before,
            "tests/fixtures/repo changed during the e2e suite -- some invocation "
            "above must have written outside its --out tempdir:\n" + after.stdout,
        )


if __name__ == "__main__":
    unittest.main()
