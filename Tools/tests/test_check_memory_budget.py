#!/usr/bin/env python3
"""Unit tests for the memory-budget gate decision logic (Tools/check_memory_budget.py).

Pure/deterministic — no build, no live capture. Run: python Tools/tests/test_check_memory_budget.py
"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from check_memory_budget import evaluate_budget, run_ratchet, read_csv_peaks  # noqa: E402


class TestEvaluateBudget(unittest.TestCase):
    def test_under_budget_passes(self):
        ok, _ = evaluate_budget(actual_peak=100, budget_bytes=200, baseline_peak_bytes=0, tolerance_pct=10)
        self.assertTrue(ok)

    def test_exactly_at_budget_passes(self):
        ok, _ = evaluate_budget(actual_peak=200, budget_bytes=200, baseline_peak_bytes=0, tolerance_pct=10)
        self.assertTrue(ok)

    def test_one_over_budget_fails(self):
        ok, reason = evaluate_budget(actual_peak=201, budget_bytes=200, baseline_peak_bytes=0, tolerance_pct=10)
        self.assertFalse(ok)
        self.assertIn("exceeds budget", reason)

    def test_unbudgeted_never_fails_on_budget(self):
        ok, _ = evaluate_budget(actual_peak=10**12, budget_bytes=0, baseline_peak_bytes=0, tolerance_pct=10)
        self.assertTrue(ok)

    def test_within_tolerance_passes(self):
        # baseline 1000, tol 5% -> limit 1050; 1040 passes
        ok, _ = evaluate_budget(actual_peak=1040, budget_bytes=0, baseline_peak_bytes=1000, tolerance_pct=5)
        self.assertTrue(ok)

    def test_beyond_tolerance_fails(self):
        # baseline 1000, tol 5% -> limit 1050; 1060 fails
        ok, reason = evaluate_budget(actual_peak=1060, budget_bytes=0, baseline_peak_bytes=1000, tolerance_pct=5)
        self.assertFalse(ok)
        self.assertIn("regressed", reason)

    def test_zero_baseline_skips_regression_check(self):
        ok, _ = evaluate_budget(actual_peak=10**9, budget_bytes=0, baseline_peak_bytes=0, tolerance_pct=5)
        self.assertTrue(ok)

    def test_budget_takes_precedence_over_tolerance(self):
        # both a budget and a baseline; over budget -> fail regardless of tolerance
        ok, reason = evaluate_budget(actual_peak=300, budget_bytes=200, baseline_peak_bytes=290, tolerance_pct=50)
        self.assertFalse(ok)
        self.assertIn("exceeds budget", reason)


class TestRatchet(unittest.TestCase):
    def test_consistent_baseline_passes(self):
        baseline = {"categories": {"Renderer": {"budget_bytes": 200, "baseline_peak_bytes": 150}}, "sources": {}}
        self.assertEqual(run_ratchet(baseline), [])

    def test_peak_above_budget_fails(self):
        baseline = {"categories": {"Renderer": {"budget_bytes": 100, "baseline_peak_bytes": 150}}, "sources": {}}
        failures = run_ratchet(baseline)
        self.assertEqual(len(failures), 1)
        self.assertIn("exceeds its budget", failures[0])

    def test_unbudgeted_entry_ignored(self):
        baseline = {"categories": {"Temp": {"budget_bytes": 0, "baseline_peak_bytes": 10**9}}, "sources": {}}
        self.assertEqual(run_ratchet(baseline), [])

    def test_committed_baseline_is_self_consistent(self):
        # The real committed baseline must always pass the ratchet.
        import json
        p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "memory_budget_baseline.json")
        with open(p, "r", encoding="utf-8") as f:
            self.assertEqual(run_ratchet(json.load(f)), [])


class TestReadCsvPeaks(unittest.TestCase):
    def _write(self, rows):
        fd, path = tempfile.mkstemp(suffix=".csv")
        with os.fdopen(fd, "w", newline="") as f:
            f.write("kind,name,bytes,count,peak_bytes,budget_bytes\n")
            for r in rows:
                f.write(",".join(str(x) for x in r) + "\n")
        return path

    def test_zero_peak_falls_back_to_bytes(self):
        # LITE emits peak_bytes=0 with live bytes>0 -> the reader MUST fall back to bytes
        # (the "0"-is-truthy trap that once made the live gate a no-op on Release).
        path = self._write([("category", "Asset", 104857600, 50, 0, 1073741824)])
        try:
            self.assertEqual(read_csv_peaks(path)["Asset"], 104857600)
        finally:
            os.remove(path)

    def test_real_peak_used_when_present(self):
        path = self._write([("category", "Renderer", 100, 5, 999, 512)])
        try:
            self.assertEqual(read_csv_peaks(path)["Renderer"], 999)
        finally:
            os.remove(path)

    def test_source_read_total_ignored(self):
        path = self._write([("source", "Jolt", 200, 3, 200, 0), ("total", "All", 300, 8, 300, 0)])
        try:
            p = read_csv_peaks(path)
            self.assertEqual(p["Jolt"], 200)   # source rows are read
            self.assertNotIn("All", p)          # the total row is ignored
        finally:
            os.remove(path)

    def test_malformed_peak_does_not_crash(self):
        path = self._write([("category", "AI", "notanumber", 1, "alsobad", 0)])
        try:
            self.assertEqual(read_csv_peaks(path)["AI"], 0)   # graceful, not an exception
        finally:
            os.remove(path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
