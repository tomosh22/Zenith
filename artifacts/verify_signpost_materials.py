from pathlib import Path
import struct, json, hashlib, io
from PIL import Image

root = Path('C:/dev/Zenith')
path = root/'Games/Zenithmon/Assets/Props/SignPost/SignPost.glb'
data = path.read_bytes()
magic, version, size = struct.unpack_from('<III', data)
assert magic == 0x46546c67 and version == 2 and size == len(data)
json_size = struct.unpack_from('<I', data, 12)[0]
doc = json.loads(data[20:20+json_size])
binary = data[28+json_size:]
assert len(doc['materials']) == 2 and len(doc['meshes']) == 1
primitives = doc['meshes'][0]['primitives']
assert len(primitives) == 2 and {p['material'] for p in primitives} == {0, 1}
materials = []
for mat in doc['materials']:
    pbr = mat['pbrMetallicRoughness']
    maps = {}
    for label, tex in [('baseColor', pbr['baseColorTexture']), ('normal', mat['normalTexture']), ('metallicRoughness', pbr['metallicRoughnessTexture'])]:
        image = doc['images'][doc['textures'][tex['index']]['source']]
        assert image['mimeType'] == 'image/png' and 'uri' not in image
        view = doc['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        im = Image.open(io.BytesIO(binary[start:start+view['byteLength']]))
        assert im.size == (1024, 1024)
        maps[label] = {'size': list(im.size), 'embedded': True}
    materials.append({'name': mat['name'], 'maps': maps})
report = {'sha256': hashlib.sha256(data).hexdigest(), 'meshCount': 1,
          'primitiveCount': 2, 'materialCount': 2, 'materials': materials,
          'primitives': [{'material': p['material'], 'indexCount': doc['accessors'][p['indices']]['count']} for p in primitives]}
out = root/'artifacts/signpost-multimaterial'
out.mkdir(exist_ok=True)
(out/'asset-verification.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
