#!/usr/bin/env python3
"""
Memory-budget CI gate for the Zenith engine.

Mirrors the complexity ratchet: a committed baseline of per-category / per-source
memory budgets (Tools/memory_budget_baseline.json), and a checker that fails when a
budget is exceeded or regresses. Two modes:

  * RATCHET (default, no --csv): pure-JSON validation of the baseline itself — every
    budgeted entry must have budget_bytes >= baseline_peak_bytes (a budget below its
    own recorded peak is inconsistent). Zero external state; always runnable in CI.

  * LIVE (--csv <file>): compares actual peaks from a `--memory-dump` CSV against the
    baseline. For each ci_capturable entry: FAIL if actual > budget (budget>0) or
    actual > baseline_peak * (1 + tolerance_pct/100). Non-capturable entries (VRAM,
    RENDERER) are skipped unless --include-gpu (self-hosted GPU runner).

  --update <csv>: rewrite baseline_peak_bytes in the baseline from a CSV (like
  --update-architecture-allowlist). Shrinks or grows peaks to the measured values.

The CSV schema (emitted by Zenith --memory-dump) is:
    kind,name,bytes,count,peak_bytes,budget_bytes
where kind is one of {category, source, total}.
"""

import argparse
import csv
import json
import os
import sys

DEFAULT_BASELINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "memory_budget_baseline.json")


def evaluate_budget(actual_peak, budget_bytes, baseline_peak_bytes, tolerance_pct):
    """Pure decision function. Returns (passed: bool, reason: str).

    FAIL if the actual peak exceeds a set budget, or regresses beyond tolerance vs the
    recorded baseline peak. PASS otherwise. budget_bytes==0 means 'unbudgeted' (no hard
    cap); baseline_peak_bytes==0 means 'not yet measured' (no regression check)."""
    if budget_bytes and actual_peak > budget_bytes:
        return (False, "actual peak %d exceeds budget %d" % (actual_peak, budget_bytes))
    if baseline_peak_bytes:
        limit = baseline_peak_bytes * (1.0 + tolerance_pct / 100.0)
        if actual_peak > limit:
            return (False, "actual peak %d regressed beyond baseline %d (+%.0f%% -> limit %.0f)"
                    % (actual_peak, baseline_peak_bytes, tolerance_pct, limit))
    return (True, "ok")


def load_baseline(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def iter_entries(baseline):
    """Yield (section, name, entry_dict) over categories + sources."""
    for section in ("categories", "sources"):
        for name, entry in baseline.get(section, {}).items():
            yield section, name, entry


def read_csv_peaks(path):
    """Return {name: peak_bytes} from a --memory-dump CSV (categories + sources)."""
    peaks = {}
    with open(path, "r", encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row.get("kind") in ("category", "source"):
                # Fall back to the current `bytes` column when peak is absent OR zero.
                # NOTE: a bare `peak_bytes or bytes` is wrong — csv yields strings, and the
                # string "0" is TRUTHY in Python, so it would never fall through. At the
                # LITE tier the per-category peak column is emitted as 0 (no per-category
                # peak), so this fallback is exactly what makes the live gate meaningful.
                try:
                    peak = int(row.get("peak_bytes") or 0)
                    if peak == 0:
                        peak = int(row.get("bytes") or 0)
                    peaks[row["name"]] = peak
                except (ValueError, TypeError):
                    peaks[row["name"]] = 0
    return peaks


def run_ratchet(baseline):
    """Pure-JSON invariant: a budgeted entry's budget must not be below its own baseline peak."""
    failures = []
    for section, name, entry in iter_entries(baseline):
        budget = int(entry.get("budget_bytes", 0))
        peak = int(entry.get("baseline_peak_bytes", 0))
        if budget and peak > budget:
            failures.append("%s/%s: baseline peak %d exceeds its budget %d" % (section, name, peak, budget))
    return failures


def run_live(baseline, csv_path, include_gpu):
    tol = float(baseline.get("tolerance_pct", 10.0))
    peaks = read_csv_peaks(csv_path)
    failures = []
    checked = 0
    for section, name, entry in iter_entries(baseline):
        if not entry.get("ci_capturable", True) and not include_gpu:
            continue
        if name not in peaks:
            continue
        checked += 1
        passed, reason = evaluate_budget(
            peaks[name], int(entry.get("budget_bytes", 0)),
            int(entry.get("baseline_peak_bytes", 0)), tol)
        if not passed:
            failures.append("%s/%s: %s" % (section, name, reason))
    if checked == 0:
        failures.append("no capturable entries matched the CSV (empty/mismatched capture?)")
    return failures


def run_update(baseline, csv_path, baseline_path):
    peaks = read_csv_peaks(csv_path)
    updated = 0
    for section, name, entry in iter_entries(baseline):
        if name in peaks:
            entry["baseline_peak_bytes"] = peaks[name]
            updated += 1
    with open(baseline_path, "w", encoding="utf-8") as f:
        json.dump(baseline, f, indent=2)
        f.write("\n")
    print("Updated %d baseline peaks from %s" % (updated, csv_path))
    return []


def main(argv=None):
    ap = argparse.ArgumentParser(description="Zenith memory-budget CI gate")
    ap.add_argument("--baseline", default=DEFAULT_BASELINE)
    ap.add_argument("--csv", default=None, help="live --memory-dump CSV (LIVE mode)")
    ap.add_argument("--update", action="store_true", help="rewrite baseline peaks from --csv")
    ap.add_argument("--include-gpu", action="store_true", help="also enforce non-capturable (VRAM) entries")
    args = ap.parse_args(argv)

    baseline = load_baseline(args.baseline)

    if args.update:
        if not args.csv:
            print("ERROR: --update requires --csv", file=sys.stderr)
            return 2
        run_update(baseline, args.csv, args.baseline)
        return 0

    if args.csv:
        failures = run_live(baseline, args.csv, args.include_gpu)
        mode = "LIVE"
    else:
        failures = run_ratchet(baseline)
        mode = "RATCHET"

    if failures:
        print("=" * 70)
        print(" MEMORY BUDGET GATE FAILED (%s)" % mode)
        print("=" * 70)
        for f in failures:
            print("  - " + f)
        print("\nBudgets are ceilings: shrink them, don't grow them. If a regression is")
        print("justified, update Tools/memory_budget_baseline.json in the same commit.")
        return 1

    print("Memory budget gate passed (%s)." % mode)
    return 0


if __name__ == "__main__":
    sys.exit(main())
