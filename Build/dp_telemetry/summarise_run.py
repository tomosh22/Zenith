import json, collections, sys
from pathlib import Path

p = Path(sys.argv[1] if len(sys.argv) > 1
         else r'C:\Users\tomos\AppData\Local\Temp\dp_personality_Trickster_playthrough.json')
d = json.loads(p.read_text())

events = d.get('events', [])
frames = d.get('frames', [])
print(f'File   : {p}')
print(f'Frames : {len(frames):,}')
print(f'Events : {len(events):,}')
print()
ec = collections.Counter(e.get('name', '?') for e in events)
print('Event counts (descending):')
for n, c in ec.most_common():
    print(f'  {c:4d}  {n}')
print()

# Headlines
def of(name): return [e for e in events if e.get('name') == name]
vict = of('Victory'); runl = of('RunLost')
poss = of('PossessionChanged'); objp = of('ObjectivePlaced')
died = of('VillagerDied'); appS = of('ApprehendChannelStart')
appI = of('ApprehendChannelInterrupted'); appD = of('ApprehendChannelComplete')
pcb = of('PerceptionContactBegin'); pce = of('PerceptionContactEnd')
fpk = of('ForgeCrafted'); doo = of('DoorOpened')
inter = of('Interact'); pickup = of('ItemPickup')

print(f'Victory     : {len(vict)} at fr={[e["frame"] for e in vict]} t={[round(e["t"],1) for e in vict]}s')
print(f'RunLost     : {len(runl)}')
print(f'Possessions : {len(poss)}')
print(f'Objectives  : {len(objp)} bit_indices={[e["payload"]["ints"][0] for e in objp]}')
print(f'Deaths      : {len(died)}')
print(f'Apprehend   : starts={len(appS)} completes={len(appD)} interrupts={len(appI)}')
print(f'Perception  : begin={len(pcb)} end={len(pce)}')
print(f'Forge       : {len(fpk)}  Door: {len(doo)}  Interact: {len(inter)}  Pickup: {len(pickup)}')
print()

print('First 25 events (timeline):')
for e in events[:25]:
    extra = ''
    if e.get('name') == 'ObjectivePlaced':
        extra = f' bit={e["payload"]["ints"][0]}'
    if e.get('name') == 'PossessionChanged':
        old = e['payload']['entityA']; new = e['payload']['entityB']
        extra = f' old=({old["idx"]},{old["gen"]}) new=({new["idx"]},{new["gen"]})'
    print(f'  t={e["t"]:7.2f}s  fr={e["frame"]:5d}  {e["name"]}{extra}')

print()
print('Last 15 events:')
for e in events[-15:]:
    extra = ''
    if e.get('name') == 'ObjectivePlaced':
        extra = f' bit={e["payload"]["ints"][0]}'
    print(f'  t={e["t"]:7.2f}s  fr={e["frame"]:5d}  {e["name"]}{extra}')

# Per-frame sampled stats
if frames:
    f0 = frames[0]
    f_last = frames[-1]
    print()
    print(f'Sampled frames: {len(frames):,}  (every {frames[1]["frame"] - frames[0]["frame"]} engine frames)')
    print(f'  first: frame={f0["frame"]} t={f0["t"]:.2f}s')
    print(f'  last : frame={f_last["frame"]} t={f_last["t"]:.2f}s')

    # held-item time
    held_counter = collections.Counter()
    poss_frames = 0
    priest_intent = collections.Counter()
    for fr in frames:
        ents = fr.get('entities', [])
        for ent in ents:
            tag = ent.get('uHeldItemTag')
            if ent.get('uHeldItemTag') is not None and ent.get('bIsPossessed', False):
                held_counter[tag] += 1
                poss_frames += 1
            intent = ent.get('aiIntent')
            if intent is not None:
                priest_intent[intent] += 1
    print()
    if poss_frames:
        print('Held-item distribution (possessed frames):')
        TAG = {0:'None',1:'Iron',2:'Key',3:'Obj1',4:'Obj2',5:'Obj3',6:'Obj4',7:'Obj5',8:'SkelKey',9:'Spike'}
        for tag, n in held_counter.most_common():
            print(f'  {n:5d} ({100*n/poss_frames:5.1f}%)  {TAG.get(tag, str(tag))}')
    if priest_intent:
        print()
        print('Priest BT-intent distribution:')
        INTENT = {0:'Idle',1:'Patrol',2:'Investigate',3:'Pursue',4:'Apprehend'}
        total = sum(priest_intent.values())
        for intent, n in priest_intent.most_common():
            print(f'  {n:5d} ({100*n/total:5.1f}%)  {INTENT.get(intent, str(intent))}')
