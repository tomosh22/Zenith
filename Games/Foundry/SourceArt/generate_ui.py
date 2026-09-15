"""Original vector-like HUD glyphs, rasterized deterministically through bpy."""
import bpy
import numpy as np
import math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Assets/Textures/UI';OUT.mkdir(parents=True,exist_ok=True)
N=128
Y,X=np.mgrid[0:N,0:N].astype(float);X=(X+.5)/2;Y=(Y+.5)/2

def polygon(points):
    mask=np.zeros((N,N),bool)
    for (ax,ay),(bx,by) in zip(points,points[1:]+points[:1]):
        mask ^= ((ay>Y)!=(by>Y)) & (X<(bx-ax)*(Y-ay)/(by-ay+1e-12)+ax)
    return mask

def disk(x,y,r):return (X-x)**2+(Y-y)**2<r*r

def line(a,b,width=3):
    ax,ay=a;bx,by=b;dx=bx-ax;dy=by-ay
    t=np.clip(((X-ax)*dx+(Y-ay)*dy)/(dx*dx+dy*dy+1e-12),0,1)
    return (X-ax-t*dx)**2+(Y-ay-t*dy)**2<(width/2)**2

def path(points,width=3):
    m=np.zeros((N,N),bool)
    for a,b in zip(points,points[1:]):m|=line(a,b,width)
    return m

icons={}
m=polygon([(10,53),(10,29),(23,35),(23,25),(37,32),(37,14),(43,14),(45,53)])
m|=polygon([(48,53),(48,5),(53,5),(55,53)])
for x in [16,28,40]:m &= ~polygon([(x,42),(x+5,42),(x+5,48),(x,48)])
icons['Machines']=m
m=path([(7,43),(25,18),(56,18),(39,43),(7,43),(7,49),(39,49),(56,25),(56,18)],3)
m|=path([(39,43),(39,49)],2)
for x,y in [(24,25),(38,25),(17,36),(31,36)]:m|=disk(x,y,2)
icons['Belts']=m
m=path([(32,9),(32,55)],5)|path([(13,21),(51,21)],4)
for x in [14,24,40,50]:m|=path([(x,16),(x,25)],3)|disk(x,15,3)
m|=path([(24,54),(40,54)],4);icons['Power']=m
m=polygon([(10,20),(31,8),(54,21),(54,46),(33,58),(10,45)])
m &= ~path([(10,20),(33,33),(54,21)],3)
m &= ~path([(33,33),(33,58)],3);icons['Logistics']=m
m=disk(32,32,18)
for i in range(8):
    a=i*math.pi/4
    m |= line((32+16*math.cos(a),32+16*math.sin(a)),(32+23*math.cos(a),32+23*math.sin(a)),10)
m &= ~disk(32,32,8);icons['Production']=m
m=path([(25,8),(39,8),(36,8),(36,25),(52,52),(49,56),(15,56),(12,52),(28,25),(28,8)],3)
m|=polygon([(23,39),(41,39),(48,51),(17,51)])
icons['Science']=m
m=polygon([(32,5),(44,23),(39,23),(50,39),(43,39),(56,51),(36,51),(36,60),(28,60),(28,51),(8,51),(21,39),(14,39),(25,23),(20,23)])
icons['Decor']=m
icons['More']=disk(14,33,4)|disk(32,33,4)|disk(50,33,4)
icons['Alert']=polygon([(35,4),(14,34),(29,34),(23,59),(51,25),(35,25)])
for name,mask in icons.items():
    data=np.ones((N,N,4),np.float32)
    data[:,:,:3]=.95;data[:,:,3]=mask.astype(np.float32)
    im=bpy.data.images.new('Foundry '+name,width=N,height=N,alpha=True)
    # bpy image coordinates begin at the bottom.
    im.pixels.foreach_set(data[::-1].copy().ravel())
    im.filepath_raw=str(OUT/(name+'.png'));im.file_format='PNG';im.save()
print('FOUNDRY_UI_COMPLETE',len(icons))
