"""Read-only acceptance checks for a GLB fitted to a shipped creature rig.

This checks geometry/skin/PBR contracts, not artistic quality. It deliberately
does not publish a runtime bundle or mark the ArtBrief checkbox complete.
Only self-contained, ordinary glTF 2.0 accessors are accepted by this authoring
check; compressed and sparse accessors need their own decoder, never a guess.
"""
from pathlib import Path
import argparse
import json
import struct
import numpy as np
import read_zenith_rig as wire


def require(ok, message):
    if not ok:
        raise ValueError(message)


class Glb:
    def __init__(self, path):
        raw = Path(path).read_bytes()
        require(len(raw) >= 20, 'Truncated GLB')
        require(struct.unpack_from('<3I', raw) == (0x46546c67, 2, len(raw)), 'Invalid GLB envelope')
        chunks = {}
        offset = 12
        while offset < len(raw):
            require(offset + 8 <= len(raw), 'Truncated chunk header')
            size, kind = struct.unpack_from('<2I', raw, offset)
            require(size % 4 == 0 and offset + 8 + size <= len(raw), 'Invalid chunk size')
            require(kind not in chunks, 'Duplicate GLB chunk')
            chunks[kind] = raw[offset + 8:offset + 8 + size]
            offset += size + 8
        require(0x4e4f534a in chunks and 0x004e4942 in chunks, 'Missing JSON or BIN chunk')
        self.doc = json.loads(chunks[0x4e4f534a])
        self.binary = chunks[0x004e4942]
        require(self.doc['asset']['version'] == '2.0', 'Unsupported glTF version')
        buffers = self.doc['buffers']
        require(len(buffers) == 1 and 'uri' not in buffers[0], 'External/multiple buffers unsupported')
        require(0 <= len(self.binary) - buffers[0]['byteLength'] <= 3, 'BIN length mismatch')
        self.parents = [-1] * len(self.doc['nodes'])
        for i, node in enumerate(self.doc['nodes']):
            for child in node.get('children', []):
                require(0 <= child < len(self.parents), 'Invalid child index')
                require(self.parents[child] == -1, 'Node has multiple parents')
                self.parents[child] = i
        self.worlds = {}
        for i in range(len(self.parents)):
            self.world(i)

    def accessor(self, index):
        a = self.doc['accessors'][index]
        require('sparse' not in a, 'Sparse accessor unsupported')
        v = self.doc['bufferViews'][a['bufferView']]
        require(v.get('buffer', 0) == 0 and not v.get('extensions'), 'Compressed/external buffer view unsupported')
        types = {5120: 'i1', 5121: 'u1', 5122: '<i2', 5123: '<u2', 5125: '<u4', 5126: '<f4'}
        widths = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
        require(a['componentType'] in types and a['type'] in widths, 'Unsupported accessor layout')
        dtype = np.dtype(types[a['componentType']]); width = widths[a['type']]
        count = a['count']; relative = a.get('byteOffset', 0)
        stride = v.get('byteStride', width * dtype.itemsize)
        require(count > 0 and relative >= 0 and stride >= width * dtype.itemsize, 'Invalid accessor count/stride')
        extent = relative + (count - 1) * stride + width * dtype.itemsize
        start = v.get('byteOffset', 0)
        require(start >= 0 and extent <= v['byteLength'] and start + v['byteLength'] <= len(self.binary), 'Accessor exceeds buffer view')
        out = np.ndarray((count, width), dtype=dtype, buffer=self.binary,
                         offset=start + relative, strides=(stride, dtype.itemsize)).copy()
        if a.get('normalized', False):
            require(dtype.kind in 'iu', 'Normalized float accessor')
            out = out.astype(np.float64) / np.iinfo(dtype).max
            if dtype.kind == 'i':
                out = np.maximum(out, -1)
        require(np.isfinite(out).all(), 'Non-finite accessor')
        return out

    def world(self, index, visiting=None):
        if index in self.worlds:
            return self.worlds[index]
        visiting = set() if visiting is None else visiting
        require(index not in visiting, 'Cyclic node hierarchy')
        visiting.add(index)
        node = self.doc['nodes'][index]
        if 'matrix' in node:
            require(not any(k in node for k in ('translation', 'rotation', 'scale')), 'Node mixes matrix and TRS')
            local = np.array(node['matrix']).reshape(4, 4).T
        else:
            x, y, z, w = node.get('rotation', [0, 0, 0, 1])
            require(abs(x*x + y*y + z*z + w*w - 1) < 1e-4, 'Non-unit node quaternion')
            local = np.eye(4)
            local[:3, :3] = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                             [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                             [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
            local[:3, :3] *= np.array(node.get('scale', [1, 1, 1]))[None, :]
            local[:3, 3] = node.get('translation', [0, 0, 0])
        require(np.isfinite(local).all() and abs(np.linalg.det(local)) > 1e-9, 'Invalid node transform')
        parent = self.parents[index]
        self.worlds[index] = local if parent < 0 else self.world(parent, visiting) @ local
        visiting.remove(index)
        return self.worlds[index]


def audit(path, rig_path):
    glb = Glb(path); doc = glb.doc; bones = wire.skeleton(Path(rig_path))
    require(0 < len(bones) <= 30, 'Creature rig exceeds 30-bone cap')
    require(len(doc.get('skins', [])) == 1, 'Exactly one shared creature skin required')
    skin = doc['skins'][0]; joints = skin['joints']
    require(len(joints) == len(bones) and len(set(joints)) == len(joints), 'Joint count or uniqueness mismatch')
    names = [doc['nodes'][i].get('name', '') for i in joints]
    expected = [b['name'] for b in bones]
    require(len(set(names)) == len(names) and set(names) == set(expected), 'Joint names differ from shared rig')
    remap = [expected.index(name) for name in names]
    inverse = glb.accessor(skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)
    require(len(inverse) == len(joints), 'Inverse bind count mismatch')
    joint_error = 0.; bind_error = 0.
    for i, node_index in enumerate(joints):
        bone = bones[remap[i]]; world = glb.world(node_index)
        error = float(np.max(np.abs(world[:3, 3] - bone['world'][:3, 3])))
        joint_error = max(joint_error, error)
        require(error < 1e-5, f'Joint {bone["name"]} is displaced from shared rig by {error:g}m')
        require(np.allclose(world, bone['world'], atol=1e-5, rtol=0), f'Joint {bone["name"]} has a different bind orientation or scale')
        require(np.allclose(inverse[i], bone['inverse'], atol=1e-5, rtol=0), f'Joint {bone["name"]} has a different inverse bind')
        parent = glb.parents[node_index]
        while parent >= 0 and parent not in joints:
            parent = glb.parents[parent]
        actual = -1 if parent < 0 else remap[joints.index(parent)]
        require(actual == bone['parent'], f'Hierarchy mismatch at {bone["name"]}')
        error = float(np.max(np.abs(world @ inverse[i] - np.eye(4))))
        bind_error = max(bind_error, error)
        require(error < 1e-5, f'Inverse bind changes rest geometry at {bone["name"]}')
    vertices = triangles = 0; bounds = []
    for node_index, node in enumerate(doc['nodes']):
        if 'mesh' not in node:
            continue
        require(node.get('skin') == 0, 'Unskinned mesh in creature source')
        require(np.allclose(glb.world(node_index), np.eye(4), atol=1e-5), 'Apply mesh transform before creature export')
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            require(primitive.get('mode', 4) == 4 and not primitive.get('targets'), 'Only triangle meshes without morphs supported')
            attrs = primitive['attributes']
            for key in ('POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0'):
                require(key in attrs, f'Missing {key}')
            require('JOINTS_1' not in attrs and 'WEIGHTS_1' not in attrs, 'More than four weights unsupported')
            p = glb.accessor(attrs['POSITION']); w = glb.accessor(attrs['WEIGHTS_0']); j = glb.accessor(attrs['JOINTS_0'])
            require(p.shape[1] == 3 and w.shape == j.shape == (len(p), 4), 'Skin accessor shape mismatch')
            require(j.dtype.kind == 'u' and np.all(j < len(joints)), 'Invalid joint index')
            require(np.all(w >= 0) and np.allclose(w.sum(axis=1), 1, atol=1e-5), 'Unnormalized or negative weights')
            normal = glb.accessor(attrs['NORMAL']); uv = glb.accessor(attrs['TEXCOORD_0'])
            require(normal.shape == p.shape and uv.shape == (len(p), 2), 'Normal/UV count mismatch')
            require(np.allclose(np.linalg.norm(normal, axis=1), 1, atol=.002), 'Non-unit normals')
            indices = glb.accessor(primitive['indices'])
            require(indices.shape[1] == 1 and indices.dtype.kind == 'u', 'Invalid index accessor')
            require(len(indices) % 3 == 0 and indices.max() < len(p), 'Triangle index out of bounds')
            vertices += len(p); triangles += len(indices)//3; bounds.extend([p.min(axis=0), p.max(axis=0)])
            m = doc['materials'][primitive['material']]; pbr = m.get('pbrMetallicRoughness', {})
            for texture in (pbr.get('baseColorTexture'), pbr.get('metallicRoughnessTexture'), m.get('normalTexture')):
                require(texture is not None and texture.get('texCoord', 0) == 0, 'Missing PBR texture or unsupported UV set')
                image = doc['images'][doc['textures'][texture['index']]['source']]
                require('bufferView' in image and 'uri' not in image, 'Texture is not embedded')
    require(vertices > 0, 'No mesh instances')
    bounds = np.array(bounds)
    return {'source': str(Path(path).resolve()), 'shared_rig': str(Path(rig_path).resolve()),
            'technical_pass': True, 'visual_acceptance': False, 'runtime_import_verified': False,
            'vertices': vertices, 'triangles': triangles, 'bones': len(bones),
            'materials': len(doc['materials']), 'max_joint_position_error_m': joint_error,
            'max_bind_identity_error': bind_error, 'gltf_joint_to_shared_bone': remap,
            'bounds_min': bounds.min(axis=0).tolist(), 'bounds_max': bounds.max(axis=0).tolist()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('glb', type=Path); parser.add_argument('rig', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = audit(args.glb, args.rig)
    encoded = json.dumps(result, indent=2)
    if args.report:
        args.report.write_text(encoded + '\n', encoding='utf-8')
    print(encoded)
