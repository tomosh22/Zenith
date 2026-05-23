"""Find stuck stretches in a specific telemetry json file."""
import json, math, sys
from collections import defaultdict
from pathlib import Path

p = Path(sys.argv[1])
d = json.loads(p.read_text())
frames = d['frames']

positions = defaultdict(list)
intent_ever = defaultdict(int)
for fr in frames:
    for e in fr['entities']:
        eid = (e['id']['idx'], e['id']['gen'])
        intent_ever[eid] = max(intent_ever[eid], e.get('aiIntent', 0))
        positions[eid].append((fr['t'], e['pos'][0], e['pos'][2], e.get('flags', 0), e.get('aiIntent', 0)))

priest_id = None
for eid, mx in intent_ever.items():
    if mx > 0:
        priest_id = eid; break

def find_stuck(samples, label, window=5.0, max_disp=2.0):
    print(f'== {label}  ({len(samples)} samples) ==')
    stretches = []
    i = 0
    while i < len(samples):
        j = i
        while j+1 < len(samples) and samples[j+1][0] - samples[i][0] < window: j += 1
        if j > i:
            cx = sum(p[1] for p in samples[i:j+1]) / (j+1-i)
            cz = sum(p[2] for p in samples[i:j+1]) / (j+1-i)
            mx = max(math.hypot(p[1]-cx, p[2]-cz) for p in samples[i:j+1])
            if mx < max_disp:
                stretches.append((samples[i][0], samples[j][0], cx, cz, mx))
        i += 1
    if not stretches:
        print('  none'); return
    merged = [stretches[0]]
    for s in stretches[1:]:
        if s[0] <= merged[-1][1] + 0.3 and math.hypot(s[2]-merged[-1][2], s[3]-merged[-1][3]) < max_disp:
            merged[-1] = (merged[-1][0], s[1], (merged[-1][2]+s[2])/2, (merged[-1][3]+s[3])/2, max(merged[-1][4],s[4]))
        else: merged.append(s)
    for t0, t1, cx, cz, mx in merged:
        if t1-t0 >= window:
            print(f'  {t0:6.1f} - {t1:6.1f}s ({t1-t0:.1f}s)  pos=({cx:6.1f},{cz:6.1f})  radius={mx:.2f}m')

print(f'File: {p}')
find_stuck(positions[priest_id], f'PRIEST {priest_id}', 5.0, 2.0)

poss_samples = [s for s in sum(positions.values(), []) if s[3] & 2]
poss_samples.sort()
find_stuck(poss_samples, 'POSSESSED VILLAGER (any host)', 3.0, 1.5)
