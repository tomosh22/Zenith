#!/usr/bin/env python
"""Crop photo-tour TGA dumps to the editor viewport and build a contact sheet.

    python Tools/phototour_crop.py Build/artifacts/zenithmon/phototour/baseline
    python Tools/phototour_crop.py <dirA> --compare <dirB>   # side-by-side pairs

Every <name>.tga has a <name>.rect sidecar "x y w h" (the editor viewport in
swapchain pixels; all zeros / missing = the whole frame). Writes <name>.png
beside each TGA plus contact.png (and compare.png with --compare).
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw


def load_shot(tga_path):
    img = Image.open(tga_path).convert("RGB")
    rect_path = os.path.splitext(tga_path)[0] + ".rect"
    if os.path.exists(rect_path):
        with open(rect_path) as f:
            parts = f.read().split()
        if len(parts) == 4:
            x, y, w, h = (int(float(p)) for p in parts)
            if w > 16 and h > 16:
                x = max(0, x)
                y = max(0, y)
                w = min(w, img.width - x)
                h = min(h, img.height - y)
                img = img.crop((x, y, x + w, y + h))
    return img


def process_dir(d):
    shots = []
    for name in sorted(os.listdir(d)):
        if not name.lower().endswith(".tga"):
            continue
        path = os.path.join(d, name)
        try:
            img = load_shot(path)
        except Exception as e:  # noqa: BLE001
            print(f"  skip {name}: {e}")
            continue
        png = os.path.splitext(path)[0] + ".png"
        img.save(png)
        shots.append((os.path.splitext(name)[0], img))
        print(f"  {name} -> {os.path.basename(png)} {img.width}x{img.height}")
    return shots


def contact_sheet(shots, out_path, cell_w=640, cols=3, label=True):
    if not shots:
        return
    thumbs = []
    for name, img in shots:
        scale = cell_w / img.width
        t = img.resize((cell_w, max(1, int(img.height * scale))), Image.LANCZOS)
        thumbs.append((name, t))
    cell_h = max(t.height for _, t in thumbs) + (18 if label else 0)
    rows = (len(thumbs) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (16, 16, 16))
    draw = ImageDraw.Draw(sheet)
    for i, (name, t) in enumerate(thumbs):
        cx = (i % cols) * cell_w
        cy = (i // cols) * cell_h
        sheet.paste(t, (cx, cy + (18 if label else 0)))
        if label:
            draw.text((cx + 4, cy + 2), name, fill=(230, 230, 230))
    sheet.save(out_path)
    print(f"  wrote {out_path} ({sheet.width}x{sheet.height})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--compare", help="second directory; pairs shots by name, A left / B right")
    ap.add_argument("--cell", type=int, default=640)
    ap.add_argument("--allow-partial", action="store_true",
                    help="permit a comparison where some shots exist on only one side")
    args = ap.parse_args()

    print(args.dir)
    a = process_dir(args.dir)
    # A directory with nothing readable in it is a FAILED tour, not an empty one.
    # contact_sheet() returns quietly on an empty list, so without this the script
    # printed a path and exited 0 having produced nothing -- the downstream half of
    # counting a queued screenshot as a written artifact.
    if not a:
        print(f"  ERROR: no readable captures in {args.dir}", file=sys.stderr)
        return 1
    contact_sheet(a, os.path.join(args.dir, "contact.png"), cell_w=args.cell)
    if args.compare:
        print(args.compare)
        b = process_dir(args.compare)
        if not b:
            print(f"  ERROR: no readable captures in {args.compare}", file=sys.stderr)
            return 1
        bmap = dict(b)
        # A comparison is only as good as the shots that PAIRED. Zero pairs used to
        # be silent success -- contact_sheet() returns quietly on an empty list --
        # so a typo'd tag, a different game's directory or a renamed pose produced a
        # confident exit 0 and no compare.png. A partial pairing is the same failure
        # wearing a smaller hat: the sheet just gets shorter, and the shot you were
        # looking for is the one that is missing.
        only_a = sorted(n for n, _ in a if n not in bmap)
        amap = dict(a)
        only_b = sorted(n for n, _ in b if n not in amap)
        pairs = []
        for name, img in a:
            if name in bmap:
                other = bmap[name]
                h = min(img.height, other.height)
                left = img.resize((int(img.width * h / img.height), h), Image.LANCZOS)
                right = other.resize((int(other.width * h / other.height), h), Image.LANCZOS)
                pair = Image.new("RGB", (left.width + right.width + 8, h), (255, 255, 255))
                pair.paste(left, (0, 0))
                pair.paste(right, (left.width + 8, 0))
                pairs.append((name, pair))
                pair.save(os.path.join(args.compare, f"compare_{name}.png"))
        if only_a or only_b:
            if only_a:
                print(f"  {len(only_a)} shot(s) only in {args.dir}: {', '.join(only_a)}", file=sys.stderr)
            if only_b:
                print(f"  {len(only_b)} shot(s) only in {args.compare}: {', '.join(only_b)}", file=sys.stderr)
        if not pairs:
            print(f"  ERROR: no shot names are common to {args.dir} and {args.compare} "
                  f"-- nothing was compared", file=sys.stderr)
            return 1
        if (only_a or only_b) and not args.allow_partial:
            print(f"  ERROR: incomplete comparison -- {len(pairs)} paired, "
                  f"{len(only_a) + len(only_b)} unmatched (listed above). "
                  f"Pass --allow-partial if the asymmetry is deliberate "
                  f"(e.g. an 'ab' run against a plain one).", file=sys.stderr)
            return 1
        contact_sheet(pairs, os.path.join(args.compare, "compare.png"), cell_w=args.cell * 2, cols=1)
        print(f"  compared {len(pairs)} shot(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
