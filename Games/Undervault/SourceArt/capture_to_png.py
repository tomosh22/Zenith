"""Losslessly wrap Zenith's uncompressed BGRA TGA capture as a PNG."""
from pathlib import Path
import struct,zlib,sys
root=Path(__file__).resolve().parents[3]
# Optional paths also support the Planning and Flooding captures.
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else (sys.argv[1:] if sys.argv[0].endswith('.py') else [])
src=root/(args[0] if args else 'Build/artifacts/undervault/Colony.tga')
dst=root/(args[1] if len(args)>1 else 'Games/Undervault/Mockups/InEngine/Colony-1920x1080.png')
b=src.read_bytes(); print('header',list(b[:18]))
w,h=struct.unpack_from('<HH',b,12);bits=b[16]
assert bits==32 and b[2]==2,(w,h,bits,b[2])
data=b[18+b[0]:];rows=[]
for y in range(h):
 iy=y if b[17]&32 else h-1-y
 row=bytearray(data[iy*w*4:(iy+1)*w*4]);row[0::4],row[2::4]=row[2::4],row[0::4];rows.append(b'\0'+row)
def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
# Replace atomically so an open image preview cannot prevent truncating the old file.
temporary=dst.with_suffix('.tmp.png')
temporary.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))
temporary.replace(dst)
print(dst,w,h)
