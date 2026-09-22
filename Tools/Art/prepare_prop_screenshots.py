"""Convert this run's engine framebuffer captures to PNG and a labelled sheet."""
from pathlib import Path
from PIL import Image, ImageDraw
import shutil

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'Build/artifacts/zenithmon/visual_audit'
OUT=ROOT/'Build/artifacts/authored_assets/screenshots'
OUT.mkdir(parents=True,exist_ok=True)
names=['lantern_west','lantern_east','townboard','fence_wood','fence_stone',
       'bridge_plank','bridge_stone','rock_small','rock_large','boulder','ledge_low','ledge_high']
tiles=[]
for name in names:
    path=SOURCE/('prop_'+name+'_three_quarter.tga')
    if not path.exists():
        continue
    im=Image.open(path).convert('RGB')
    im.save(OUT/(name+'_editor.png'))
    shutil.copy2(path,OUT/path.name)
    # Measured from this run's 1280x720 editor framebuffer. Preserve the full
    # original beside the viewport crop; do not apply this to another layout.
    if im.size == (1280,720):
        im=im.crop((232,93,1015,517))
    im.save(OUT/(name+'.png'))
    thumb=im.copy(); thumb.thumbnail((640,360))
    tile=Image.new('RGB',(640,392),(24,27,31)); tile.paste(thumb,(0,32))
    ImageDraw.Draw(tile).text((12,10),name.replace('_',' ').title(),fill='white')
    tiles.append(tile)
if tiles:
    sheet=Image.new('RGB',(1280,392*((len(tiles)+1)//2)),(24,27,31))
    for i,tile in enumerate(tiles): sheet.paste(tile,((i%2)*640,(i//2)*392))
    sheet.save(OUT/'contact_sheet.jpg',quality=92)
print('Prepared',len(tiles),'world screenshots in',OUT)
