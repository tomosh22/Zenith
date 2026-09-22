"""Read-only reconciliation of 3D ArtBrief rows against local GLB sources."""
from pathlib import Path
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'Games/Zenithmon/Assets'
REPORT = ROOT / 'Build/artifacts/authored_assets'
REPORT.mkdir(parents=True, exist_ok=True)
text = (ROOT / 'Games/Zenithmon/Docs/ArtBrief.md').read_text(encoding='utf-8')
text = text[text.index('## 1. Props'):text.index('## 6. What has NO generator')]
rows = []
all_glbs = list(ASSETS.rglob('*.glb'))
player_source=ROOT/'Zenith/Assets/Meshes/Humans/Male/Male.glb'
if player_source.exists():all_glbs.append(player_source)
for checked, asset_id, name in re.findall(r'^- \[([ x])\] `(AB-(?:PROP|CRE|HUM|BLD|INT)-\d+)` \*\*(.*?)\*\*', text, re.M):
    matches = [p for p in all_glbs if p.stem == name.replace(' interior','')]
    if asset_id=='AB-HUM-01' and player_source.exists():matches=[player_source]
    # Interior and exterior PlayerHome sources are separate assets.
    if asset_id.startswith('AB-INT'):
        matches = [p for p in matches if 'Interiors' in p.parts]
    elif asset_id.startswith('AB-BLD'):
        matches = [p for p in matches if 'Buildings' in p.parts]
    row = {'id':asset_id,'name':name,'checked':checked=='x',
           'sources':[str(p.relative_to(ROOT)).replace('\\','/') for p in matches]}
    rows.append(row)

glbs=[]
for path in sorted(all_glbs):
    raw=path.read_bytes()
    magic,version,length=struct.unpack_from('<III',raw)
    assert magic==0x46546c67 and version==2 and length==len(raw),path
    n,kind=struct.unpack_from('<II',raw,12)
    assert kind==0x4e4f534a,path
    doc=json.loads(raw[20:20+n])
    materials=doc.get('materials',[])
    meshes=doc.get('meshes',[])
    triangles=0
    for mesh in meshes:
        for p in mesh['primitives']:
            assert p.get('mode',4)==4,path
            for attr in ['POSITION','NORMAL','TEXCOORD_0']:
                assert attr in p['attributes'],(path,attr)
            triangles+=doc['accessors'][p['indices']]['count']//3
    glbs.append({'path':str(path.relative_to(ROOT)).replace('\\','/'),
        'bytes':len(raw),'triangles':triangles,'materials':len(materials),
        'embedded_images':sum('bufferView' in i for i in doc.get('images',[])),
        'pbr_textures_complete':all('baseColorTexture' in m.get('pbrMetallicRoughness',{})
            and 'metallicRoughnessTexture' in m.get('pbrMetallicRoughness',{})
            and 'normalTexture' in m for m in materials),
        'alpha_modes':[m.get('alphaMode','OPAQUE') for m in materials]})
report={'rows':rows,'glbs':glbs}
(REPORT/'inventory.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
lines=['# Authored 3D asset reconciliation','',
       'Source presence is distinct from import, placement and visual acceptance.','',
       '| ID | Asset | Checkbox | GLB source |','|---|---|---|---|']
for row in rows:
    lines.append('| '+row['id']+' | '+row['name']+' | '+('checked' if row['checked'] else 'unchecked')+' | '+('; '.join(row['sources']) or 'MISSING')+' |')
(REPORT/'inventory.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
counts={}
for row in rows:
    family=row['id'].split('-')[1]
    c=counts.setdefault(family,{'total':0,'source_present':0,'missing':0})
    c['total']+=1
    c['source_present' if row['sources'] else 'missing']+=1
print(json.dumps(counts,indent=2))
print('GLBs:',len(glbs),'PBR complete:',sum(g['pbr_textures_complete'] for g in glbs))
