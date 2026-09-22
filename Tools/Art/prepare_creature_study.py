"""Present original engine captures of a named creature candidate."""
from pathlib import Path
from PIL import Image, ImageDraw
import argparse, html

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=Path, default=ROOT / 'Build/artifacts/authored_assets/creature_work/FernfawnReference')
parser.add_argument('--extended', action='store_true', help='Include intermediate Faint poses and shiny Idle')
parser.add_argument('--name', default='Fernfawn', help='Exact creature source basename')
args = parser.parse_args()
if not args.name.isalnum(): parser.error('Creature name must be alphanumeric')
NAME = args.name
OUT = args.directory.resolve()
POSES = ['Idle', 'Walk', 'Attack', 'Special', 'Hit', 'Faint']
if args.extended:
    POSES += ['Faint_033', 'Faint_067', 'Shiny_Idle']
sheet = Image.new('RGB', (1566, 75 + ((len(POSES)+2)//3)*320), '#151a1d')
draw = ImageDraw.Draw(sheet)
draw.text((16, 12), NAME.upper()+' DEVELOPMENT STUDY - actual Zenith engine review-world captures', fill='white')
draw.text((16, 34), 'Development candidate. Photorealistic visual acceptance remains outstanding.', fill='#c1c7cb')
cards = []
for i, pose in enumerate(POSES):
    source = OUT / f'{NAME}_engine_{pose}.tga'
    if not source.exists():
        raise FileNotFoundError(source)
    im = Image.open(source).convert('RGB')
    im.save(source.with_name(source.stem + '_full.png'))
    if im.size != (1280, 720):
        raise ValueError(f'Unreviewed editor layout: {im.size}')
    viewport = im.crop((232, 93, 1015, 517))
    viewport.save(source.with_suffix('.png'))
    small = viewport.resize((510, 276), Image.Resampling.LANCZOS)
    x = 12 + (i % 3)*522; y = 75 + (i // 3)*320
    label = pose.replace('_033', ' at 33%').replace('_067', ' at 67%').replace('_', ' ')
    sheet.paste(small, (x, y)); draw.text((x+8, y+284), label, fill='white')
    cards.append(f'<figure><img src="{source.stem}.png"><figcaption>{html.escape(label)}</figcaption></figure>')
sheet.save(OUT / 'engine_review_contact.jpg', quality=94)
(OUT / 'engine_review.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<title>Creature development review</title><style>body{margin:32px;background:#151a1d;color:#e9eef2;font:16px system-ui}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}figure{margin:0}img{width:100%;border-radius:6px}figcaption{padding:10px}</style>
<h1>'''+html.escape(NAME)+''' development study</h1><p>Actual Zenith engine review-world captures. This creature remains a development candidate and is not accepted as photorealistic. Full, uncropped PNGs and original TGAs are retained beside these viewport crops.</p><main>'''+''.join(cards)+'</main></html>', encoding='utf-8')
print(f'{len(cards)} engine captures prepared under {OUT}')
