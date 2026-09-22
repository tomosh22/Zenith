"""Original feathered avian sources authored with Blender's Python API.

Blender --background --factory-startup --disable-autoexec --python-exit-code 1
--python Tools/Art/create_lark_creatures.py -- [Pipwit Trillark Stratavis]
Feather anatomy reference: https://academy.allaboutbirds.org/feathers-article/
No external geometry or images are embedded. Visual acceptance remains open.
"""
from pathlib import Path
import bpy, numpy as np, math, sys
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_lancefish as art
art.OUT=art.ROOT/'Build/artifacts/authored_assets/creature_work/LarkLine'
art.OUT.mkdir(parents=True,exist_ok=True)
NAMES=['Spine00','Spine01','Spine02','Head','Beak','WingL','WingR',
       'LegLUp','LegLLo','LegRUp','LegRLo','Tail00','Tail01']
PARENTS=[-1,0,1,2,3,1,1,0,7,0,9,0,11]
ATLAS_PATH=Path(__file__).resolve().parent/'textures/lark_feather_atlas.png'
ATLAS=bpy.data.images.load(str(ATLAS_PATH),check_existing=True)
atlas_pixels=np.empty(ATLAS.size[0]*ATLAS.size[1]*4,np.float32)
ATLAS.pixels.foreach_get(atlas_pixels);atlas_pixels=atlas_pixels.reshape(ATLAS.size[1],ATLAS.size[0],4)


class Surface:
    def __init__(self,name,mat):self.name=name;self.mat=mat;self.v=[];self.f=[];self.uv=[];self.w=[]
    def grid(self,grid,weights,reverse=False):
        rows=len(grid);cols=len(grid[0]);start=len(self.v)
        for row,ws in zip(grid,weights):self.v.extend(row);self.w.extend(ws)
        for i in range(rows-1):
            for j in range(cols-1):
                self.f.append(tuple(start+k for k in (i*cols+j,i*cols+j+1,(i+1)*cols+j+1,(i+1)*cols+j)))
                self.uv.append([(j/(cols-1),i/(rows-1)),((j+1)/(cols-1),i/(rows-1)),((j+1)/(cols-1),(i+1)/(rows-1)),(j/(cols-1),(i+1)/(rows-1))])
                if reverse:self.f[-1]=tuple(reversed(self.f[-1]));self.uv[-1]=list(reversed(self.uv[-1]))
    def finish(self):
        if self.v:return art.mesh(self.name,self.v,self.f,self.mat,self.uv,self.w)


def feather_mat(name,colour,flight=False,tile=None):
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/(n-1);rng=np.random.default_rng(742)
    grain=rng.random((n,n)).astype(np.float32)-.5
    phase=v*(168 if flight else 75)+abs(u-.5)*45+.2*np.sin(v*34)
    barbs=(.5+.5*np.cos(phase*math.tau))**5
    shaft=np.exp(-((u-.5)/.012)**2)
    edge=np.clip((abs(u-.5)-.36)/.14,0,1)**2
    streak=np.exp(-((u-.5)/.18)**2)*np.clip((v-.1)/.5,0,1)
    c=np.tile(colour,(n,n,1))*(.82+.14*barbs[:,:,None]+.07*grain[:,:,None])
    c*=1-streak[:,:,None]*(.33 if flight else .20)
    c+=edge[:,:,None]*np.array([.08,.064,.040])+shaft[:,:,None]*np.array([.028,.025,.019])
    height=barbs*.003+shaft*.024+grain*.0005
    if tile is not None:
        x,y=tile;hh,ww=atlas_pixels.shape[0]//2,atlas_pixels.shape[1]//2
        c=atlas_pixels[y*hh:(y+1)*hh,x*ww:(x+1)*ww].copy()
    return art.pbr(name,art.image(name+' base',np.clip(c,0,1)),art.normal_image(name+' normal',height,18),
        art.image(name+' MR',np.stack([np.ones_like(v),np.clip(.63+edge*.10+grain*.10,.35,.9),np.zeros_like(v)],axis=2),True))


def compose(name,stage):
    H=[.34,.62,1.02][stage-1];rng=np.random.default_rng(371+stage)
    def P(p):return Vector(p)*H
    mats={
        'back':feather_mat(name+' warm mantle',[.28,.19,.10],tile=(0,1)),
        'breast':feather_mat(name+' buff breast',[.60,.48,.30],tile=(1,1)),
        'wing':feather_mat(name+' flight feather',[.16,.115,.075],True,tile=(0,0)),
        'blue':feather_mat(name+' slate blue crown',[.12,.23,.28],tile=(1,0)),
        'pale':feather_mat(name+' pale eyebrow',[.70,.64,.48],tile=(1,1)),
    }
    batch={key:Surface(name+' '+key+' feather tracts',mat) for key,mat in mats.items()}
    base=art.plain(name+' down undercoat',[.17,.115,.061],.85)
    beak=art.plain(name+' horn bill',[.12,.095,.057],.38)
    n=512;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n
    rings=(.5+.5*np.cos(v*math.tau*19+.12*np.sin(u*math.tau*5)))**8
    scutes=(.5+.5*np.cos(u*math.tau*6))**4
    foot=art.pbr(name+' tarsal scutes',art.image(name+' foot base',np.tile([.24,.17,.105],(n,n,1))*(.77+.23*rings[:,:,None])),
        art.normal_image(name+' foot normal',rings*.025+scutes*.006,24),
        art.image(name+' foot MR',np.stack([np.ones_like(u),.51+.14*(1-rings),np.zeros_like(u)],axis=2),True))
    claw=art.plain(name+' translucent horn claw',[.14,.102,.065],.35)
    iris=art.plain(name+' umber iris',[.11,.064,.023],.24)
    pupil=art.plain(name+' wet pupil',[.002,.003,.003],.09)
    seam=art.plain(name+' bill commissure',[.018,.012,.007],.64)
    heads=[(0,.06,.35),(0,.02,.49),(0,-.07,.62),(0,-.16,.76),(0,-.29,.755),
           (-.175,-.015,.59),(.175,-.015,.59),(-.085,.04,.29),(-.09,.075,.16),
           (.085,.04,.29),(.09,.075,.16),(0,.245,.34),(0,.385,.265)]
    def body_weight(p):
        z=p.z/H
        knots=[.35,.49,.62,.76]
        if z<=knots[0]:return {'Spine00':1}
        if z>=knots[-1]:return {'Head':1}
        for i in range(3):
            if z<=knots[i+1]:
                t=(z-knots[i])/(knots[i+1]-knots[i]);t=t*t*(3-2*t)
                return {NAMES[i]:1-t,NAMES[i+1]:t}
    # Feather tracts follow curved surfaces; tips overlap the next row instead
    # of floating tangent cards above a smooth sphere.
    def tract(center,size,rows,columns,weight,choose,theta_start=.13,theta_end=2.88):
        center=Vector(center);size=Vector(size)
        art.ellipsoid(name+' hidden down volume',P(center),P(size),'Spine01',base)
        under=art.objects[-1];under.vertex_groups.clear();under.data.materials.clear()
        keys=list(mats)
        for key in keys:under.data.materials.append(mats[key])
        # The dense underplumage shares the tract pigment. Small spaces between
        # contour feather tips must not expose an unrelated dark body colour.
        for poly in under.data.polygons:
            q=poly.center/H-center
            theta=math.acos(float(np.clip(q.z/size.z,-1,1)));phi=math.atan2(q.y/size.y,q.x/size.x)
            poly.material_index=keys.index(choose(theta,phi))
        for b in NAMES:under.vertex_groups.new(name=b)
        for vertex in under.data.vertices:
            ws=weight(vertex.co) if callable(weight) else {weight:1}
            for b,w in ws.items():
                if w>0:under.vertex_groups[b].add([vertex.index],w,'REPLACE')
        def point(theta,phi):return center+Vector((size.x*math.sin(theta)*math.cos(phi),size.y*math.sin(theta)*math.sin(phi),size.z*math.cos(theta)))
        for row in range(rows):
            theta=theta_start+(theta_end-theta_start)*row/(rows-1)
            count=max(8,int(columns*math.sin(theta)))
            for j in range(count):
                phi=math.tau*(j+(row%2)*.5)/count+float(rng.uniform(-.026,.026))
                root_theta=theta+float(rng.uniform(-.010,.010))
                reach=(theta_end-theta_start)/rows*2.05*float(rng.uniform(.90,1.08))
                span=math.tau/count*.82
                grid=[];ws=[]
                for k in range(7):
                    t=k/6;shape=(.28+.72*math.sin(math.pi*t)**.40)*(1-.75*t**12)
                    points=[];weights=[]
                    for a in np.linspace(-1,1,5):
                        edge=1+.075*math.sin(k*2.73+j*7.4+row*3.5)*abs(float(a))
                        p=point(root_theta+reach*t,phi+span*float(a)*shape*edge)
                        normal=Vector(((p.x-center.x)/size.x**2,(p.y-center.y)/size.y**2,(p.z-center.z)/size.z**2)).normalized()
                        p+=normal*(.0007+.0011*math.sin(t*math.pi)+.0012*t*t)*(1-.15*abs(float(a)))
                        p=P(p);points.append(p);weights.append(weight(p) if callable(weight) else {weight:1})
                    grid.append(points);ws.append(weights)
                batch[choose(theta,phi)].grid(grid,ws,reverse=True)
    # One continuous longitudinal profile connects breast, nape and skull.
    # Each row carries the same skin interpolation as the underplumage.
    knots=np.array([.16,.22,.34,.47,.57,.65,.72,.79,.85,.90,.918])
    profiles=np.array([
        [.009,.015,.055],[.115,.140,.055],[.184,.243,.050],
        [.195,.261,.045],[.159,.218,.010],[.111,.151,-.075],
        [.109,.136,-.151],[.128 if stage==1 else .119,.144,-.177],
        [.108,.119,-.176],[.061,.065,-.166],[.001,.001,-.159]])
    slopes=np.gradient(profiles,knots,axis=0)
    def profile(z):
        z=float(np.clip(z,knots[0],knots[-1]));i=min(len(knots)-2,max(0,int(np.searchsorted(knots,z)-1)))
        step=knots[i+1]-knots[i];t=(z-knots[i])/step
        v=(2*t**3-3*t*t+1)*profiles[i]+(t**3-2*t*t+t)*step*slopes[i]+(-2*t**3+3*t*t)*profiles[i+1]+(t**3-t*t)*step*slopes[i+1]
        return max(.001,float(v[0])),max(.001,float(v[1])),float(v[2])
    def point(z,phi):
        rx,ry,cy=profile(z);return Vector((rx*math.cos(phi),cy+ry*math.sin(phi),z))
    def colour(z,phi):
        if z>.858+.006*math.sin(phi*5):return 'blue'
        if .794<z<.813 and abs(math.cos(phi))>.40:return 'pale'
        return 'breast' if math.sin(phi)<-.20 else 'back'
    bases={k:Surface(name+' continuous '+k+' underplumage',m) for k,m in mats.items()}
    for i in range(95):
        z0=.918-(.918-.16)*i/95;z1=.918-(.918-.16)*(i+1)/95
        for j in range(64):
            phi0=math.tau*j/64;phi1=math.tau*(j+1)/64
            grid=[[P(point(z,phi)) for phi in (phi0,phi1)] for z in (z0,z1)]
            bases[colour((z0+z1)*.5,(phi0+phi1)*.5)].grid(grid,[[body_weight(p) for p in row] for row in grid],reverse=True)
    for surface in bases.values():surface.finish()
    for row in range(55):
        z=.91-(.91-.19)*row/54;rx,ry,cy=profile(z);count=max(10,int(70*rx/.195))
        for j in range(count):
            phi=math.tau*(j+(row%2)*.5)/count+float(rng.uniform(-.020,.020));zroot=z+float(rng.uniform(-.003,.003))
            span=math.tau/count*.79;reach=.030*float(rng.uniform(.92,1.10));grid=[];weights=[]
            for k in range(7):
                t=k/6;shape=(.27+.73*math.sin(math.pi*t)**.42)*(1-.75*t**12);points=[];ws=[]
                for a in np.linspace(-1,1,5):
                    angle=phi+span*float(a)*shape;zz=max(.161,zroot-reach*t);p=point(zz,angle)
                    dp=point(zz,angle+.001)-point(zz,angle-.001);dz=point(min(.917,zz+.001),angle)-point(max(.160,zz-.001),angle)
                    normal=dp.cross(dz).normalized();p+=normal*(.0009+.0012*math.sin(t*math.pi)+.0010*t*t)
                    p=P(p);points.append(p);ws.append(body_weight(p))
                grid.append(points);weights.append(ws)
            batch[colour(zroot,phi)].grid(grid,weights,reverse=True)

    def feather(key,root,tip,width,normal,bone,curve=.015,asym=.85,segments=16):
        root=Vector(root);tip=Vector(tip);axis=tip-root;normal=Vector(normal).normalized();side=axis.normalized().cross(normal).normalized()
        grid=[];weights=[]
        for k in range(segments+1):
            t=k/segments;shape=(.16+.84*math.sin(math.pi*t)**.7)*(1-.97*t**12)
            row=[];ws=[]
            for a in np.linspace(-1,1,7):
                aa=float(a);b=aa*(asym if aa<0 else 1)
                fringe=1+.045*math.sin(k*2.7+root.x*170)*abs(b)
                p=root+axis*t+side*(b*width*shape*fringe)+normal*(curve*math.sin(t*math.pi)-width*.18*abs(b)*shape)
                row.append(P(p));ws.append(bone(P(p)) if callable(bone) else {bone:1})
            grid.append(row);weights.append(ws)
        batch[key].grid(grid,weights)
        if segments>=12:
            shaft=[P(root+axis*t+normal*(curve*math.sin(t*math.pi)+.0005)) for t in np.linspace(.03,.95,14)]
            art.tube(name+' feather rachis',shaft,[H*(.0012*(1-k/13)+.00018) for k in range(14)],bone if isinstance(bone,str) else 'Tail00',mats[key],5,bone if callable(bone) else None)

    for side,letter in [(-1,'L'),(1,'R')]:
        wing='Wing'+letter
        tract((side*.165,.095,.475),(.038,.152,.101),12,20,wing,lambda t,p:'back')
        # Long asymmetric primaries and shorter secondaries overlap in a folded
        # fan; coverts conceal their insertion, with a separate blue shoulder.
        for j in range(10):
            t=j/9;root=(side*(.17+.060*t),-.015+.15*t,.57-.10*t)
            tip=(side*(.23+.045*t),.30+.18*t+stage*.015,.235-.045*t)
            feather('wing',root,tip,.034,(side,0,.3),wing,curve=.012,asym=.63)
        for j in range(9):
            t=j/8;feather('wing',(side*(.171+.052*t),-.025+.08*t,.59-.11*t),
                (side*(.235+.035*t),.225+.13*t,.29-.08*t),.042,(side,0,.25),wing,asym=.90)
        for row in range(3):
            for j in range(9):
                t=j/8;x=side*(.186+.018*row+.049*t);y=-.035+.16*t+.045*row;z=.610-.092*t-.055*row
                feather('blue' if row==0 and j<5 else 'back',(x,y,z),(x+side*.013,y+.105,z-.108),.035,(side,0,.2),wing,curve=.012,segments=8)
        # Two visible tarsometatarsi; front digits II/III/IV and a rear hallux.
        hip=P((side*.085,.04,.29));hock=P((side*.09,.075,.16));ankle=P((side*.09,.017,.036))
        art.tube(name+' feathered thigh '+letter,[hip,P((side*.09,.060,.23)),hock],[H*.029,H*.025,H*.014],'Leg'+letter+'Up',base,16)
        art.tube(name+' scaled tarsus '+letter,[hock,P((side*.09,.045,.10)),ankle],[H*.013,H*.011,H*.013],'Leg'+letter+'Lo',foot,16)
        for toe in range(4):
            if toe==3:
                middle=(side*.085,.068,.020);end=(side*.073,.112,.017);tip=(side*.068,.137,.010)
            else:
                dx=(toe-1)*.045;middle=(side*.09+dx*.65,-.049,.020);end=(side*.09+dx,-.110+(abs(toe-1)*.015),.014);tip=(end[0],end[1]-.026,.007)
            art.tube(name+' toe '+letter,[ankle,P(middle),P(end)],[H*.010,H*.008,H*.005],'Leg'+letter+'Lo',foot,12)
            art.tube(name+' curved talon '+letter,[P(end),P(((end[0]+tip[0])*.5,(end[1]+tip[1])*.5,.018)),P(tip)],[H*.006,H*.004,H*.0003],'Leg'+letter+'Lo',claw,12)
        # Eye sits in a feathered orbit; small bill/nostrils are horn, not feathers.
        eye=(side*.111,-.227,.796)
        art.ellipsoid(name+' eye iris '+letter,P(eye),P((.015,.026,.023)),'Head',iris)
        art.ellipsoid(name+' eye pupil '+letter,P((side*.119,-.232,.797)),P((.009,.019,.019)),'Head',pupil)
        points=[P((side*(.112+.003*math.sin(a)),-.227+.028*math.cos(a),.796+.025*math.sin(a))) for a in np.linspace(0,math.tau,49)]
        art.tube(name+' orbital rim '+letter,points,[H*.002]*49,'Head',beak,7)

    # Bill with separate upper/lower mandibles and an actual commissure line.
    art.tube(name+' upper bill',[P((0,-.282,.765)),P((0,-.322,.763)),P((0,-.386-stage*.018,.746))],[H*.039,H*.029,H*.0007],'Beak',beak,24)
    art.tube(name+' lower bill',[P((0,-.279,.746)),P((0,-.328,.743)),P((0,-.379-stage*.018,.744))],[H*.023,H*.015,H*.0007],'Beak',beak,20)
    for side in (-1,1):
        art.tube(name+' bill opening',[P((side*.032,-.293,.748)),P((side*.020,-.34,.747)),P((side*.003,-.387,.745))],[H*.0014]*3,'Beak',seam,6)
        art.ellipsoid(name+' nostril',P((side*.030,-.314,.776)),P((.002,.007,.004)),'Beak',seam)
    # Twelve rectrices form a gently spread two-joint tail, longer with maturity.
    def tail_weight(p):
        t=float(np.clip((p.y/H-.33)/.16,0,1));return {'Tail00':1-t,'Tail01':t}
    for j in range(12):
        t=(j-5.5)/5.5;root=(t*.049,.222,.335)
        tip=(t*(.13+stage*.012),.59+stage*.07-abs(t)*.060,.12+abs(t)*.016)
        feather('pale' if j in (0,11) else 'wing',root,tip,.030,(0,.45,1),tail_weight,curve=.015,asym=.90)
    # A swept blue crown persists through the line, gradually becoming a crest.
    for j in range(5+stage*2):
        t=(j-(4+stage*2)*.5)/(4+stage*2)
        feather('blue',(t*.065,-.175,.892 if stage==1 else .88),
                (t*.12,-.08+stage*.045,.925+stage*.035-abs(t)*.07),.024,(0,-.6,1),'Head',curve=.010,segments=12)
    for surface in batch.values():surface.finish()
    return NAMES,PARENTS,[P(p) for p in heads]


art.compose=compose
if __name__=='__main__':
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for stage,name in enumerate(['Pipwit','Trillark','Stratavis'],1):
        if requested and name not in requested:continue
        art.deliver(name,stage,{'source':'original-lark-authoring','recipe':'Tools/Art/create_lark_creatures.py','externalSources':[],
            'generatedTexture':{'path':'Tools/Art/textures/lark_feather_atlas.png','method':'built-in image_gen','prompt':'Tools/Art/textures/lark_feather_atlas.prompt.txt','measuredReflectance':False},
            'anatomicalReferences':['https://academy.allaboutbirds.org/feathers-article/','https://www.allaboutbirds.org/guide/Eurasian_Skylark/id'],
            'features':'Layered contour, covert, primary, secondary and rectrix geometry; fine vane/rachis PBR maps; anisodactyl feet and horn bill; thirteen-joint canonical skin.',
            'visualAcceptance':False,'limitations':'Original fictional bird anatomy and authored optical values require photorealistic and animation review; references supply structure, not embedded imagery.'})
