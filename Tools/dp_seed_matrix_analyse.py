"""dp_seed_matrix_analyse.py -- extract insights from a seed-matrix run.

Reads the per-(seed, personality) .telemetry.json files produced by
Tools/dp_seed_matrix_run.ps1 and emits a Markdown report covering:

  * Procgen layout summary per seed (wall / room / obstacle counts).
  * Per-(seed, personality) headline metrics.
  * Event-type breakdown grouped by personality.
  * New v3 telemetry insights:
      - Held-item time fraction
      - Life-timer at first death (per personality, per seed)
      - Priest BT-intent distribution
      - Perception contact counts
      - Apprehend channel lifecycle stats
      - Pause-toggle events
  * Cross-seed insights (variance across procgen layouts).

Usage:
    python Tools/dp_seed_matrix_analyse.py [--root Build/artifacts/telemetry/seed_matrix]
                                            [--out  Build/artifacts/telemetry/seed_matrix/REPORT.md]

ASCII-only. Standard library only (json + pathlib + argparse).
"""

import argparse
import json
import statistics
from collections import Counter, defaultdict
from pathlib import Path

PERSONALITIES = ["Casual", "Stealth", "Speedrunner", "Zealot",
                 "Magpie", "Relay", "Heretic", "Trickster"]

# DP_ItemTag enum -> name (DPTelemetry::EntitySnapshot.uHeldItemTag uses
# DP_ItemTag values; the canonical mapping is in DevilsPlayground_Tags.h).
ITEM_TAGS = {
    0: "None",
    1: "Iron",
    2: "Key",
    3: "Objective1",
    4: "Objective2",
    5: "Objective3",
    6: "Objective4",
    7: "Objective5",
    8: "SkeletonKey",
    9: "Spike",
}

# DPTelemetry::PriestIntent -> name.
PRIEST_INTENTS = {
    0: "None",
    1: "Idle",
    2: "Patrol",
    3: "Investigate",
    4: "Pursue",
    5: "Apprehend",
}

# DP_ApprehendInterruptReason -> name (PublicInterfaces.h).
APPREHEND_REASONS = {
    0: "Unknown",
    1: "TargetSwitched",
    2: "TargetLost",
    3: "OutOfRange",
    4: "PriestDespawned",
}

# StateFlags::IsPriest bit (DPTelemetry::StateFlags::IsPriest = 1u << 7).
FLAG_IS_PRIEST = 1 << 7


def load_run(path: Path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def analyse_run(tlm: dict) -> dict:
    """Compute per-run stats from a single telemetry JSON."""
    hdr = tlm.get("header", {})
    frames = tlm.get("frames", [])
    events = tlm.get("events", [])

    res = {
        "scene": hdr.get("sceneName", ""),
        "seed": hdr.get("seed", 0),
        "personality": hdr.get("personalityName", ""),
        "build": hdr.get("buildConfig", ""),
        "frames": len(frames),
        "events": len(events),
        "obstacleCount": len(hdr.get("obstacles", [])),
    }

    # Event-type histogram.
    type_counts = Counter()
    for e in events:
        name = e.get("name") or "type_%d" % e.get("type", -1)
        type_counts[name] += 1
    res["eventCounts"] = dict(type_counts)

    # Apprehend channel lifecycle pairing.
    n_start  = type_counts.get("ApprehendChannelStart", 0)
    n_done   = type_counts.get("ApprehendChannelComplete", 0)
    n_intr   = type_counts.get("ApprehendChannelInterrupted", 0)
    res["apprehendStart"]       = n_start
    res["apprehendComplete"]    = n_done
    res["apprehendInterrupted"] = n_intr

    # Interrupt reason breakdown.
    reason_counts = Counter()
    for e in events:
        if e.get("name") == "ApprehendChannelInterrupted":
            r = e.get("payload", {}).get("ints", [0,0,0,0])[0]
            reason_counts[APPREHEND_REASONS.get(r, str(r))] += 1
    res["interruptReasons"] = dict(reason_counts)

    # Perception contact counts.
    res["perceptionBegin"] = type_counts.get("PerceptionContactBegin", 0)
    res["perceptionEnd"]   = type_counts.get("PerceptionContactEnd", 0)

    # Pause toggles -- count + total paused frames.
    pause_events = sorted(
        [e for e in events if e.get("name") == "PauseToggle"],
        key=lambda e: e.get("frame", 0))
    paused_frames = 0
    pausing_at = None
    for pe in pause_events:
        pausing = bool(pe.get("payload", {}).get("ints", [0])[0])
        if pausing and pausing_at is None:
            pausing_at = pe.get("frame", 0)
        elif (not pausing) and pausing_at is not None:
            paused_frames += pe.get("frame", 0) - pausing_at
            pausing_at = None
    res["pauseToggleEvents"]  = len(pause_events)
    res["pausedFramesTotal"]  = paused_frames

    # Per-frame analyses.
    held_item_distribution    = Counter()
    held_item_frames_possessed = 0
    total_possessed_frames     = 0
    priest_intent_counts       = Counter()
    villager_life_at_death     = []
    last_alive_life_per_villager = {}  # entity_idx -> last positive life

    for frame in frames:
        for ent in frame.get("entities", []):
            flags = int(ent.get("flags", 0))
            is_priest = bool(flags & FLAG_IS_PRIEST)
            is_possessed = bool(flags & 1)  # StateFlags::Possessed = 1<<0
            is_alive     = bool(flags & 8)  # StateFlags::Alive     = 1<<3

            if is_priest:
                intent = int(ent.get("aiIntent", 0))
                priest_intent_counts[PRIEST_INTENTS.get(intent, str(intent))] += 1
                continue

            # Villager bookkeeping.
            ent_idx = ent.get("id", {}).get("idx", -1)
            life    = float(ent.get("life", 0.0))

            if is_possessed:
                total_possessed_frames += 1
                tag = int(ent.get("heldItem", 0))
                if tag != 0:
                    held_item_frames_possessed += 1
                    held_item_distribution[ITEM_TAGS.get(tag, str(tag))] += 1

            # Detect death: previous life > 0, this life == 0.
            prev = last_alive_life_per_villager.get(ent_idx, None)
            if prev is not None and prev > 0.5 and life < 0.01:
                villager_life_at_death.append(prev)
            last_alive_life_per_villager[ent_idx] = life

    res["priestIntentFrames"] = dict(priest_intent_counts)
    res["heldItemFractionWhilePossessed"] = (
        held_item_frames_possessed / total_possessed_frames
        if total_possessed_frames > 0 else 0.0)
    res["heldItemDistributionWhilePossessed"] = dict(held_item_distribution)
    res["possessedFrames"] = total_possessed_frames
    res["villagerDeathLives"] = villager_life_at_death

    # Frame perf (avg ms / sample).
    frame_ms = [float(f.get("frameMs", 0.0)) for f in frames if float(f.get("frameMs", 0.0)) > 0.0]
    if frame_ms:
        res["frameMsAvg"]    = statistics.mean(frame_ms)
        res["frameMsMedian"] = statistics.median(frame_ms)
        res["frameMsMax"]    = max(frame_ms)
    else:
        res["frameMsAvg"] = res["frameMsMedian"] = res["frameMsMax"] = 0.0

    return res


def summarise_event_breakdown(stats_by_personality_by_seed):
    """Aggregate per-personality event totals across all seeds."""
    out = {}
    for p in PERSONALITIES:
        all_evt = Counter()
        for seed_stats in stats_by_personality_by_seed.get(p, {}).values():
            for k, v in seed_stats["eventCounts"].items():
                all_evt[k] += v
        out[p] = dict(all_evt)
    return out


def render_md(stats_table, seeds, out_path: Path):
    lines = []
    lines.append("# Personality x Procgen-Seed insights")
    lines.append("")
    lines.append("Generated by `Tools/dp_seed_matrix_analyse.py`.")
    lines.append("")
    lines.append("Source data: `Tools/dp_seed_matrix_run.ps1` ran %d personalities x %d seeds = %d cells in `vs2022_Debug_Win64_False`."
                 % (len(PERSONALITIES), len(seeds), len(PERSONALITIES) * len(seeds)))
    lines.append("Seeds tested: " + ", ".join(str(s) for s in seeds))
    lines.append("")

    # ---- Procgen variation table (per-seed obstacle counts).
    lines.append("## Procgen layout variation by seed")
    lines.append("")
    lines.append("| Seed | Obstacle count (any personality) |")
    lines.append("|---:|---:|")
    for s in seeds:
        any_p = next(iter(stats_table[s].values()))
        lines.append("| %d | %d |" % (s, any_p["obstacleCount"]))
    lines.append("")

    # ---- Headline matrix.
    lines.append("## Headline matrix (frames / events / pause-frames / apprehend)")
    lines.append("")
    lines.append("| Seed | Personality | Frames | Events | Possessed frames | Held-item %% | Pause events | Apprehend start/done/intr |")
    lines.append("|---:|---|---:|---:|---:|---:|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None:
                lines.append("| %d | %s | -- | -- | -- | -- | -- | -- |" % (s, p))
                continue
            lines.append("| %d | %s | %d | %d | %d | %.1f%% | %d | %d/%d/%d |" % (
                s, p, r["frames"], r["events"], r["possessedFrames"],
                100.0 * r["heldItemFractionWhilePossessed"],
                r["pauseToggleEvents"],
                r["apprehendStart"], r["apprehendComplete"], r["apprehendInterrupted"],
            ))
    lines.append("")

    # ---- Priest intent distribution.
    lines.append("## Priest BT-intent distribution (priest frames only)")
    lines.append("")
    lines.append("Fraction of priest-sampled frames spent in each BT branch. Idle ~= no intent yet, Patrol ~= roaming, Investigate ~= chasing a noise, Pursue ~= moving toward the possessed villager, Apprehend ~= channeling the catch.")
    lines.append("")
    lines.append("| Seed | Personality | Idle | Patrol | Investigate | Pursue | Apprehend |")
    lines.append("|---:|---|---:|---:|---:|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None:
                continue
            d = r["priestIntentFrames"]
            total = sum(d.values()) or 1
            pct = lambda k: 100.0 * d.get(k, 0) / total
            lines.append("| %d | %s | %.0f%% | %.0f%% | %.0f%% | %.0f%% | %.0f%% |"
                % (s, p, pct("Idle"), pct("Patrol"), pct("Investigate"), pct("Pursue"), pct("Apprehend")))
    lines.append("")

    # ---- Perception contacts.
    lines.append("## Perception contacts (begin/end pairs)")
    lines.append("")
    lines.append("How often the priest's awareness of a target crossed 0.4 (rising = begin, falling = end). Higher counts = more cat-and-mouse contact.")
    lines.append("")
    lines.append("| Seed | Personality | Begin | End |")
    lines.append("|---:|---|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None: continue
            lines.append("| %d | %s | %d | %d |" % (s, p, r["perceptionBegin"], r["perceptionEnd"]))
    lines.append("")

    # ---- Apprehend reasons.
    lines.append("## Apprehend interrupt reasons")
    lines.append("")
    lines.append("When the priest's apprehend channel ended early, why?")
    lines.append("")
    lines.append("| Seed | Personality | TargetSwitched | TargetLost | OutOfRange |")
    lines.append("|---:|---|---:|---:|---:|")
    any_intr = False
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None: continue
            ir = r["interruptReasons"]
            if not ir: continue
            any_intr = True
            lines.append("| %d | %s | %d | %d | %d |" % (
                s, p,
                ir.get("TargetSwitched", 0),
                ir.get("TargetLost", 0),
                ir.get("OutOfRange", 0)))
    if not any_intr:
        lines.append("| -- | -- | 0 | 0 | 0 |")
        lines.append("")
        lines.append("(No apprehend interrupts fired in this matrix.)")
    lines.append("")

    # ---- Held item.
    lines.append("## Held-item distribution while possessed")
    lines.append("")
    lines.append("Fraction of possessed-frames the bot was carrying each tag. Adds up to <100%% when the bot was empty-handed for the rest.")
    lines.append("")
    lines.append("| Seed | Personality | Iron | Key | Objectives | SkeletonKey | Empty-handed |")
    lines.append("|---:|---|---:|---:|---:|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None: continue
            d   = r["heldItemDistributionWhilePossessed"]
            tot = r["possessedFrames"] or 1
            iron  = 100.0 * d.get("Iron", 0)  / tot
            key   = 100.0 * d.get("Key", 0)   / tot
            objs  = 100.0 * sum(d.get(f"Objective{i}", 0) for i in range(1,6)) / tot
            skel  = 100.0 * d.get("SkeletonKey", 0) / tot
            empty = max(0.0, 100.0 - (iron + key + objs + skel))
            lines.append("| %d | %s | %.1f%% | %.1f%% | %.1f%% | %.1f%% | %.1f%% |"
                % (s, p, iron, key, objs, skel, empty))
    lines.append("")

    # ---- Villager deaths.
    lines.append("## Villager deaths")
    lines.append("")
    lines.append("Two ways to count: the DP_OnVillagerDied event (canonical -- fires per death), and the per-frame life-timer rising/falling-edge detection (under-counts because 10 Hz sample period can miss deaths that happen + respawn within 100ms). First-death-life is only available for the per-frame method.")
    lines.append("")
    lines.append("| Seed | Personality | VillagerDied events | Per-frame deaths | First-death life (s) |")
    lines.append("|---:|---|---:|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None: continue
            event_deaths = r["eventCounts"].get("VillagerDied", 0)
            deaths = r["villagerDeathLives"]
            first = "%.2f" % deaths[0] if deaths else "--"
            lines.append("| %d | %s | %d | %d | %s |" % (s, p, event_deaths, len(deaths), first))
    lines.append("")

    # ---- Frame perf.
    lines.append("## Frame perf (wall-clock ms between samples)")
    lines.append("")
    lines.append("Time between consecutive 10 Hz samples in real wall-clock. Median is the better signal (mean is skewed by occasional GC / asset-load spikes).")
    lines.append("")
    lines.append("| Seed | Personality | Median ms | Avg ms | Max ms |")
    lines.append("|---:|---|---:|---:|---:|")
    for s in seeds:
        for p in PERSONALITIES:
            r = stats_table[s].get(p)
            if r is None: continue
            lines.append("| %d | %s | %.1f | %.1f | %.1f |"
                % (s, p, r["frameMsMedian"], r["frameMsAvg"], r["frameMsMax"]))
    lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="Build/artifacts/telemetry/seed_matrix")
    ap.add_argument("--out",  default="Build/artifacts/telemetry/seed_matrix/REPORT.md")
    args = ap.parse_args()

    root = Path(args.root)
    if not root.is_dir():
        print(f"Matrix root not found: {root}")
        return 1

    seed_dirs = sorted([d for d in root.iterdir() if d.is_dir() and d.name.startswith("seed_")],
                       key=lambda d: int(d.name.split("_")[1]))
    seeds = [int(d.name.split("_")[1]) for d in seed_dirs]

    # stats_table[seed][personality] -> result dict.
    stats_table = defaultdict(dict)
    for seed, sd in zip(seeds, seed_dirs):
        for p in PERSONALITIES:
            jp = sd / f"{p}.telemetry.json"
            if not jp.is_file():
                continue
            tlm = load_run(jp)
            stats_table[seed][p] = analyse_run(tlm)

    render_md(stats_table, seeds, Path(args.out))
    print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
