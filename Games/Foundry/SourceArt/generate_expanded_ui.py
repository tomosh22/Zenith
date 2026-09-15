"""Original static recipe, throughput and terrain-map imagery for the extra views."""
from pathlib import Path
import bpy, numpy as np
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Assets/Textures/UI'
OUT.mkdir(parents=True,exist_ok=True)
def save(name,pixels):
    h,w,_=pixels.shape
    im=bpy.data.images.new(name,width=w,height=h,alpha=True)
    im.pixels.foreach_set(pixels[::-1].copy().ravel())
    im.filepath_raw=str(OUT/(name+'.png'));im.file_format='PNG';im.save()
N=256
a=np.zeros((N,N,4),dtype=np.float32)
a[40:216,32:224]=(.09,.40,.19,1)
a[44:212,36:220]=(.16,.56,.27,1)
for i in range(8):
    p=53+i*21
    a[31:48,p:p+10]=(.72,.73,.61,1);a[208:225,p:p+10]=(.72,.73,.61,1)
    a[p:p+7,41:102]=(.57,.75,.33,1);a[p:p+7,156:215]=(.57,.75,.33,1)
a[88:171,87:172]=(.025,.055,.047,1)
a[98:160,98:161]=(.10,.15,.14,1)
for x,y in [(47,57),(196,57),(47,190),(196,190)]:a[y:y+10,x:x+10]=(.75,.77,.63,1)
save('Circuit',a)
a=np.zeros((64,256,4),dtype=np.float32)
for x in range(256):
    y=int(30+8*np.sin(x*.08)+4*np.sin(x*.21))
    a[y:,x]=(.03,.18,.10,.7);a[y-2:y+2,x]=(.19,.86,.34,1)
save('Throughput',a)
y,x=np.mgrid[0:256,0:256]
a=np.ones((256,256,4),dtype=np.float32);a[:,:,:3]=(.055,.095,.085)
land=(np.sin(x*.043)+np.cos(y*.053)+np.sin((x+y)*.027)>-.7)
a[land,:3]=(.16,.24,.12)
river=abs(x-(52+14*np.sin(y*.035)))<9;a[river,:3]=(.025,.20,.29)
a[76:175,88:181,:3]=(.33,.37,.30)
for i in range(5):
    a[85+i*17:96+i*17,99:160,:3]=(.13,.18,.17)
a[68:71,83:191,:3]=.75;a[181:184,83:191,:3]=.75
a[68:184,83:86,:3]=.75;a[68:184,188:191,:3]=.75
rng=np.random.default_rng(3303)
for xx,yy in rng.integers((198,72),(248,179),(25,2)):a[yy:yy+3,xx:xx+3,:3]=(.85,.15,.09)
save('Minimap',a)
print('EXPANDED_UI_COMPLETE')
