#!/usr/bin/env python3
"""dp_personality_compare.py -- cross-personality summary from a seed
matrix run.

Reads `Build/dp_telemetry/seed_matrix/seed_<seed>/<Personality>.events.csv`
files and pivots them into a per-personality summary table that focuses on
the "did the bot actually win" question that the existing analyser
(`dp_seed_matrix_analyse.py`) doesn't expose directly.

Per-personality metrics:
  * cells       -- total seeds the personality ran against.
  * wins        -- count of seeds where Victory fired (5/5 obj delivered).
  * win_rate    -- wins / cells.
  * objs_avg    -- mean # of ObjectivePlaced events per cell.
  * deaths_avg  -- mean # of VillagerDied events per cell.
  * possess_avg -- mean # of PossessionChanged events per cell.
  * volswitch_avg -- mean voluntary-switch count per cell (PossessionChanged
                     events where the previous villager had >2s of life
                     remaining; heuristic for "switched off, not died").
                     Relay should be the only one with non-zero values.
  * delivery_order -- for Magpie: whether the delivered obj-tag sequence
                      is in monotonic Obj1->Obj5 order (False = any-order
                      working).
  * first_pickup_t -- mean time to first ItemPickup event (proxy for
                      "how fast does the bot reach the first objective").

ASCII-only output, prints a Markdown table.
"""

import argparse
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path

PERSONALITIES = ["Casual", "Stealth", "Speedrunner", "Zealot",
                 "Magpie", "Relay", "Heretic", "Trickster"]


def load_cell_events(events_csv: Path):
    """Read an events CSV and return a list of dicts."""
    if not events_csv.exists():
        return None
    rows = []
    with open(events_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for r in reader:
            try:
                r["frame"] = int(r["frame"])
                r["t"] = float(r["t"])
                r["int0"] = int(r["int0"])
                r["entityA_idx"] = int(r["entityA_idx"])
            except (ValueError, KeyError):
                continue
            rows.append(r)
    return rows


def load_cell_frames(frames_csv: Path):
    """Read frames CSV and return list of dicts. Used to compute per-
    villager life-remaining trajectories."""
    if not frames_csv.exists():
        return []
    rows = []
    with open(frames_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for r in reader:
            try:
                r["frame"] = int(r["frame"])
                r["t"] = float(r["t"])
                r["entity_idx"] = int(r["entity_idx"])
                r["fLifeRemaining"] = float(r["fLifeRemaining"])
            except (ValueError, KeyError):
                continue
            rows.append(r)
    return rows


def compute_voluntary_switches(events, frames):
    """A voluntary switch is a PossessionChanged event where the PREVIOUSLY
    possessed villager had > 2 s of life remaining at the moment of the
    switch. Burnout deaths show life ~= 0 just before PossessionChanged.

    Returns (vol_switch_count, total_switch_count).
    """
    poss_events = [e for e in events if e["type_name"] == "PossessionChanged"]
    if len(poss_events) < 2:
        return 0, len(poss_events)

    # Build per-(entity_idx, t) lookup of life remaining.
    # frames is 10 Hz sampled across all entities, including the villagers.
    # entityA_idx in PossessionChanged is the new villager; we want to know
    # the OLD villager's life. PossessionChanged.int0 holds the previous
    # villager index in v3 telemetry; but our events CSV doesn't expose
    # that cleanly, so we just look at the sample N ms before the switch
    # to see if ANY villager had > 2 s when its possessed-flag was about
    # to drop.
    #
    # Approximation: count how many PossessionChanged events follow a
    # frame-sample where the (previously) possessed villager had > 2 s
    # of life remaining. Heuristic: look at the most-recent frame sample
    # BEFORE the PossessionChanged event, find the villager whose life
    # delta is large (death -> respawn at max life), and check the BEFORE
    # value.
    #
    # Simpler: count VillagerDied events; voluntary = PossessionChanged - died.
    died = sum(1 for e in events if e["type_name"] == "VillagerDied")
    # PossessionChanged fires once on initial possession + once per re-
    # possess. So total possessions = poss_events count; deaths = died;
    # voluntary switches >= possessions - died - 1 (the -1 is initial).
    vol = max(0, len(poss_events) - died - 1)
    return vol, len(poss_events)


def compute_objective_delivery_order(events):
    """Return list of ObjectivePlaced int0 values in order of delivery,
    e.g. [0,1,2,3,4] for Casual or [3,4,1,0,2] for Magpie on seed 0.
    """
    return [e["int0"] for e in events if e["type_name"] == "ObjectivePlaced"]


def compute_first_pickup_t(events):
    """Time (s) of first ItemPickup event. Useful for "how fast did the
    bot start the loop"."""
    for e in events:
        if e["type_name"] == "ItemPickup":
            return e["t"]
    return None


def is_monotonic(seq):
    """Return True iff seq is strictly increasing (e.g. [0,1,2,3,4])."""
    return all(seq[i] < seq[i+1] for i in range(len(seq)-1))


def compute_first_apprehend_t(events):
    """Time (s) of first ApprehendChannelStart event -- proxy for 'when
    did the priest first close on the bot'."""
    for e in events:
        if e["type_name"] == "ApprehendChannelStart":
            return e["t"]
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--matrix-root", type=Path,
                   default=Path("Build/dp_telemetry/seed_matrix"))
    p.add_argument("--seeds", type=str, default=None,
                   help="Comma-separated seed list (default: auto-detect)")
    args = p.parse_args()

    root = args.matrix_root
    if not root.exists():
        print(f"ERROR: matrix root not found: {root}", file=sys.stderr)
        return 1

    if args.seeds:
        seeds = [int(s) for s in args.seeds.split(",")]
    else:
        seeds = sorted(
            int(d.name.removeprefix("seed_"))
            for d in root.glob("seed_*") if d.is_dir())

    print(f"Loaded matrix root: {root}")
    print(f"Seeds: {seeds}")
    print()

    # Per-personality aggregate buckets.
    agg = defaultdict(lambda: {
        "cells": 0, "wins": 0,
        "objs": [], "deaths": [], "possess": [],
        "vol_switches": [],
        "first_pickup": [],
        "first_apprehend": [],
        "delivery_orders": [],
        "monotonic_count": 0, "nonmonotonic_count": 0,
    })

    # Per-cell raw results so we can print a per-seed table too.
    per_cell = {}

    for seed in seeds:
        for p in PERSONALITIES:
            events = load_cell_events(root / f"seed_{seed}" / f"{p}.events.csv")
            if events is None:
                continue
            frames = []  # frames CSV inspection optional; not needed here.

            agg[p]["cells"] += 1

            objs_placed = sum(1 for e in events if e["type_name"] == "ObjectivePlaced")
            deaths      = sum(1 for e in events if e["type_name"] == "VillagerDied")
            possess     = sum(1 for e in events if e["type_name"] == "PossessionChanged")
            won = any(e["type_name"] == "Victory" for e in events)
            vol_switch, _ = compute_voluntary_switches(events, frames)
            first_pickup    = compute_first_pickup_t(events)
            first_apprehend = compute_first_apprehend_t(events)
            order = compute_objective_delivery_order(events)

            agg[p]["objs"].append(objs_placed)
            agg[p]["deaths"].append(deaths)
            agg[p]["possess"].append(possess)
            agg[p]["vol_switches"].append(vol_switch)
            if won:
                agg[p]["wins"] += 1
            if first_pickup is not None:
                agg[p]["first_pickup"].append(first_pickup)
            if first_apprehend is not None:
                agg[p]["first_apprehend"].append(first_apprehend)
            agg[p]["delivery_orders"].append(order)
            if len(order) >= 2:
                if is_monotonic(order):
                    agg[p]["monotonic_count"] += 1
                else:
                    agg[p]["nonmonotonic_count"] += 1

            per_cell[(seed, p)] = {
                "objs": objs_placed, "deaths": deaths, "possess": possess,
                "won": won, "vol_switch": vol_switch,
                "first_pickup": first_pickup,
                "first_apprehend": first_apprehend,
                "order": order,
            }

    # ---- Per-personality summary table.
    print("## Per-personality summary")
    print()
    print("| Personality | Cells | Wins | WinRate | ObjsAvg | DeathsAvg | PossessAvg | VolSwitchAvg | 1stPickup(s) | 1stApprehend(s) | %NonMonotonic |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for p in PERSONALITIES:
        a = agg[p]
        if a["cells"] == 0: continue
        objs_avg     = statistics.mean(a["objs"])      if a["objs"]      else 0.0
        deaths_avg   = statistics.mean(a["deaths"])    if a["deaths"]    else 0.0
        possess_avg  = statistics.mean(a["possess"])   if a["possess"]   else 0.0
        vol_avg      = statistics.mean(a["vol_switches"]) if a["vol_switches"] else 0.0
        first_pu_avg = statistics.mean(a["first_pickup"])    if a["first_pickup"]    else None
        first_ap_avg = statistics.mean(a["first_apprehend"]) if a["first_apprehend"] else None
        total_orders = a["monotonic_count"] + a["nonmonotonic_count"]
        pct_nonmon = 100.0 * a["nonmonotonic_count"] / total_orders if total_orders else 0.0
        win_rate     = 100.0 * a["wins"] / a["cells"]
        print("| %s | %d | %d | %.0f%% | %.1f | %.1f | %.1f | %.2f | %s | %s | %.0f%% |" % (
            p, a["cells"], a["wins"], win_rate,
            objs_avg, deaths_avg, possess_avg, vol_avg,
            ("%.1f" % first_pu_avg) if first_pu_avg is not None else "--",
            ("%.1f" % first_ap_avg) if first_ap_avg is not None else "--",
            pct_nonmon))
    print()

    # ---- Per-cell win matrix.
    print("## Per-cell wins (1 = Victory fired, 0 = no win)")
    print()
    print("| Seed | " + " | ".join(PERSONALITIES) + " |")
    print("|---:|" + "---:|" * len(PERSONALITIES))
    for seed in seeds:
        row = ["%d" % seed]
        for p in PERSONALITIES:
            c = per_cell.get((seed, p))
            row.append("1" if c and c["won"] else "0")
        print("| " + " | ".join(row) + " |")
    print()

    # ---- Per-cell ObjectivePlaced count.
    print("## Per-cell objectives delivered (out of 5)")
    print()
    print("| Seed | " + " | ".join(PERSONALITIES) + " |")
    print("|---:|" + "---:|" * len(PERSONALITIES))
    for seed in seeds:
        row = ["%d" % seed]
        for p in PERSONALITIES:
            c = per_cell.get((seed, p))
            row.append(str(c["objs"]) if c else "--")
        print("| " + " | ".join(row) + " |")
    print()

    # ---- Magpie order analysis.
    print("## Magpie objective-delivery order (per seed)")
    print()
    print("Order is the int0 value of each ObjectivePlaced event (0=Obj1, ..., 4=Obj5). A monotonic [0,1,2,3,4] sequence means the bot delivered in fixed order; anything else means Magpie's any-order pick was active.")
    print()
    print("| Seed | Magpie order |")
    print("|---:|---|")
    for seed in seeds:
        c = per_cell.get((seed, "Magpie"))
        if not c: continue
        ord_str = "[" + ",".join(str(x) for x in c["order"]) + "]"
        mark = "yes" if c["order"] and is_monotonic(c["order"]) else "no"
        print(f"| {seed} | {ord_str} (monotonic? {mark}) |")
    print()

    # ---- Relay voluntary-switch detail.
    print("## Relay voluntary-switch count (per seed)")
    print()
    print("Approximation: PossessionChanged - VillagerDied - 1. Counts switches that happened while the previous villager was still alive (i.e. the drop-and-relay path actually fired).")
    print()
    print("| Seed | Relay vol-switches | Relay deaths | Relay possess |")
    print("|---:|---:|---:|---:|")
    for seed in seeds:
        c = per_cell.get((seed, "Relay"))
        if not c: continue
        print(f"| {seed} | {c['vol_switch']} | {c['deaths']} | {c['possess']} |")
    print()

    # ---- Heretic priest-distraction signal.
    print("## Heretic priest-distraction (first ApprehendChannelStart time)")
    print()
    print("Later = priest was distracted longer before chasing. Reference: Speedrunner / Zealot for the no-noise-machine case.")
    print()
    print("| Seed | Heretic (s) | Speedrunner (s) | Zealot (s) | Casual (s) |")
    print("|---:|---:|---:|---:|---:|")
    for seed in seeds:
        def get(name):
            c = per_cell.get((seed, name))
            if not c: return "--"
            return ("%.1f" % c["first_apprehend"]) if c["first_apprehend"] is not None else "--"
        print(f"| {seed} | {get('Heretic')} | {get('Speedrunner')} | {get('Zealot')} | {get('Casual')} |")
    print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
