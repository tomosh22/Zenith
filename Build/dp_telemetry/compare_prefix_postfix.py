"""Compare priest engagement stats between the pre-fix and post-fix matrices."""
import json
import math
import glob
import statistics


def analyse_matrix(root):
    apprehend_starts = 0
    apprehend_complete = 0
    apprehend_interrupt = 0
    perception_begins = 0
    priest_dists = []

    paths = sorted(glob.glob(f'{root}/seed_*/*.telemetry.json'))
    for path in paths:
        try:
            d = json.load(open(path))
        except Exception:
            continue
        for e in d.get('events', []):
            n = e['name']
            if n == 'ApprehendChannelStart': apprehend_starts += 1
            elif n == 'ApprehendChannelComplete': apprehend_complete += 1
            elif n == 'ApprehendChannelInterrupted': apprehend_interrupt += 1
            elif n == 'PerceptionContactBegin': perception_begins += 1

        # find priest entity (whoever ever had aiIntent > 0)
        intent_seen = {}
        for fr in d.get('frames', []):
            for ent in fr.get('entities', []):
                if ent.get('aiIntent', 0) > 0:
                    key = (ent['id']['idx'], ent['id']['gen'])
                    intent_seen[key] = max(intent_seen.get(key, 0), ent['aiIntent'])
        if not intent_seen:
            continue
        pr_id = max(intent_seen, key=intent_seen.get)
        last_pos = None
        dist = 0.0
        for fr in d.get('frames', []):
            for ent in fr['entities']:
                if (ent['id']['idx'], ent['id']['gen']) == pr_id:
                    p = (ent['pos'][0], ent['pos'][2])
                    if last_pos is not None:
                        dist += math.hypot(p[0] - last_pos[0], p[1] - last_pos[1])
                    last_pos = p
                    break
        priest_dists.append(dist)

    return {
        'cells': len(paths),
        'starts': apprehend_starts,
        'complete': apprehend_complete,
        'interrupt': apprehend_interrupt,
        'perception': perception_begins,
        'priest_dists': priest_dists,
    }


def fmt(stats, label):
    pd = stats['priest_dists']
    print(f'== {label} ==')
    print(f'  cells: {stats["cells"]}')
    print(f'  apprehend starts:        {stats["starts"]}')
    print(f'  apprehend completes:     {stats["complete"]}  ({100 * stats["complete"] / max(1, stats["starts"]):.1f}% of starts)')
    print(f'  apprehend interrupts:    {stats["interrupt"]}')
    print(f'  perception contacts:     {stats["perception"]}')
    print(f'  priest distance/cell:    min={min(pd):.1f}m  median={statistics.median(pd):.1f}m  max={max(pd):.1f}m  mean={statistics.mean(pd):.1f}m')


pre = analyse_matrix('Build/dp_telemetry/seed_matrix')
post = analyse_matrix('Build/dp_telemetry/seed_matrix_postfix')
fmt(pre, 'PRE-FIX')
print()
fmt(post, 'POST-FIX (partial-path FAILURE)')
print()
print('=== Delta ===')
pd_pre, pd_post = pre['priest_dists'], post['priest_dists']
print(f'  apprehend starts:    {pre["starts"]:4d} -> {post["starts"]:4d}  ({post["starts"] - pre["starts"]:+d})')
print(f'  apprehend completes: {pre["complete"]:4d} -> {post["complete"]:4d}  ({post["complete"] - pre["complete"]:+d})')
print(f'  perception contacts: {pre["perception"]:4d} -> {post["perception"]:4d}  ({post["perception"] - pre["perception"]:+d})')
print(f'  priest median dist:  {statistics.median(pd_pre):6.1f}m -> {statistics.median(pd_post):6.1f}m  ({statistics.median(pd_post) - statistics.median(pd_pre):+.1f}m)')
print(f'  priest mean dist:    {statistics.mean(pd_pre):6.1f}m -> {statistics.mean(pd_post):6.1f}m  ({statistics.mean(pd_post) - statistics.mean(pd_pre):+.1f}m)')
