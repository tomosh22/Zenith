"""Original botanical Puffseed / Dandelift / Zephyrbloom sources in Blender.

Run Blender --background --factory-startup --disable-autoexec
--python-exit-code 1 --python Tools/Art/create_windseed_creatures.py -- [names].
Geometry and maps are authored here; botanical references inform structure only.
"""
from pathlib import Path
import bpy, numpy as np, math, sys
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_lancefish as art
art.OUT=art.ROOT/'Build/artifacts/authored_assets/creature_work/Windseed'
art.OUT.mkdir(parents=True,exist_ok=True)
NAMES=['Spine00','Spine01','Spine02','Head']+['Tendril'+str(i) for i in range(6)]
PARENTS=[-1,0,1,2]+[0]*6


def linspace(a,b,n):return [float(x) for x in np.linspace(a,b,n)]


class Batch:
    def __init__(self,name,mat):
        self.name,self.mat=name,mat;self.v=[];self.f=[];self.uv=[];self.w=[]
    def surface(self,grid,weights):
        rows=len(grid);cols=len(grid[0]);base=len(self.v)
        for row,wr in zip(grid,weights):self.v.extend(row);self.w.extend(wr)
        for i in range(rows-1):
            for j in range(cols-1):
                self.f.append(tuple(base+k for k in (i*cols+j,i*cols+j+1,(i+1)*cols+j+1,(i+1)*cols+j)))
                self.uv.append([(j/(cols-1),i/(rows-1)),((j+1)/(cols-1),i/(rows-1)),((j+1)/(cols-1),(i+1)/(rows-1)),(j/(cols-1),(i+1)/(rows-1))])
    def tube(self,points,radii,bone,sides=6,ribs=0):
        points=list(map(Vector,points));grid=[];weights=[]
        for i,p in enumerate(points):
            axis=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized()
            ref=Vector((0,0,1)) if abs(axis.z)<.9 else Vector((1,0,0))
            a=axis.cross(ref).normalized();b=axis.cross(a)
            grid.append([p+float(radii[i])*(1+(.065*math.cos(j*math.tau/sides*ribs+.3*math.sin(i*.17)) if ribs else 0))*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides)) for j in range(sides+1)])
            weights.append([bone(p) if callable(bone) else {bone:1} for j in range(sides+1)])
        self.surface(grid,weights)
    def finish(self):
        if self.v:return art.mesh(self.name,self.v,self.f,self.mat,self.uv,self.w)


def material(name,kind,stage):
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n
    rng=np.random.default_rng(2189+stage+len(kind));grain=rng.random((n,n)).astype(np.float32)-.5
    cloud=sum(np.sin(u*f*math.tau+p)*np.cos(v*(f+.7)*math.tau-p)/f for f,p in ((3,.7),(9,1.9),(23,.6),(71,2.1)))
    if kind=='leaf':
        ribs=np.exp(-(np.sin((v*17+abs(u-.5)*4)*math.pi)/.20)**2)
        mid=np.exp(-((u-.5)/.018)**2)
        colour=np.tile([.065,.17,.029],(n,n,1))*(1+cloud[:,:,None]*.28+grain[:,:,None]*.055)
        colour+=np.stack([mid*.024+ribs*.008,mid*.052+ribs*.019,mid*.007],axis=2)
        height=ribs*.009+mid*.04+grain*.001;rough=.53+grain*.12
    elif kind=='pod':
        ribs=(.5+.5*np.cos(u*math.tau*13+.7*np.sin(v*6)))**8
        colour=np.tile([.24,.145,.054],(n,n,1))*(.76+cloud[:,:,None]*.22+ribs[:,:,None]*.34+grain[:,:,None]*.055)
        colour=colour*(1-v[:,:,None]*.20)+np.array([.10,.16,.031])*v[:,:,None]*.20
        height=ribs*.025+grain*.004;rough=.62+grain*.13
    elif kind=='hair':
        colour=np.tile([.84,.79,.64],(n,n,1))*(.92+v[:,:,None]*.07+grain[:,:,None]*.045)
        height=grain*.001;rough=.72+grain*.10
    elif kind=='seed':
        ribs=(.5+.5*np.cos(u*math.tau*8))**5
        colour=np.tile([.18,.092,.028],(n,n,1))*(.76+ribs[:,:,None]*.32+grain[:,:,None]*.09)
        height=ribs*.035+grain*.005;rough=.70+grain*.14
    else:
        colour=np.tile([.82,.49,.016],(n,n,1))*(.78+v[:,:,None]*.23+grain[:,:,None]*.065)
        height=np.cos(u*math.tau*9)*.01+grain*.002;rough=.47+grain*.09
    return art.pbr(name+' '+kind,art.image(name+' '+kind+' base',np.clip(colour,0,1)),
        art.normal_image(name+' '+kind+' relief',height,12),
        art.image(name+' '+kind+' MR',np.stack([np.ones_like(u),np.clip(rough,.1,.95),np.zeros_like(u)],axis=2),True))


def compose(name,stage):
    H=[.44,.79,1.22][stage-1];rng=np.random.default_rng(910+stage)
    mats={k:material(name,k,stage) for k in ('leaf','pod','hair','seed','petal')}
    batches={k:Batch(name+' '+k+' surfaces',m) for k,m in mats.items()}
    heads=[Vector((0,0,z*H)) for z in (.16,.31,.47,.62)]
    def weight(p):
        t=p[2]/H;knots=[.16,.31,.47,.62]
        if t<=knots[0]:return {'Spine00':1}
        if t>=knots[-1]:return {'Head':1}
        for i in range(3):
            if knots[i]<=t<=knots[i+1]:
                f=(t-knots[i])/(knots[i+1]-knots[i]);f=f*f*(3-2*f)
                return {NAMES[i]:1-f,NAMES[i+1]:f}
    # The achene-like body has real longitudinal ribs and a softly curved stalk.
    points=[];radii=[]
    for t in linspace(0,1,65):
        z=(.12+.53*t)*H
        points.append(Vector((.012*H*math.sin(t*4),.01*H*math.sin(t*3),z)))
        radii.append(H*(.009+.09*math.sin(math.pi*t)**1.4*(1-.63*t)))
    batches['pod'].tube(points,radii,weight,64,13)
    # Six independently skinned leaves retain the canonical tendril attachments.
    for k in range(6):
        angle=k*math.tau/6;d=Vector((math.cos(angle),math.sin(angle),0));side=Vector((-d.y,d.x,0))
        anchor=d*(.046*H)+Vector((0,0,.27*H));heads.append(anchor)
        length=H*(.19+.055*stage)*rng.uniform(.87,1.10);grid=[];weights=[];mid=[]
        for i in range(49):
            t=i/48;centre=anchor+d*(length*t)+Vector((0,0,H*(-.11*t-.035*math.sin(t*math.pi))))
            mid.append(centre);width=H*(.00012+(.024+.009*stage)*math.sin(math.pi*t)**.70)
            width*=.69+.31*(.5+.5*math.cos(t*math.tau*(6+stage)))**.7
            row=[];wr=[]
            blend=min(1,t*3);ws={'Spine00':1-blend,'Tendril'+str(k):blend}
            for j in range(9):
                s=j/4-1;row.append(centre+side*width*s+Vector((0,0,H*(.012*abs(s)**1.4+.004*s*math.sin(t*11)))))
                wr.append(ws)
            grid.append(row);weights.append(wr)
        batches['leaf'].surface(grid,weights)
        batches['leaf'].tube(mid,[H*(.0016*(1-i/48)+.00015) for i in range(49)],'Tendril'+str(k),6)
        # Fine feeder roots curl below the seed body; each shares its leaf joint.
        root=[Vector((0,0,H*(.19-.14*t)))+d*(H*(.025+.07*t))+side*(H*.015*math.sin(t*6)) for t in linspace(0,1,18)]
        batches['pod'].tube(root,[H*(.003*(1-i/17)+.00025) for i in range(18)],'Tendril'+str(k),6)
    # Ribbed seeds, each with its own beak and open radial pappus. Deliberately
    # uneven spacing/lengths preserve the delicate structure instead of a shell.
    centre=Vector((0,0,.66*H));core_radius=H*[.064,.087,.10][stage-1]
    art.ellipsoid(name+' receptacle',centre,(core_radius,core_radius,core_radius*.8),'Head',mats['seed'])
    count=[67,113,157][stage-1]
    for i in range(count):
        z=1-1.69*(i+.5)/count;theta=i*math.pi*(3-math.sqrt(5))+rng.uniform(-.075,.075)
        d=Vector((math.sqrt(1-z*z)*math.cos(theta),math.sqrt(1-z*z)*math.sin(theta),z))
        a=d.cross(Vector((0,0,1))).normalized();b=d.cross(a)
        root=centre+d*core_radius*.94
        seedlength=H*rng.uniform(.025,.038);tip=root+d*seedlength
        seedpoints=[root.lerp(tip,float(t)) for t in linspace(0,1,7)]
        batches['seed'].tube(seedpoints,[H*(.0007+.0038*math.sin(math.pi*j/6)) for j in range(7)],'Head',8)
        beak=tip+d*(H*rng.uniform(.087,.12))
        batches['hair'].tube([tip,tip.lerp(beak,.5),beak],[H*.0009,H*.0007,H*.0005],'Head',5)
        fan_radius=H*rng.uniform(.039,.061)
        hairs=[44,54,62][stage-1]
        for j in range(hairs):
            t=j*math.tau/hairs+rng.uniform(-.025,.025);v=a*math.cos(t)+b*math.sin(t)
            reach=fan_radius*rng.uniform(.81,1.12)
            hairpoints=[beak+v*(reach*q)+d*(H*(.009*q+.022*q*q))+a*(H*.0015*math.sin(q*5+j)) for q in linspace(0,1,5)]
            batches['hair'].tube(hairpoints,[H*(.00038*(1-q)+.00008) for q in linspace(0,1,5)],'Head',3)
    # The mature form carries two smaller flowering buds below its seed crown.
    if stage==3:
        for sign in (-1,1):
            bud=Vector((sign*.23*H,-.035*H,.46*H));stemroot=Vector((0,0,.43*H))
            batches['leaf'].tube([stemroot,stemroot.lerp(bud,.45)+Vector((0,0,.04*H)),bud],[H*.007,H*.005,H*.008],'Spine02',12)
            art.ellipsoid(name+' flower disk',bud,(H*.035,H*.035,H*.017),'Spine02',mats['petal'])
            for k in range(65):
                angle=k*math.pi*(3-math.sqrt(5));rad=H*(.021+.034*(k/64))
                d=Vector((math.cos(angle),math.sin(angle),0));side=Vector((-d.y,d.x,0));grid=[];weights=[]
                for t in linspace(0,1,13):
                    c=bud+d*(H*.012+rad*t)+Vector((0,0,H*(.026*math.sin(t*math.pi*.8)-.003*t)))
                    width=H*(.00008+.0032*math.sin(math.pi*t)**.6)
                    grid.append([c+side*width*s for s in (-1,-.5,0,.5,1)]);weights.append([{'Spine02':1}]*5)
                batches['petal'].surface(grid,weights)
    # Small dark seed-coat eyes distinguish the creature without plastic bulbs.
    eye=art.plain(name+' dark seed eyes',[.009,.013,.005],.20)
    eye_t=(.31-.12)/.53
    body_x=.012*H*math.sin(eye_t*4);body_y=.01*H*math.sin(eye_t*3)
    body_r=H*(.009+.09*math.sin(math.pi*eye_t)**1.4*(1-.63*eye_t))
    eye_y=body_y-math.sqrt(body_r**2-(.031*H)**2)-.001*H
    for sign in (-1,1):art.ellipsoid(name+' eye',Vector((body_x+sign*.031*H,eye_y,.31*H)),(H*.011,H*.008,H*.013),'Spine01',eye)
    for batch in batches.values():batch.finish()
    for ob in art.objects:ob.data.validate(verbose=True,clean_customdata=True)
    return NAMES,PARENTS,heads


art.compose=compose
requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
for stage,name in enumerate(('Puffseed','Dandelift','Zephyrbloom'),1):
    if requested and name not in requested:continue
    art.deliver(name,stage,{'source':'original-windseed-botanical-authoring','recipe':'Tools/Art/create_windseed_creatures.py',
        'externalAssets':[],'botanicalReferences':['https://www.kew.org/plants/dandelion','https://www.nature.com/articles/s41586-018-0604-2'],
        'features':'Ribbed seed body, six serrated leaves, curled roots, individually modelled achenes/beaks/pappus filaments; mature flowering side branches.',
        'materials':'Embedded base colour, tangent normal and green-roughness/blue-metallic maps; dielectric tissues; geometry filaments, no billboard clouds.',
        'limitations':'Authored optical parameters, not measured botanical scattering; deformation and in-engine filament visibility require review.','visualAcceptance':False})
