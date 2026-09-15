from pathlib import Path
import subprocess, struct
root = Path('C:/dev/Zenith')
relative = 'Games/Zenithmon/Assets/Scenes/Dawnmere.zscen'
base = subprocess.check_output(['git', 'show', 'HEAD:'+relative], cwd=root)
path = root/relative
current = bytearray(path.read_bytes())
assert len(current) == len(base)
# These five existing rocks pick up one or two ULPs on a tools re-export.
# Restore only those unrelated scale floats, preserving both sign transforms.
restored = []
for position in [82850, 83013, 83176, 83339, 83496]:
    for axis in [0, 4, 8]:
        offset = position+axis
        old = struct.unpack_from('<I', base, offset)[0]
        new = struct.unpack_from('<I', current, offset)[0]
        assert abs(old-new) <= 2, (offset, old, new)
        if old != new:
            restored.append(offset)
            current[offset:offset+4] = base[offset:offset+4]
path.write_bytes(current)
print({'restoredUnrelatedScaleOffsets': restored,
       'remainingChangedBytes': [i for i, (a,b) in enumerate(zip(base,current)) if a != b]})
