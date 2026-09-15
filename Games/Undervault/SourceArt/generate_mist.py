"""Seeded soft water/gas billboard, authored for Flux GPU particle alpha blending."""
from pathlib import Path
import bpy, numpy as np
out=Path(__file__).resolve().parents[1]/'Assets/Textures/Particles/Mist.png'
out.parent.mkdir(parents=True,exist_ok=True)
n=256
y,x=np.mgrid[-1:1:complex(n),-1:1:complex(n)]
density=np.zeros((n,n));rng=np.random.default_rng(9012)
for i in range(22):
    cx,cy=rng.uniform(-.43,.43,2);r=rng.uniform(.12,.34)
    density+=np.exp(-((x-cx)**2+(y-cy)**2)/(r*r))*rng.uniform(.15,.4)
alpha=np.clip(density,0,1)*np.clip((1-x*x-y*y)*2,0,1)**2
rgba=np.ones((n,n,4),dtype=np.float32);rgba[:,:,3]=alpha
im=bpy.data.images.new('Undervault soft mist',width=n,height=n,alpha=True)
im.pixels.foreach_set(rgba.ravel());im.filepath_raw=str(out);im.file_format='PNG';im.save()
print('UNDERVAULT_MIST_TEXTURE',out)
