"""Read-only current-schema mesh/rig reader used by Blender source authoring.

The baked rig is a fitting reference, never rewritten by this module. Unsupported
envelopes or trailing/missing bytes fail closed instead of guessing binary data.
"""
import struct
import numpy as np

class Reader:
    def __init__(self,path):self.data=path.read_bytes();self.pos=0
    def read(self,fmt):
        result=struct.unpack_from('<'+fmt,self.data,self.pos);self.pos+=struct.calcsize('<'+fmt)
        return result[0] if len(result)==1 else result
    def string(self):
        n=self.read('I');value=self.data[self.pos:self.pos+n].decode();self.pos+=n;return value
    def array(self,count,width,dtype='<f4'):
        n=count*width;out=np.frombuffer(self.data,dtype=dtype,count=n,offset=self.pos).copy().reshape(count,width)
        self.pos+=n*np.dtype(dtype).itemsize;return out
    def envelope(self):
        magic,version,kind,schema=self.read('4I')
        if magic!=0x5a4e5448 or version!=1:raise ValueError('Unsupported Zenith envelope')
        return kind,schema
    def end(self):
        if self.pos!=len(self.data):raise ValueError(f'Unexpected payload length {self.pos} / {len(self.data)}')

def mesh(path):
    r=Reader(path);kind,schema=r.envelope()
    if (kind,schema)!=(3,1):raise ValueError(f'Unsupported mesh type/schema: {kind}/{schema}')
    n,indices,parts=r.read('3I')
    submeshes=[r.read('3I') for _ in range(parts)]
    r.read('10f')
    skinned=r.read('?');skeleton=r.string() if skinned else None
    arrays={}
    for name,width in [('positions',3),('normals',3),('uvs',2),('tangents',3),('bitangents',3),('colors',4)]:
        arrays[name]=r.array(n,width) if r.read('?') else None
    arrays['indices']=r.array(indices//3,3,'<u4')
    if skinned:
        arrays['joints']=r.array(n,4,'<u4');arrays['weights']=r.array(n,4)
    r.end();arrays.update(skeleton=skeleton,submeshes=submeshes)
    return arrays

def skeleton(path):
    r=Reader(path);kind,schema=r.envelope()
    if (kind,schema)!=(4,2):raise ValueError(f'Unsupported skeleton type/schema: {kind}/{schema}')
    n=r.read('I');bones=[]
    for _ in range(n):
        name=r.string();parent=r.read('i');position=r.read('3f');rotation=r.read('4f');scale=r.read('3f')
        local=r.array(4,4).T;world=r.array(4,4).T;inverse=r.array(4,4).T;assimp=r.read('?')
        bones.append(dict(name=name,parent=parent,position=position,rotation=rotation,scale=scale,local=local,world=world,inverse=inverse))
    r.end();return bones
