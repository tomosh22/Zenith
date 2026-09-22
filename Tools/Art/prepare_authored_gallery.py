"""Publish actual engine captures as browsable local review sheets.

Full framebuffer TGAs remain the evidence. Crops remove only the known editor
panels; no scene content is generated, repainted, or composited into screenshots.
"""
from pathlib import Path
from PIL import Image,ImageDraw
import json,html,shutil

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Build/artifacts/authored_assets'
VISUAL=ROOT/'Build/artifacts/zenithmon/visual_audit'
groups={}
def add(group,path,label):
    im=Image.open(path).convert('RGB')
    destination=OUT/'gallery'/group
    destination.mkdir(parents=True,exist_ok=True)
    png=destination/(path.stem+'.png')
    if im.size==(1280,720):im=im.crop((232,93,1015,517))
    im.save(png)
    groups.setdefault(group,[]).append((label,png,im))

for path in sorted(VISUAL.glob('prop_*_three_quarter.tga')):
    add('World props',path,path.stem.removeprefix('prop_').removesuffix('_three_quarter').replace('_',' ').title())
for path in VISUAL.glob('*room_wide*.tga'):
    add('Shipped world',path,path.stem.removeprefix('prop_room_wide_'))
for path in sorted((OUT/'screenshots').glob('Dressing*.tga')):add('Battle biomes',path,path.stem)
for path in sorted((VISUAL/'grounditem_props').glob('*.tga')):add('Route 1 pickups',path,path.stem.replace('_',' '))
for path in sorted((OUT/'architecture').glob('*.tga')):add('Architecture review world',path,path.stem)
for path in sorted((OUT/'cast').glob('*.tga')):add('Cast review world',path,path.stem.replace('_',' '))
for path in sorted((OUT/'screenshots').glob('*_battle*.tga')):
    Image.open(path).convert('RGB').save(path.with_name(path.stem+'_full.png'))
    add('Creature gameplay',path,path.stem.replace('_',' '))
for family,label in [('StoneLine','Stone'),('FungalLine','Fungal'),('GroveDeer','Grove deer'),('Lancefish','Lancefish'),('MineralBeetles','Original mineral beetle study'),('MineralBeetlesScan','Scanned Prismite'),('Shoalfish','Shoalfish'),('Windseed','Windseed'),('EmberBipeds','Fire biped'),('LarkLine','Lark')]:
    for path in sorted((OUT/'creature_work'/family).glob('*/*_engine_*.tga')):
        Image.open(path).convert('RGB').save(path.with_name(path.stem+'_full.png'))
        add(label+' creature motion review',path,path.stem.replace('_engine_',' / ').replace('_',' '))

sections=[]
navigation=[]
for group,entries in groups.items():
    anchor=group.lower().replace(' ','-')
    navigation.append('<a href="#'+anchor+'">'+html.escape(group)+' ('+str(len(entries))+')</a>')
    sheets=[]
    for batch in range(0,len(entries),12):
        chunk=entries[batch:batch+12];sheet=Image.new('RGB',(1200,246*((len(chunk)+2)//3)),(25,29,32))
        draw=ImageDraw.Draw(sheet)
        for j,(label,path,im) in enumerate(chunk):
            thumb=im.copy();thumb.thumbnail((400,212));x=(j%3)*400;y=(j//3)*246
            sheet.paste(thumb,(x,y+29));draw.text((x+10,y+9),label,fill='white')
        target=OUT/'gallery'/group/f'contact_{batch//12+1}.jpg';sheet.save(target,quality=92);sheets.append(target)
    cards=''.join('<figure><a href="'+html.escape(str(path.relative_to(OUT)).replace('\\','/'))+'"><img loading="lazy" src="'+html.escape(str(path.relative_to(OUT)).replace('\\','/'))+'"></a><figcaption>'+html.escape(label)+'</figcaption></figure>' for label,path,im in entries)
    sections.append('<section id="'+anchor+'"><h2>'+html.escape(group)+'</h2><div class="grid">'+cards+'</div></section>')
page='''<!doctype html><meta charset="utf-8"><title>Zenithmon authored asset review</title>
<style>body{margin:32px auto;padding:0 24px;max-width:1600px;background:#15191d;color:#e7ebe9;font:16px system-ui}h1{font-size:30px}p{max-width:950px;line-height:1.6;color:#bcc9c3}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:18px}figure{margin:0;background:#22292c;border-radius:8px;overflow:hidden}img{width:100%;display:block}figcaption{padding:12px}h2{margin-top:45px;font-size:22px}a{color:#a2dac0}nav{display:flex;flex-wrap:wrap;gap:14px;margin:24px 0}input{padding:12px;width:min(90%,460px);background:#22292c;color:white;border:1px solid #64746b;border-radius:6px}section{scroll-margin-top:24px}[hidden]{display:none!important}</style>
<h1>Zenithmon — authored asset review</h1>
<p><strong>Incomplete first pass: __PRIMARY__ new primary candidates and __SHINY__ shiny variants; __MISSING__ creatures still lack GLB sources.</strong> The requested photorealistic finish has not been achieved across this set. All creature checkboxes remain under visual review.</p>
<p>These are actual Zenithmon framebuffer captures. Props, pickups and interiors show their gameplay scenes. Biome views isolate the arena's dressing components rather than showing full battles. Architecture and cast sheets use dedicated in-engine review worlds. Rejected creature studies are not presented as finished assets.</p>
<p><a href="inventory.md">Full source inventory</a> · <a href="review_status.md">Verification and outstanding work</a> · <a href="creature_work/FernfawnReference/engine_review.html">Fernfawn motion and shiny review</a></p>
<label>Find an asset <input type="search" placeholder="e.g. Aster, bridge, Gym" oninput="const q=this.value.toLowerCase();document.querySelectorAll('figure').forEach(f=>f.hidden=!f.textContent.toLowerCase().includes(q))"></label>
<nav>'''+''.join(navigation)+'</nav>'+''.join(sections)
inventory=json.loads((OUT/'inventory.json').read_text(encoding='utf-8'))
shiny=sum(Path(g['path']).stem.endswith('_shiny') for g in inventory['glbs'])
missing=sum(r['id'].startswith('AB-CRE-') and not r['sources'] for r in inventory['rows'])
# The initial inventory retained eight prop sources and the supplied Male model.
new_primary=len(inventory['glbs'])-shiny-9
page=page.replace('__PRIMARY__',str(new_primary)).replace('__SHINY__',str(shiny)).replace('__MISSING__',str(missing))
(OUT/'gallery.html').write_text(page,encoding='utf-8')
overview=Image.new('RGB',(1800,720),(25,29,32));draw=ImageDraw.Draw(overview)
examples=[
    ('World props/prop_townboard_three_quarter.png','Town board and home - Dawnmere'),
    ('World props/prop_bridge_stone_three_quarter.png','Stone crossing - Dawnmere'),
    ('Shipped world/prop_room_wide_PlayerHome.png','PlayerHome interior - gameplay scene'),
    ('Architecture review world/GymGrass.png','Grass gym - in-engine review world'),
    ('Cast review world/Aster_idle.png','Aster - in-engine review world'),
    ('World props/prop_rock_small_three_quarter.png','Fractured granite rock - Dawnmere'),
]
for i,(relative,label) in enumerate(examples):
    path=OUT/'gallery'/relative
    if not path.exists():continue
    im=Image.open(path);im.thumbnail((600,325));x=i%3*600;y=i//3*360
    draw.text((x+12,y+10),label,fill='white');overview.paste(im,(x,y+32))
overview.save(OUT/'review_overview.jpg',quality=93)
print(json.dumps({k:len(v) for k,v in groups.items()},indent=2))
