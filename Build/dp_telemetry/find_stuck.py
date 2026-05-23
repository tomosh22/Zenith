"""Find stretches where the priest or possessed villager was 'stuck'.

Stuck = small total displacement over a sliding window of samples.
"""
import json, math
from pathlib import Path
from collections import defaultdict

p = Path(r'C:\Users\tomos\AppData\Local\Temp\dp_personality_Trickster_playthrough.json')
d = json.loads(p.read_text())
frames = d['frames']

# flags=8 is villager (life>0 in first frame). The priest has aiIntent that
# transitions through Patrol/Investigate/Pursue/Apprehend. Find the priest by
# looking for the entity that ever has aiIntent != 0 over the whole run.
intent_ever = defaultdict(int)
for fr in frames:
    for e in fr['entities']:
        eid = (e['id']['idx'], e['id']['gen'])
        intent_ever[eid] = max(intent_ever[eid], e.get('aiIntent', 0))

priest_id = None
for eid, mx in intent_ever.items():
    if mx > 0:
        priest_id = eid
        break
print(f'Priest id: {priest_id}  (max-ever intent: {intent_ever[priest_id]})')

# Track every entity's position over time.
positions = defaultdict(list)  # eid -> [(t, x, z), ...]
for fr in frames:
    t = fr['t']
    for e in fr['entities']:
        eid = (e['id']['idx'], e['id']['gen'])
        positions[eid].append((t, e['pos'][0], e['pos'][2], e.get('aiIntent', 0)))

def dist2d(a, b):
    return math.hypot(a[1] - b[1], a[2] - b[2])

# Slide a 5-second window over the priest's positions and report stuck stretches.
def find_stuck(samples, label, window_s=5.0, max_disp=2.0):
    print()
    print(f'== {label}  ({len(samples)} samples) ==')
    stuck_windows = []
    i = 0
    while i < len(samples):
        j = i
        while j + 1 < len(samples) and samples[j+1][0] - samples[i][0] < window_s:
            j += 1
        if j > i:
            # Max-pairwise-distance within window i..j
            ps = samples[i:j+1]
            cx = sum(p[1] for p in ps) / len(ps)
            cz = sum(p[2] for p in ps) / len(ps)
            max_off = max(math.hypot(p[1]-cx, p[2]-cz) for p in ps)
            if max_off < max_disp:
                stuck_windows.append((samples[i][0], samples[j][0], cx, cz, max_off, j-i+1))
        i += 1

    # Merge overlapping windows.
    if not stuck_windows:
        print('  (no stuck windows found)')
        return
    merged = [stuck_windows[0]]
    for w in stuck_windows[1:]:
        last = merged[-1]
        if w[0] <= last[1] + 0.3 and math.hypot(w[2]-last[2], w[3]-last[3]) < max_disp:
            merged[-1] = (last[0], w[1], (last[2]+w[2])/2, (last[3]+w[3])/2,
                          max(last[4], w[4]), last[5] + w[5])
        else:
            merged.append(w)

    print(f'  Stuck stretches (>= {window_s}s within {max_disp}m radius):')
    print(f'  {"start":>8} {"end":>8} {"dur":>6}  pos                    radius  n_samples')
    for t0, t1, cx, cz, mxoff, n in merged:
        dur = t1 - t0
        if dur >= window_s:
            print(f'  {t0:8.2f} {t1:8.2f} {dur:6.1f}s ({cx:6.1f},{cz:6.1f})  {mxoff:5.2f}m  {n}')

# Build priest sample list.
priest_samples = positions[priest_id]
find_stuck(priest_samples, f'PRIEST id={priest_id}', window_s=5.0, max_disp=2.0)

# Possessed villager: each frame can have a different possessed villager.
# 'flags' bit 1 (=2) is "possessed" in DPTelemetry. Let me detect by
# scanning for samples where flags & 2 is set, then track that entity's
# position across the contiguous possessed run.
possessed_segments = []  # [(eid, [(t,x,z), ...])]
cur = None  # (eid, samples)
for fr in frames:
    poss_in_frame = None
    for e in fr['entities']:
        if e.get('flags', 0) & 2:
            poss_in_frame = e
            break
    if poss_in_frame is None:
        if cur is not None:
            possessed_segments.append(cur)
            cur = None
        continue
    eid = (poss_in_frame['id']['idx'], poss_in_frame['id']['gen'])
    if cur is None or cur[0] != eid:
        if cur is not None:
            possessed_segments.append(cur)
        cur = (eid, [])
    cur[1].append((fr['t'], poss_in_frame['pos'][0], poss_in_frame['pos'][2], 0))
if cur is not None:
    possessed_segments.append(cur)

print()
print(f'Possessed-villager segments: {len(possessed_segments)}')
for i, (eid, samples) in enumerate(possessed_segments[:5]):
    if samples:
        t0, t1 = samples[0][0], samples[-1][0]
        print(f'  #{i+1}  id={eid}  t={t0:6.2f}-{t1:6.2f}s ({len(samples)} samples)')

# Find stuck stretches across ALL possessed segments (concatenate by time).
all_poss_samples = []
for eid, samples in possessed_segments:
    all_poss_samples.extend(samples)
all_poss_samples.sort()
find_stuck(all_poss_samples, 'POSSESSED VILLAGER (any host)', window_s=3.0, max_disp=1.5)

# Total distance traveled by priest vs villager for sanity.
def total_dist(samples):
    return sum(dist2d(samples[i], samples[i+1]) for i in range(len(samples)-1))

priest_total = total_dist(priest_samples)
priest_dur = priest_samples[-1][0] - priest_samples[0][0]
print()
print(f'Priest total distance: {priest_total:.1f}m over {priest_dur:.0f}s')
print(f'  avg speed: {priest_total/priest_dur:.2f} m/s   '
      f'(walking ~1m/s, running ~3m/s)')

if all_poss_samples:
    v_total = total_dist(all_poss_samples)
    v_dur = all_poss_samples[-1][0] - all_poss_samples[0][0]
    print(f'Villager(possessed) total distance: {v_total:.1f}m over {v_dur:.0f}s')
    print(f'  avg speed: {v_total/v_dur:.2f} m/s')
