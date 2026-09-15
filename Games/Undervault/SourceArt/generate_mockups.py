"""Undervault: original deterministic, posed 3D cutaway studies (seed 1209).
Blender --background --python generate_mockups.py [-- Colony|Planning|Flooding].
Coordinates: X across, Z up, -Y toward viewer; glTF export is +Y up.
"""
import bpy, math, random, json, sys
import numpy as np
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Assets/Dioramas'; OUT.mkdir(parents=True,exist_ok=True)
random.seed(1209)
M={}; lights=[]
def mat(name,c,metal=0,emit=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*c,1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1)
    p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=.62
    if emit:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=emit
    if name in ['sand','wall','sanddark','wood','woodlight'] or name.startswith('Basalt'):
        n=128;rng=np.random.default_rng(317)
        yy,xx=np.mgrid[0:n,0:n];noise=rng.random((n,n))
        grain=.78+.27*noise+.08*np.sin(xx*.17+np.sin(yy*.09)*2)+.06*np.sin(yy*.27)
        if 'wood' in name:grain=.8+.12*np.sin(xx*.45+np.sin(yy*.12))+.12*noise
        rgba=np.ones((n,n,4),dtype=np.float32);rgba[:,:,:3]=np.array(c)[None,None,:]*grain[:,:,None]
        im=bpy.data.images.new(name+' mineral grain',width=n,height=n);im.pixels.foreach_set(rgba.ravel());im.pack()
        tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=im;m.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color'])
        p.inputs['Base Color'].default_value=(1,1,1,1)
    M[name]=m;return m

def mesh(name,v,f,m):
    d=bpy.data.meshes.new(name);d.from_pydata(v,[],f);d.update();o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o);d.materials.append(m);return o

def box(name,p,s,m,bevel=.035):
    x,y,z=p;a,b,c=[n/2 for n in s]
    v=[(x+i*a,y+j*b,z+k*c) for i,j,k in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
    o=mesh(name,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],m)
    if bevel:
        mod=o.modifiers.new('Soft worn edges','BEVEL');mod.width=bevel;mod.segments=2
    return o

def rod(name,a,b,r,m,n=12):
    a,b=Vector(a),Vector(b);axis=(b-a).normalized();u=axis.cross(Vector((0,0,1)))
    if u.length<.01:u=axis.cross(Vector((0,1,0)))
    u.normalize();v=axis.cross(u);vs=[]
    for p in [a,b]:
        for i in range(n):vs.append(p+r*(u*math.cos(i*math.tau/n)+v*math.sin(i*math.tau/n)))
    return mesh(name,vs,[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)],m)

def sphere(name,p,s,m,detail=2):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=detail,radius=1,location=p)
    o=bpy.context.object;o.name=name;o.scale=s;o.data.materials.append(m)
    if m.name in ['skin','hair','yellow','cream','olive']:
        for f in o.data.polygons:f.use_smooth=True
    return o

def text(t,x,z,size=.25,m=None,y=-.95):
    d=bpy.data.curves.new('Lettering '+t,'FONT');d.body=t;d.size=size;d.align_x='CENTER';d.extrude=.001
    o=bpy.data.objects.new('Sign '+t,d);bpy.context.collection.objects.link(o);o.location=(x,y,z);o.rotation_euler=(math.pi/2,0,0);d.materials.append(m or M['cream']);return o

def pipe(points,r=.10,m=None):
    m=m or M['pipe']
    for a,b in zip(points,points[1:]):
        rod('Pipe section',a,b,r,m)
        sphere('Cast elbow',a,(r*1.13,)*3,m,1)
        aa,bb=Vector(a),Vector(b);v=(bb-aa).normalized()
        for t in [.12,.85]:
            q=aa+(bb-aa)*t;rod('Pipe collar',q-v*.055,q+v*.055,r*1.3,M['steel'])

def bolt(x,y,z):rod('Hex bolt',(x,y,z),(x,y-.04,z),.044,M['steel'],6)
def panel(x,z,w,h,m=None):
    box('Riveted cabinet',(x,-.20,z),(w,.65,h),m or M['pipe'],.08)
    box('Inset door',(x,-.555,z),(w*.83,.07,h*.8),M['dark'],.05)
    for xx in [-1,1]:
        for zz in [-1,1]:bolt(x+xx*w*.38,-.61,z+zz*h*.35)

def lamp(x,z,c=(1,.57,.19),power=70):
    rod('Lamp hanger',(x,.0,z+.30),(x,.0,z+.08),.045,M['steel'])
    box('Amber lamp lens',(x,-.22,z),(.23,.26,.35),M['lamp'],.06)
    for xx in [-.16,.16]:rod('Lantern cage',(x+xx,-.22,z-.23),(x+xx,-.22,z+.23),.025,M['brass'])
    for zz in [-.23,.23]:box('Lantern rim',(x,-.22,z+zz),(.38,.33,.07),M['brass'])
    lights.append([x,z,-1.7,*c,power,4.5])

def ladder(x,lo,hi):
    for xx in [x-.34,x+.34]:box('Ladder iron rail',(xx,-.75,(lo+hi)/2),(.085,.15,hi-lo+.1),M['brass'])
    for i in range(int((hi-lo)/.34)+1):
        z=lo+i*.34;box('Ladder timber rung',(x,-.86,z),(.79,.15,.095),M['woodlight'])
        for xx in [x-.3,x+.3]:bolt(xx,-.96,z)

def floor(x,z,w,m=None):
    for i in range(math.ceil(w/.9)):
        ww=min(.9,w-i*.9);xx=x-w/2+i*.9+ww/2
        box('Cut stone sill',(xx,-.05,z),(ww-.025,1.8,.43),m or M['sand'],.065)
        if i%2==0:box('Sill weathering',(xx,-.97,z+.04),(ww*.65,.016,.03),M['sanddark'],.005)

def room(x,z,w,h):
    box('Warm excavated backwall',(x,1.02,z+h/2),(w,.5,h),M['wall'],.04)
    for i in range(32):
        xx=x+random.uniform(-w/2,w/2);zz=z+random.uniform(.1,h-.1)
        box('Backwall mineral fleck',(xx,.747,zz),(random.uniform(.025,.10),.02,.024),M['sanddark'],0)
    floor(x,z,w);floor(x,z+h,w)
    for xx in [x-w/2,x+w/2]:
        box('Timber support',(xx,.37,z+h/2),(.18,.28,h),M['wood'])
        for zz in [z+.4,z+h-.35]:box('Iron support strap',(xx,.19,zz),(.24,.12,.13),M['steel'])
    box('Ceiling timber',(x,.4,z+h-.24),(w,.32,.19),M['woodlight'])

def crate(x,z,s=.65):
    box('Storage chest',(x,-.3,z+s/2),(s,.6,s),M['wood'],.045)
    for xx in [-.37,.37]:box('Chest band',(x+xx*s,-.62,z+s/2),(.075,.045,s),M['steel'])
    box('Chest lid',(x,-.3,z+s),(s*1.06,.65,.08),M['woodlight']);box('Latch',(x,-.66,z+s*.6),(.13,.08,.16),M['brass'])

def plant(x,z,s=.7):
    rod('Terracotta planter',(x,0,z),(x,0,z+.26*s),.19*s,M['clay'])
    for i in range(7):
        a=i*2.4;end=(x+math.sin(a)*.34*s,math.cos(a)*.2*s,z+(.45+random.random()*.48)*s)
        rod('Leaf stem',(x,0,z+.22*s),end,.023*s,M['green'])
        sphere('Broad leaf',end,(.12*s,.065*s,.24*s),M['leaf'],1)

def bed(x,z):
    for xx in [x-1.1,x+1.1]:
        for yy in [-.48,.4]:box('Bed frame leg',(xx,yy,z+.45),(.10,.11,.8),M['wood'])
    box('Mattress',(x,-.03,z+.53),(2.2,.96,.25),M['cream'],.13)
    box('Folded olive quilt',(x+.35,-.05,z+.70),(1.3,.98,.19),M['olive'],.09)
    sphere('Soft pillow',(x-.70,-.10,z+.72),(.42,.40,.17),M['cream'])
    for xx in [x-1.16,x+1.16]:box('Bed end',(xx,.0,z+.6),(.13,1.0,.7),M['woodlight'])

def table(x,z):
    box('Mess table',(x,-.1,z+.85),(3,.96,.14),M['woodlight'],.055)
    for xx in [x-1.2,x+1.2]:box('Table leg',(xx,0,z+.43),(.14,.55,.8),M['wood'])
    for xx in [x-1.12,x,x+1.12]:
        box('Stool seat',(xx,-.7,z+.4),(.55,.5,.12),M['woodlight'])
        for dx in [-.18,.18]:box('Stool leg',(xx+dx,-.7,z+.18),(.09,.36,.35),M['wood'])
    for xx in [x-.7,x+.65]:
        rod('Tin mug',(xx,-.25,z+.94),(xx,-.25,z+1.16),.09,M['cream'])
        rod('Mug handle',(xx+.07,-.25,z+1.03),(xx+.16,-.25,z+1.08),.033,M['brass'])

def person(x,z,pose='carry',hat='yellow',flip=1):
    # Large expressive head, small practical body; independent static meshes.
    before=set(bpy.context.scene.objects)
    y=-.72; skin=M['skin']; cloth=M['yellow']; dark=M['dark']; hair=M['hair']
    for dx,dz in [(-.18,0),(.18,.035 if pose=='climb' else 0)]:
        rod('Trouser leg',(x+dx,y,z+.56),(x+dx+flip*.06,y-.02,z+.16+dz),.105,dark)
        box('Work boot',(x+dx+flip*.04,y-.08,z+.10+dz),(.29,.39,.18),dark,.07)
    sphere('Work jacket',(x,y,z+.81),(.32,.23,.42),cloth)
    for dx in [-.14,.14]:box('Overall braces',(x+dx,y-.22,z+.86),(.055,.04,.53),M['steel'],.015)
    box('Utility belt',(x,y-.01,z+.58),(.61,.46,.08),M['wood'])
    box('Belt buckle',(x,y-.26,z+.59),(.12,.06,.11),M['brass'])
    sphere('Hair silhouette',(x,y+.025,z+1.35),(.41,.31,.43),hair)
    sphere('Vaulter face',(x+flip*.045,y-.14,z+1.34),(.35,.23,.35),skin)
    sphere('Nose',(x+flip*.1,y-.385,z+1.31),(.08,.065,.08),skin)
    for dx in [-.12,.12]:
        sphere('Bright eye',(x+dx+flip*.025,y-.346,z+1.41),(.043,.030,.069),dark)
        sphere('Eye glint',(x+dx+flip*.013,y-.373,z+1.434),(.012,.01,.017),M['cream'],1)
        rod('Eyebrow',(x+dx-.045,y-.354,z+1.51),(x+dx+.047,y-.354,z+1.52),.020,hair)
    rod('Smile',(x-.04,y-.368,z+1.21),(x+.065,y-.368,z+1.20),.016,hair)
    hm=M[hat];sphere('Helmet dome',(x,y+.015,z+1.65),(.43,.34,.22),hm)
    box('Hard hat brim',(x,y-.12,z+1.57),(.91,.76,.07),hm,.065)
    rod('Helmet seam',(x,y-.28,z+1.69),(x,y+.2,z+1.79),.035,M['cream'])
    sphere('Helmet lamp',(x,y-.43,z+1.65),(.09,.05,.07),M['lamp'])
    box('Backpack',(x,y+.25,z+.85),(.45,.19,.55),M['olive'],.06)
    if pose=='carry':
        ends=[(x-.27,y-.52,z+.87),(x+.27,y-.52,z+.87)]
        crate(x,z+.58,.54)
        # Bring carried box forward of the body.
        for o in list(bpy.context.scene.objects)[-5:]:o.location.y-=1.0
    elif pose=='climb':ends=[(x-.3,y-.13,z+1.33),(x+.36,y-.14,z+1.04)]
    else:
        ends=[(x+flip*.55,y-.23,z+1.0),(x+flip*.64,y-.25,z+1.17)]
        a=(x+flip*.43,y-.26,z+.78);b=(x+flip*.91,y-.26,z+1.68)
        rod('Pickaxe handle',a,b,.045,M['woodlight']);rod('Forged pick head',(b[0]-.36,b[1],b[2]+.09),(b[0]+.31,b[1],b[2]-.13),.075,M['steel'])
    for dx,end in zip([-.26,.26],ends):
        elbow=((x+dx+end[0])/2,y-.20,z+.84)
        rod('Jacket sleeve',(x+dx,y,z+1.0),elbow,.11,cloth);rod('Forearm',elbow,end,.08,cloth);sphere('Glove',end,(.10,.09,.10),skin)

    bpy.context.view_layer.update()
    pivot=Vector((x,y,z));angle=math.pi if pose=='climb' else .28*flip
    transform=Matrix.Translation(pivot) @ Matrix.Rotation(angle,4,'Z') @ Matrix.Translation(-pivot)
    for ob in set(bpy.context.scene.objects)-before:ob.matrix_world=transform @ ob.matrix_world

def shelf(x,z):
    box('Pantry shelving',(x,.48,z),(1.35,.36,.11),M['woodlight'])
    for dx in [-.50,-.17,.16,.48]:
        rod('Preserving jar',(x+dx,.36,z+.08),(x+dx,.36,z+.34),.105,M['cream'])
        rod('Jar brass lid',(x+dx,.36,z+.34),(x+dx,.36,z+.39),.115,M['brass'])
        box('Jar label',(x+dx,.235,z+.20),(.13,.02,.11),M['paper'],0)
    for dx in [-.46,.46]:rod('Shelf bracket',(x+dx,.65,z-.3),(x+dx,.30,z-.04),.025,M['steel'])

def machine(x,z,kind='battery'):
    panel(x,z+.85,1.1,1.55)
    if kind=='pump':
        text('H2O',x,z+.75,.23,M['cyan'],y=-.61)
        pipe([(x-.65,.0,z+.2),(x-.85,.0,z+.2),(x-.85,.0,z+1.8),(x+.1,.0,z+1.8)])
    else:
        text('POWER',x,z+.8,.17,M['yellow'],y=-.61)
        for xx in [x-.64,x+.64]:rod('Battery cell',(xx,0,z+.24),(xx,0,z+1.38),.14,M['yellow'])
    for xx in [-.22,0,.22]:box('Status lamp',(x+xx,-.62,z+1.42),(.12,.06,.065),M['lamp'])
    box('Machine plinth',(x,0,z+.08),(1.6,.9,.16),M['steel'])

def algae(x,z):
    # Open front glass ribs expose modelled algae; no opaque cylinder hiding it.
    box('Algae liquid volume',(x,.13,z+1.9),(1.75,.65,2.45),M['teal'],.18)
    for xx in [-.9,.9]:rod('Luminous vessel edge',(x+xx,-.35,z+.7),(x+xx,-.35,z+3.12),.038,M['cyan'])
    for zz in [.6,3.18]:
        rod('Pressure vessel cap',(x,0,z+zz-.12),(x,0,z+zz+.12),1.06,M['steel'],24)
        box('Cyan gauge',(x,-1.02,z+zz),(.58,.07,.08),M['cyan'])
    for i in range(11):
        xx=x+random.uniform(-.7,.7);last=(xx,-.30,z+.75)
        for j in range(1,7):
            p=(xx+math.sin(j*.85+i)*.17,-.36,z+.75+j*.32);rod('Algae frond',last,p,.065,M['leaf']);last=p
    panel(x,z+.35,1.45,.70);text('O2',x,z+.20,.36,M['cyan'],y=-.62)
    pipe([(x-1.2,.22,z+.3),(x-1.45,.22,z+.3),(x-1.45,.22,z+3.9),(x-.5,.22,z+3.9),(x-.5,.22,z+3.35)],.13)
    pipe([(x+.5,.22,z+3.4),(x+1.35,.22,z+3.4),(x+1.35,.22,z+.4),(x+1.7,.22,z+.4)],.12)
    lights.append([x,z+2,-1.5,.05,.9,.83,70,4.5])

def wheel(x,z):
    for i in range(32):
        a=i*math.tau/32;b=(i+1)*math.tau/32
        rod('Crank wheel rim',(x+math.cos(a)*1.3,-.5,z+1.55+math.sin(a)*1.3),(x+math.cos(b)*1.3,-.5,z+1.55+math.sin(b)*1.3),.16,M['woodlight'])
    for i in range(10):
        a=i*math.tau/10;rod('Wheel spoke',(x,-.5,z+1.55),(x+math.cos(a)*1.2,-.5,z+1.55+math.sin(a)*1.2),.095,M['wood'])
        bolt(x+math.cos(a)*1.28,-.68,z+1.55+math.sin(a)*1.28)
    rod('Crank hub',(x,.1,z+1.55),(x,-.9,z+1.55),.24,M['steel'])
    for xx in [x-.72,x+.72]:rod('Wheel support',(xx,0,z+.1),(x,0,z+1.55),.13,M['steel'])
    panel(x,z+.30,1.15,.6);text('120 W',x,z+.23,.18,M['yellow'],y=-.62)

def rock(x,z,s=1,m=None):
    mm=m or random.choice(rocks)
    o=box('Weathered fractured rock',(x,.12,z),(s*1.05,1.7,s*1.04),mm,.085)
    # Jitter the block corners without turning every rock into a regular crystal.
    for v in o.data.vertices:
        v.co.x+=random.uniform(-.14,.14)*s;v.co.z+=random.uniform(-.12,.12)*s
        v.co.y+=random.uniform(-.10,.10)
    return o

def geology(holes,biomes=False):
    box('Deep vault backing',(0,2.0,6),(30,.6,18),M['black'],0)
    for row in range(-2,16):
        for col in range(-16,17):
            x=col*.85+(row%2)*.35;z=row*.83
            if any(a-.18<x<b+.18 and c-.15<z<d+.15 for a,b,c,d in holes):continue
            mm=None
            if biomes and x<-9:mm=random.choice([M['ice'],M['blue'],M['steel']])
            if biomes and x>9:mm=random.choice([M['lava'],M['rust'],M['black']]) if random.random()<.22 else None
            rock(x+random.uniform(-.13,.13),z+random.uniform(-.1,.1),random.uniform(.85,1.2),mm)
    if biomes:
        for x,col in [(-10.7,M['cyan']),(11.1,M['lava'])]:
            for i in range(22):
                z=random.uniform(1,12);sphere('Biome outcrop',(x+random.uniform(-1.6,1.6),-.65,z),(.21,.24,random.uniform(.4,1.1)),col,1)
        lights.extend([[-10,6,-2,.08,.48,1,150,8],[11,6,-2,1,.12,.01,180,8]])

def pool(x,z,w,h):
    box('Water cutaway',(x,.13,z+h/2),(w,1.0,h),M['water'],.04)
    box('Water surface',(x,-.15,z+h),(w,1.45,.045),M['teal'],.02)
    for i in range(int(w*15)):
        xx=x+random.uniform(-w/2,w/2);zz=z+h+random.uniform(-.025,.025)
        rod('Surface glint',(xx,-.95,zz),(xx+random.uniform(.03,.16),-.95,zz+.01),.012,M['cyan'],6)
    for i in range(int(w*9)):
        xx=x+random.uniform(-w/2,w/2);zz=z+random.uniform(.1,h)
        sphere('Water bubble',(xx,-.42,zz),(.024,.02,.03),M['foam'],1)
    lights.append([x,z+h,-1.6,.02,.51,.75,45,w])

def waterfall(x,z,drop,width=.3):
    for i in range(14):
        xx=x+random.uniform(-width,width);last=(xx,-.7,z)
        for j in range(1,9):
            t=j/8;p=(xx+t*t*.65,-.7-random.uniform(0,.14),z-drop*t)
            rod('Falling water strand',last,p,random.uniform(.018,.046),M['foam'] if i%3==0 else M['cyan'],6);last=p
    for i in range(35):
        sphere('Splash drop',(x+random.uniform(-.4,.95),-random.uniform(.6,1),z-drop+random.random()*.5),(.027,.026,.043),M['foam'],1)

def sign(x,z,t,w=1.25,h=1.6):
    box('Colony poster',(x,.60,z),(w,.045,h),M['paper'],.02)
    for i,line in enumerate(t.split('|')):text(line,x,z+h*.30-i*.27,.21,M['wood'],y=.57)

def designation(x,z,color):
    for i in range(5):
        off=-.65+i*.29
        for zz in [z-.65,z+.65]:box('Dashed designation',(x+off,-1.05,zz),(.18,.035,.028),color,0)
        for xx in [x-.65,x+.65]:box('Dashed designation',(xx,-1.05,z+off),(.028,.035,.18),color,0)
    rod('Designated pick handle',(x-.18,-1.09,z-.25),(x+.2,-1.09,z+.25),.035,color)
    rod('Designated pick head',(x-.05,-1.09,z+.30),(x+.39,-1.09,z+.08),.045,color)

for name,c,metal,emit in [
('black',(.035,.042,.055),0,0),('dark',(.07,.10,.12),.35,0),('pipe',(.10,.20,.22),.6,0),('steel',(.31,.37,.38),.65,0),
('sand',(.49,.34,.17),0,0),('sanddark',(.25,.17,.09),0,0),('wall',(.33,.24,.135),0,0),('wood',(.22,.12,.05),0,0),('woodlight',(.48,.28,.095),0,0),
('brass',(.51,.34,.14),.55,0),('yellow',(.88,.56,.08),.15,0),('cream',(.80,.76,.57),0,0),('olive',(.30,.36,.12),0,0),('clay',(.44,.24,.10),0,0),
('green',(.12,.26,.07),0,0),('leaf',(.28,.54,.10),0,0),('teal',(.025,.34,.37),.3,.14),('cyan',(.12,.82,.89),.1,1.1),('water',(.017,.16,.23),.25,.09),
('foam',(.47,.87,.93),.1,.65),('lamp',(1,.66,.20),0,3),('skin',(.72,.43,.22),0,0),('hair',(.065,.035,.021),0,0),('blue',(.08,.24,.45),.2,0),
('ice',(.12,.40,.56),.15,0),('lava',(1,.12,.006),0,2),('rust',(.29,.09,.027),.3,0),('paper',(.57,.46,.27),0,0),('red',(.96,.19,.08),0,1)]:mat(name,c,metal,emit)
rocks=[mat('Basalt facet '+str(i),c) for i,c in enumerate([(.065,.075,.09),(.10,.11,.13),(.13,.135,.14),(.09,.10,.12),(.15,.14,.125)])]
views=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['Colony','Planning','Flooding']
for view in views:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);random.seed(1209);lights=[]
    if view=='Colony':
        holes=[(-10,-8,1.6,13),(-7.4,-.4,3,10.4),(-.1,5.6,4.5,10.5),(5.7,12.5,4.5,8.6),(1.9,7.1,1.4,4.5)]
        geology(holes);room(-3.9,6.9,6.6,3.3);room(-3.9,3.3,6.6,3.2);room(2.8,4.9,5.1,5.3);room(9,4.9,6,3.4)
        ladder(-8.8,1.8,12.7);floor(-9,1.7,3)
        shelf(-6,5.18);shelf(-6,4.55);bed(-4.9,7.14);crate(-1.5,7.13,.8);plant(-1.5,8,.75);sign(-2.8,9.05,'A DEEPER|TOMORROW',1.55,1.25)
        table(-3.4,3.55);sign(-1.3,5.40,'SMALL|PEOPLE|BIG DEPTHS',1.3,1.6)
        for x in [-5.3,-4.6]:plant(x,5.1,.65)
        box('Shelf',(-4.9,.45,5.04),(1.6,.5,.12),M['woodlight']);crate(-6,3.55,.5)
        shelf(.6,5.5);algae(2.2,5.13);wheel(9.8,5.15);person(-9.55,1.95,'dig',flip=-1);person(-6.0,3.55,'carry','steel');person(6.7,5.15,'dig','cream')
        pool(4.65,1.7,4.3,1.4);pipe([(3.6,0,5.2),(3.6,0,4.5),(4,0,4.5)],.14);waterfall(4,4.5,1.4,.12)
        for x,z in [(-9.7,10.1),(-6.8,9.25),(-.8,9.55),(-6.5,5.35),(-2.4,4.65),(6.1,7.7),(8.2,6.1),(11.4,7.8)]:lamp(x,z)
        for i in range(15):rock(-10.5+random.uniform(-.6,.3),2.0+random.random()*.55,random.uniform(.18,.4),M['sand'])
        for x,z in [(-11.8,6.5),(10.7,1.1)]:
            for i in range(6):sphere('Teal cave crystal',(x+random.uniform(-.6,.6),-.6,z),(.13,.2,random.uniform(.3,.7)),M['cyan'],1)
    elif view=='Planning':
        holes=[(-8.5,8.3,4.2,11),(-.7,.7,1.5,11.4),(1,6.9,1.3,4.1),(-4.5,-1,2.4,4.1)]
        geology(holes,True);room(-4.5,7.55,7.1,3.1);room(4.4,7.55,7.1,3.1);room(-4.5,4.5,7.1,2.65);room(4.4,4.5,7.1,2.65)
        ladder(0,1.5,11.3);bed(-6.2,7.8);crate(-4.4,7.8,.8);sign(-2.5,9.2,'SMALL DIGS|BRIGHTER|DAYS',1.4,1.75)
        machine(4.5,7.8);panel(1.6,8.4,1.2,1.2);text('RESEARCH',1.6,8.4,.14,M['cyan'],y=-.61)
        pipe([(4.6,.3,9.8),(4.6,.3,10.2),(7.0,.3,10.2),(7.0,.3,8.0)],.075)
        pool(-6.1,4.75,2.8,1.45);machine(-3.4,4.75,'pump');pipe([(-3.4,.3,6.3),(-1.2,.3,6.3),(-1.2,.3,4.9),(2,.3,4.9)],.07)
        for x in [2.1,3.6,5.5]:crate(x,4.75,.7)
        plant(3.3,5.5,1.2);person(2.3,7.8,'carry','blue');person(6.8,4.75,'dig');person(-2.5,4.75,'carry','steel')
        for x in [-7.8,-6.3,-4.8]:designation(x,3.5,M['yellow'])
        designation(-6.3,2,M['yellow'])
        for x,z in [(8.4,6.2),(9.9,6.2),(8.4,2.3),(9.9,2.3)]:designation(x,z,M['red'])
        # A wireframe 3D pump and plumbing route: no fullscreen world overlay.
        for xx in [-3.9,-2.6]:
            for zz in [2.6,3.9]:rod('Build ghost horizontal',(-3.9,-1,zz),(-2.6,-1,zz),.025,M['cyan'])
            rod('Build ghost upright',(xx,-1,2.6),(xx,-1,3.9),.025,M['cyan'])
        pipe([(-3.3,-.9,3.9),(-3.3,-.9,4.12),(-1.2,-.9,4.12),(-1.2,-.9,3),(1,-.9,3)],.037,M['cyan'])
        text('BREATHABLE AIR',3.7,3.65,.25,M['cyan']);text('21% O2',3.7,3.28,.23,M['cyan']);text('CO2',3.5,1.85,.3,M['olive'])
        # Gas pocket is a Flux GPU mist emitter, authored by the engine bootstrap.
        for x,z in [(-6.8,10.15),(-3.7,10.15),(1.5,10.15),(6.7,10.15),(-7.2,6.8),(2.1,6.8),(6.6,6.8),(.8,3.7)]:lamp(x,z)
    else:
        holes=[(-8.3,12.7,7.5,11.4),(-11.7,-1.4,1.7,7.1),(2.0,3.9,1.7,7.5),(4.1,11.8,4.0,6.6),(4.1,12,1.7,3.8)]
        geology(holes);room(-5.4,7.8,5.8,3.2);room(-.6,7.8,3.4,3.2);room(8.9,7.8,7.1,3.2)
        shelf(-.25,9.6);bed(-6.8,8.05);bed(-4.1,8.05);plant(-7.8,8.7,.8);table(-.6,8.05);sign(-4.5,9.8,'SAFER|DEEPER|TOMORROW',1.3,1.7)
        panel(3,9.4,2.0,2.7);text('AIRLOCK',3,9.9,.28,M['cream'],y=-.62);text('SEALED',3,9.4,.23,M['leaf'],y=-.62)
        for i in range(8):box('Airlock caution stripe',(2.1+i*.26,-.66,8.17),(.14,.05,.18),M['yellow'],0)
        rod('Generator drum',(8.5,-.15,8.8),(10.8,-.15,8.8),.51,M['steel'],24)
        for x in [8.5,8.85,10.4,10.75]:rod('Generator hoop',(x-.06,-.15,8.8),(x+.06,-.15,8.8),.57,M['yellow'],24)
        for i in range(9):rod('Cooling fin',(9+i*.14,-.15,8.8),(9.04+i*.14,-.15,8.8),.53,M['dark'],24)
        box('Generator foot',(9.6,0,8.14),(2.8,.95,.2),M['woodlight']);sign(11.5,9.8,'STRONGER|PEOPLE|DEEPER HOMES',1.35,1.65)
        ladder(3,1.9,7.75);ladder(7.7,2,4.6);floor(8.1,4.4,7.5)
        box('Breach backwall',(-6.7,1,4.5),(9,.5,5.6),M['pipe'])
        for zz in [2.1,6.7]:pipe([(-11,.6,zz),(-2,.6,zz)],.14)
        for xx in [-10,-8.8,-3]:pipe([(xx,.55,2),(xx,.55,6.7)],.11)
        pipe([(-11,-.1,5.6),(-7,-.1,5.6)],.46,M['rust'])
        rod('Broken water main mouth',(-7.1,-.1,5.6),(-6.9,-.1,5.6),.60,M['steel'])
        rod('Dark pipe opening',(-6.88,-.1,5.6),(-6.86,-.1,5.6),.41,M['black'])
        sign(-4.3,5.5,'WATER|MAIN B-3',1.9,1.8);sign(9.5,5.5,'ESCAPE|CHANNEL',2.2,1.3)
        person(-1.0,8.05,'carry','steel');person(7.5,8.05,'dig','cream');person(3,5.45,'climb');person(3,3.4,'climb','blue');person(7.3,4.65,'dig','cream')
        pool(-5.2,1.9,12.1,2.15);pool(7.1,1.9,8.2,2.15)
        # Silhouettes visible through the staged blue water cross-section.
        pipe([(-10,-.7,2.3),(-8.5,-.7,2.3),(-8.5,-.7,3.6),(-6.9,-.7,3.6)],.15)
        pipe([(-5,-.7,2.5),(-3,-.7,2.5),(-3,-.7,3.6)],.16)
        for xx in [-7.6,-5.3,9.4]:
            box('Submerged storage',(xx,-.7,2.38),(.85,.3,.78),M['pipe'])
            for dx in [-.34,.34]:box('Submerged crate strap',(xx+dx,-.87,2.38),(.055,.035,.74),M['teal'])
        waterfall(-6.8,5.65,1.62,.43)
        for x,z in [(-7.9,10.3),(-4.3,10.45),(-.4,10.3),(3,11),(6.5,10.45),(10.8,10.45),(6.5,6.25),(10.6,6.15)]:lamp(x,z)
        text('CO2',-.3,5.9,.52,M['steel']);text('O2',.0,2.9,.5,M['cyan']);text('THIN 12%',0,2.5,.24,M['cyan'])
    # Consistent physical-scale triplanar UVs, baked into editable meshes.
    for ob in bpy.context.scene.objects:
        if ob.type!='MESH':continue
        uv=ob.data.uv_layers.active or ob.data.uv_layers.new(name='UVMap')
        for face in ob.data.polygons:
            axis=max(range(3),key=lambda a:abs(face.normal[a]));axes=[a for a in range(3) if a!=axis]
            for li in face.loop_indices:
                co=ob.matrix_world @ ob.data.vertices[ob.data.loops[li].vertex_index].co
                uv.data[li].uv=(co[axes[0]]*.75,co[axes[1]]*.75)
    # Preserve individually editable kit pieces in the blend; export a joined copy.
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt'/('Undervault'+view+'.blend')))
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.convert(target='MESH')
    bpy.context.view_layer.objects.active=next(iter(bpy.context.scene.objects));bpy.ops.object.join();bpy.context.object.name=view
    # Zenith views +Z; reflect source depth so authored fronts face the camera.
    for vertex in bpy.context.object.data.vertices: vertex.co.y = -vertex.co.y
    bpy.context.object.data.flip_normals()
    bpy.ops.export_scene.gltf(filepath=str(OUT/(view+'.glb')),export_format='GLB',use_selection=True,export_animations=False,export_yup=True)
    # Light locations are retained as source-art metadata for the matching bootstrap.
    (OUT/(view+'Lights.json')).write_text(json.dumps(lights))
    print('UNDERVAULT_EXPORT_COMPLETE',view,len(lights),flush=True)


import runpy
runpy.run_path(str(ROOT/'SourceArt/generate_mist.py'))
