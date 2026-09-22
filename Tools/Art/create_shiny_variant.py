"""Make a strictly hue-only GLB variant without re-exporting geometry or skin.

A cyclic RGB channel permutation rotates HSV hue by exactly 120 degrees while
preserving saturation/value, alpha and every non-colour resource byte-for-byte.
The source must use embedded PNG base-colour textures and neutral RGB factors.
"""
from pathlib import Path
import argparse, copy, hashlib, io, json, struct
import numpy as np
from PIL import Image
from audit_creature_glb import Glb, require


def image_bytes(glb, index):
    im = glb.doc['images'][index]
    require(im.get('mimeType') == 'image/png' and 'bufferView' in im, 'Expected embedded PNG')
    view = glb.doc['bufferViews'][im['bufferView']]
    start = view.get('byteOffset', 0)
    return glb.binary[start:start + view['byteLength']]


def create(source, destination):
    require(source.resolve() != destination.resolve(), 'Do not overwrite the source')
    src = Glb(source); doc = copy.deepcopy(src.doc); binary = bytearray(src.binary)
    base_images = set(); other_images = set()
    for mat in doc['materials']:
        pbr = mat.get('pbrMetallicRoughness', {})
        require(pbr.get('baseColorFactor', [1,1,1,1])[:3] == [1,1,1], 'Non-neutral colour factor requires explicit authoring')
        require('extensions' not in mat, 'Material extension needs explicit colour-role handling')
        require('baseColorTexture' in pbr, 'Missing base-colour texture')
        base_images.add(doc['textures'][pbr['baseColorTexture']['index']]['source'])
        for slot in [pbr.get('metallicRoughnessTexture'), mat.get('normalTexture'), mat.get('occlusionTexture'), mat.get('emissiveTexture')]:
            if slot: other_images.add(doc['textures'][slot['index']]['source'])
    require(not base_images.intersection(other_images), 'A colour image is reused as non-colour data')
    for index in sorted(base_images):
        raw = np.array(Image.open(io.BytesIO(image_bytes(src, index))).convert('RGBA'))
        variant = raw[:, :, [2,0,1,3]]
        encoded = io.BytesIO(); Image.fromarray(variant, 'RGBA').save(encoded, format='PNG')
        data = encoded.getvalue(); binary.extend(b'\0' * (-len(binary) % 4))
        view = {'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(data)}
        doc['images'][index]['bufferView'] = len(doc['bufferViews']); doc['bufferViews'].append(view)
        binary.extend(data)
    doc['buffers'][0]['byteLength'] = len(binary)
    encoded_doc = json.dumps(doc, separators=(',', ':')).encode('utf-8'); encoded_doc += b' ' * (-len(encoded_doc) % 4)
    binary.extend(b'\0' * (-len(binary) % 4))
    output = struct.pack('<3I', 0x46546c67, 2, 28 + len(encoded_doc) + len(binary))
    output += struct.pack('<2I', len(encoded_doc), 0x4e4f534a) + encoded_doc
    output += struct.pack('<2I', len(binary), 0x004e4942) + binary
    temporary = destination.with_suffix('.glb.tmp'); temporary.write_bytes(output)
    try:
        result = Glb(temporary)
        for field in ['meshes', 'accessors', 'nodes', 'skins', 'animations', 'materials', 'textures', 'samplers']:
            require(src.doc.get(field) == result.doc.get(field), f'{field} changed')
        require(result.binary[:len(src.binary)] == src.binary, 'Original geometry/resource bytes changed')
        for index in other_images:
            require(image_bytes(src, index) == image_bytes(result, index), 'Non-colour resource changed')
        for index in base_images:
            a = np.array(Image.open(io.BytesIO(image_bytes(src,index))).convert('RGBA'))
            b = np.array(Image.open(io.BytesIO(image_bytes(result,index))).convert('RGBA'))
            require(np.array_equal(b, a[:, :, [2,0,1,3]]), 'Colour rotation was not exact')
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    report = {'source': str(source.resolve()), 'variant': str(destination.resolve()),
              'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'variant_sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
              'hue_rotation_degrees': 120, 'base_colour_images': len(base_images),
              'geometry_skin_and_relief_unchanged': True, 'saturation_value_alpha_unchanged': True,
              'visual_acceptance': False}
    destination.with_suffix('.audit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path); parser.add_argument('destination', type=Path)
    args = parser.parse_args(); print(json.dumps(create(args.source, args.destination), indent=2))
