"""Two static Foundry reference studies. Requires the approved workshop blend.
Run using Blender --background --python this_file.py. No workshop outputs changed.
"""
import ast, math, random, sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Assets/Workshop'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'SourceArt/FoundryWorkshop.blend'))
scene=bpy.context.scene
kit={o.name:o for o in scene.objects if o.name!='Workshop'}
bpy.data.objects.remove(bpy.data.objects['Workshop'],do_unlink=True)
source=ast.parse((ROOT/'SourceArt/generate_workshop.py').read_text())
names=['material','mesh','finish','box','cylinder','rod','stone','place','rounded_path','ground_uv']
exec(compile(ast.Module(body=[n for n in source.body if isinstance(n,ast.FunctionDef) and n.name in names],type_ignores=[]),'workshop helpers','exec'))
for symbol,name in {'Y':'MK1 golden enamel','Y2':'Enamel edge highlights','D':'Carbon steel','S':'Machined alloy','P':'Porcelain trim','W':'Deep teal water','SAND':'Sandstone shore','HOT':'Furnace coals','RED':'Vermilion pivot caps','GROUND':'Mottled meadow','RUBBER':'Conveyor rubber'}.items():
    globals()[symbol]=bpy.data.materials[name]
BLUE=material('Electric blue belt guides',(.025,.35,.70),.3,.35,(.01,.32,.8))
GREEN=material('Radar phosphor',(.08,.48,.29),.15,.55,(.02,.22,.10))
SOLAR=material('Photovoltaic blue',(.025,.12,.25),.6,.25)
CONCRETE=material('Factory concrete',(.24,.27,.24),0,.95)
ENEMY=material('Native carapace',(.22,.055,.032),.25,.62)
YELLOW=material('Sulphur uranium seams',(.58,.58,.055),.15,.8)
COAL=material('Anthracite seam',(.045,.055,.055),.25,.75)
IRONBLUE=material('Iron deposit blue',(.13,.33,.48),.2,.7)

# Direct meshes for repeated details avoid expensive Blender operators.
def block(name,p,size,m):
    x,y,z=p;a,b,c=[v/2 for v in size]
    return mesh(name,[(x+dx*a,y+dy*b,z+dz*c) for dx,dy,dz in
        [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]],
        [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],[m])

def rod(name,a,b,radius,m,sides=8):
    a,b=Vector(a),Vector(b);axis=(b-a).normalized()
    u=axis.cross(Vector((0,0,1)) if abs(axis.z)<.9 else Vector((0,1,0))).normalized()
    v=axis.cross(u);verts=[]
    for p in [a,b]:
        for i in range(sides):
            q=p+radius*(u*math.cos(i*math.tau/sides)+v*math.sin(i*math.tau/sides));verts.append(tuple(q))
    return mesh(name,verts,[tuple(reversed(range(sides))),tuple(range(sides,2*sides))]+
        [(i,(i+1)%sides,(i+1)%sides+sides,i+sides) for i in range(sides)],[m])

def strip(name,points,width,z,m):
    pts=[Vector(p) for p in points];verts=[]
    for i,p in enumerate(pts):
        tangent=pts[min(i+1,len(pts)-1)]-pts[max(0,i-1)]
        tangent.normalize();n=Vector((-tangent.y,tangent.x))*width/2
        verts.extend([(p.x-n.x,p.y-n.y,z),(p.x+n.x,p.y+n.y,z)])
    return mesh(name,verts,[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(pts)-1)],[m])

def line(name,pts,width,z,m):return strip(name,[(p.x,p.y) for p in rounded_path(pts)],width,z,m)

def belt(points,item='PlateItem',highlight=False):
    pts=rounded_path(points);xy=[(p.x,p.y) for p in pts]
    strip('Belt chassis',xy,.72,.18,D);strip('Belt safety edging',xy,.64,.25,BLUE if highlight else Y2)
    strip('Moving surface static',xy,.52,.27,RUBBER)
    distance=0;next_item=.3;next_tread=0
    for a,b in zip(pts,pts[1:]):
        distance+=(b-a).length;ang=math.atan2(b.y-a.y,b.x-a.x)
        if distance>=next_tread:
            ob=block('Belt slat',(0,0,.285),(.035,.51,.022),S)
            ob.location.x=b.x;ob.location.y=b.y;ob.rotation_euler.z=ang;next_tread+=.22
        if distance>=next_item:
            place(item,b.x,b.y,.30,scale=.65,yaw=ang);next_item+=.65

def ring(x,y,r,m,z=.045,dashed=False):
    for i in range(96):
        if dashed and i%3==2:continue
        a=i*math.tau/96;b=(i+1)*math.tau/96
        strip('Survey radius',[(x+r*math.cos(a),y+r*math.sin(a)),(x+r*math.cos(b),y+r*math.sin(b))],.045,z,m)

def slab(x,y,w,h):block('Service foundation',(x,y,.035),(w,h,.07),CONCRETE)

def tower(x,y,scale=1):
    for dx,dy in [(-.45,-.45),(.45,-.45),(.45,.45),(-.45,.45)]:
        rod('Pylon leg',(x+dx,y+dy,0),(x,y,3.8*scale),.045,S)
        rod('Lattice diagonal',(x+dx,y+dy,.4),(x-dx*.5,y-dy*.5,2.5*scale),.025,S)
    rod('Power crossarm',(x-.8,y,2.7*scale),(x+.8,y,2.7*scale),.04,S)
    ring(x,y,3,BLUE)

def tank(x,y,scale=1):
    cylinder('Tank plinth',(x,y,.12),.65*scale,.24,D)
    cylinder('Pressure vessel',(x,y,1.4*scale),.52*scale,2.5*scale,S)
    for z in [.3,1.2,2.4]:cylinder('Tank band',(x,y,z*scale),.55*scale,.06,D)
    cylinder('Vessel cap',(x,y,2.68*scale),.4*scale,.12,P)
    block('Level gauge',(x,y+.53*scale,1.1*scale),(.15,.06,1.1*scale),BLUE)
    for z in [.35,.65,.95,1.25,1.55,1.85,2.15]:
        rod('Tank ladder rung',(x-.15*scale,y-.57*scale,z*scale),(x+.15*scale,y-.57*scale,z*scale),.025,S)
    for dx in [-.18,.18]:rod('Ladder rail',(x+dx*scale,y-.57*scale,.2),(x+dx*scale,y-.57*scale,2.4*scale),.027,D)

def assembler(x,y,scale=1,selected=False):
    slab(x,y,2.8*scale,2.6*scale)
    box('Assembler alloy housing',(x,y,.82*scale),(2.2*scale,1.9*scale,1.6*scale),S,.08)
    block('Machine front recess',(x,y+.96*scale,.70*scale),(1.55*scale,.025,1.05*scale),D)
    block('Assembly conveyor throat',(x,y+1.08*scale,.29*scale),(1.10*scale,.55*scale,.20*scale),Y)
    block('Assembly roof well',(x,y,1.64*scale),(1.55*scale,1.3*scale,.09),D)
    for dx in [-.90,.90]:
        for dy in [-.75,.75]:
            block('Assembler yellow pillar',(x+dx*scale,y+dy*scale,.9*scale),(.16,.18,1.85*scale),Y2)
    cylinder('Assembly spindle',(x,y,1.85*scale),.36*scale,.35,S)
    rod('Assembly crossbar',(x-.85*scale,y,2.10*scale),(x+.85*scale,y,2.10*scale),.075,Y)
    block('Circuit status panel',(x+.70*scale,y+.98*scale,1.18*scale),(.25,.04,.20),GREEN)
    for dx in [-.52,-.26,0,.26,.52]:block('Vent grille',(x+dx*scale,y-.98*scale,.95*scale),(.09,.04,.55*scale),D)
    for dx in [-.78,.78]:
        for dy in [-.68,.68]:
            rod('Machine crown bolt',(x+dx*scale,y+dy*scale,1.65*scale),(x+dx*scale,y+dy*scale,1.72*scale),.048,P,6)
    block('Electrical side pod',(x+1.22*scale,y,.6*scale),(.35*scale,.9*scale,.95*scale),D)
    block('Side access hatch',(x+1.4*scale,y,.7*scale),(.035,.6*scale,.6*scale),Y)
    rod('Hydraulic service line',(x-1.15*scale,y-.6*scale,.3),(x-1.15*scale,y+.7*scale,.3),.065,Y)
    for z in [.6,.9,1.2]:block('Front rib',(x-.78*scale,y+.99*scale,z*scale),(.13,.08,.08),P)
    if selected:
        line('Selected assembler',[ (x-1.5,y-1.5),(x+1.5,y-1.5),(x+1.5,y+1.5),(x-1.5,y+1.5),(x-1.5,y-1.5)],.085,.08,BLUE)

def rail(points):
    pts=rounded_path(points,radius=2.4);xy=[(p.x,p.y) for p in pts]
    strip('Ballast',xy,1.30,.06,CONCRETE)
    for offset in [-.43,.43]:
        vv=[]
        for i,p in enumerate(pts):
            d=pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)];d.normalize()
            vv.append((p.x-d.y*offset,p.y+d.x*offset))
        strip('Steel railway',vv,.07,.14,S)
    dist=0;nxt=0
    for a,b in zip(pts,pts[1:]):
        dist+=(b-a).length
        if dist<nxt:continue
        ob=block('Rail sleeper',(0,0,.095),(.13,1.18,.06),D)
        ob.location.x=b.x;ob.location.y=b.y;ob.rotation_euler.z=math.atan2(b.y-a.y,b.x-a.x);nxt+=.42

def car(x,y,m,engine=False):
    box('Rail chassis',(x,y,.40),(2.1,.93,.32),D,.04)
    box('Freight container',(x,y,.95),(1.9,.84,.83),m,.045)
    for dx in [-.72,.72]:
        for dy in [-.5,.5]:
            ob=cylinder('Train wheel',(x+dx,y+dy,.28),.22,.12,D);ob.rotation_euler.x=math.pi/2
    for dx in [-.7,-.35,0,.35,.7]:block('Container ribs',(x+dx,y,.99),(.035,.87,.77),S)
    if engine:
        box('Locomotive cab',(x-.5,y,1.40),(.7,.89,.6),P,.06)
        block('Cab windscreen',(x-.86,y,1.44),(.025,.68,.30),GLASS)

GLASS=bpy.data.materials['Blue laboratory glazing']
def landscape(extent,variant):
    ob=block('Meadow',(0,0,-.16),(extent*2+30,extent*2+30,.3),GROUND);ground_uv(ob)
    # Meandering river at the western edge leaves the factory clear.
    path=[(-extent*.75+math.sin(y*.19)*2,y) for y in range(-extent,extent+1)]
    strip('Riverbank',path,3.8,-.005,SAND);strip('River',path,2.8,.012,W)
    if variant=='MapDefense':
        branch=[(x,16+3*math.sin(x*.15)) for x in range(-extent,extent+1)]
        strip('Southern riverbank',branch,3.4,.004,SAND);strip('Southern river',branch,2.5,.018,W)
    for i in range(1050 if variant=='Logistics' else 3400):
        x=random.uniform(-extent,extent);y=random.uniform(-extent,extent)
        if (abs(x)<13 and abs(y)<10) or (variant=='MapDefense' and abs(x)<17 and abs(y)<13) or (variant=='MapDefense' and x>10 and abs(y)<12):continue
        if math.sin(x*.52)+math.cos(y*.41)<.0:continue
        if abs(x-(-extent*.75+math.sin(y*.19)*2))<2:continue
        if variant=='MapDefense' and abs(y-(16+3*math.sin(x*.15)))<2:continue
        if variant=='MapDefense' and ((abs(x+16)<1.4 and -11<y<10) or (abs(y-12)<1.3 and -13<x<13)):continue
        place('Pine',x,y,scale=random.uniform(.35,.9),yaw=random.random()*6.28)
    for cx,cy in [(-extent*.84,-15),(-extent*.90,10),(extent*.84,-17),(-3,-extent*.85),(9,extent*.86)]:
        for i in range(8):
            place('Rock',cx+random.uniform(-2.4,2.4),cy+random.uniform(-2,2),-.1,
                  scale=(random.uniform(1.8,3.6),random.uniform(1.5,2.7),random.uniform(2,4.8)))
    for i in range(180):
        x=random.uniform(-extent,extent);y=random.uniform(-extent,extent)
        if abs(x)<12 and abs(y)<9:continue
        place('Rock' if i%4==0 else 'Bush',x,y,scale=random.uniform(.2,.8))

def ore(x,y,kind,n=80):
    for i in range(n):
        angle=random.random()*math.tau;r=math.sqrt(random.random())*2.4
        ob=place(kind,x+math.cos(angle)*r,y+math.sin(angle)*r,-.02,scale=random.uniform(.22,.68))
        if kind=='CopperRock' and x<0 and y>12:
            ob.data.materials.clear();ob.data.materials.append(YELLOW)
        elif kind=='Rock' and x>0 and y<-10:
            ob.data.materials.clear();ob.data.materials.append(COAL)
        elif kind=='IronRock':
            ob.data.materials.clear();ob.data.materials.append(IRONBLUE)

def drone(x,y):
    cylinder('Drone body',(x,y,2.9),.23,.18,D)
    for dx,dy in [(-.4,-.4),(-.4,.4),(.4,-.4),(.4,.4)]:
        rod('Drone arm',(x,y,2.9),(x+dx,y+dy,2.9),.035,S)
        ring(x+dx,y+dy,.23,P,z=2.93)

def export(name):
    bpy.ops.object.select_all(action='DESELECT')
    objects=[o for o in scene.objects if o not in kit.values()]
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join()
    ob=bpy.context.object;ob.name=name
    bpy.ops.export_scene.gltf(filepath=str(OUT/(name+'.glb')),export_format='GLB',use_selection=True,export_animations=False,export_yup=True)
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt'/('Foundry'+name+'.blend')))
    bpy.data.objects.remove(ob,do_unlink=True)
    print('FOUNDRY_SCENE_COMPLETE',name,flush=True)

if '--defense-only' not in sys.argv:
    random.seed(2202)
    landscape(25,'Logistics')
    ore(-13,-8,'IronRock');ore(-13,6,'CopperRock');ore(12,7,'Rock')
    for x,y in [(-11,-8),(-11,6),(11,7)]:place('MiningDrill',x,y,scale=.85)
    for x,y in [(-4,-3),(0,-3),(4,-3),(-3,2),(5,2),(0,6)]:assembler(x,y,.83,selected=(x==5))
    for x,y in [(-3,-6),(-1,-6),(1,-6)]:tank(x,y,1.2 if x==-1 else .9)
    for x in [-3,-1,1]:
        rod('Tank outlet',(x,-6,.5),(x,-4.5,.5),.13,S)
        cylinder('Valve wheel',(x,-5.2,.72),.20,.05,RED)
    rod('Process manifold',(-3,-4.5,.5),(4,-4.5,.5),.13,Y)
    for x in [-4,0,4]:
        rod('Plant inlet',(x,-4.5,.5),(x,-3.8,.5),.10,S)
    place('Furnace',0,-.4,scale=.72)
    # Auxiliary process equipment fills the plant rather than six isolated machines.
    for x,y in [(-5.4,-5.1),(2.7,-5.7),(-5.1,2.5),(1.4,5.9)]:
        slab(x,y,1.1,1.1)
        block('Control cabinet',(x,y,.55),(.8,.8,1.0),D)
        block('Yellow cabinet trim',(x,y+.42,.57),(.65,.055,.75),Y)
        rod('Cabinet vent',(x,y,.98),(x,y,1.19),.25,S,12)
    for x in [-4,0,4]:place('Inserter',x+1.3,-2.8,scale=.7,yaw=math.pi)
    # Guide lights and legs strengthen the cyan construction preview.
    line('Preview ghost',[(9.4,5.4),(10.4,5.4),(10.4,6.4),(9.4,6.4),(9.4,5.4)],.10,.35,BLUE)
    for points,item,lit in [([(-10,-8),(-7,-8),(-7,-3),(-5,-3)],'IronItem',False),
        ([(-10,6),(-7,6),(-7,2),(-4,2)],'CopperItem',False),
        ([(-4,-2),(-4,0),(1,0),(1,2),(4,2)],'PlateItem',True),
        ([(-3,3),(-3,4),(3,4),(3,3),(4,3)],'PlateItem',True),
        ([(6,2),(8,2),(8,6),(10,6)],'PlateItem',True),
        ([(10,7),(9,7),(9,9),(0,9),(0,7)],'IronItem',False),
        ([(4,-2),(4,0),(8,0),(8,-5)],'CopperItem',False)]:belt(points,item,lit)
    for x,y in [(-8,-1),(-3,8),(-1,-11)]:tower(x,y)
    for x,y in [(6,-7),(1,-8),(9,1)]:drone(x,y)
    rail([(4,-14),(17,-14),(17,-6),(9,-6),(6,-9),(6,-14),(4,-14)])
    rail([(17,-6),(20,-3),(25,0)])
    for i,m in enumerate([P,RED,BLUE,Y]):car(8+i*2.3,-14,m,engine=(i==0))
    slab(14,-6,6,2.9)
    for x in [12,14,16]:place('StorageCrate',x,-6,scale=.65)
    box('Station canopy',(14,-6,2),(6,2,.15),S,.05)
    for x in [11.3,16.7]:block('Station support',(x,-6,.9),(.14,.14,1.8),Y)
    export('Logistics')


random.seed(3303)
landscape(36,'MapDefense')
for x,y,kind in [(-20,-13,'CopperRock'),(-21,0,'IronRock'),(-16,15,'CopperRock'),(21,14,'Rock'),(19,-14,'Rock')]:ore(x,y,kind,130)
# Variegated industrial soil has an irregular boundary, not a single concrete board.
soil=GROUND.copy();soil.name='Textured industrial earth'
node=next(n for n in soil.node_tree.nodes if n.type=='TEX_IMAGE')
original=node.image;pixels=np.array(original.pixels[:],dtype=np.float32).reshape((-1,4))
luma=pixels[:,:3].mean(axis=1)
pixels[:,0]=luma*.72;pixels[:,1]=luma*.76;pixels[:,2]=luma*.70
soil_image=bpy.data.images.new('Industrial earth variation',width=original.size[0],height=original.size[1])
soil_image.pixels.foreach_set(pixels.ravel());soil_image.pack();node.image=soil_image
vv=[(0,0,.026)]
for i in range(64):
    t=i*math.tau/64;r=1+.04*math.sin(i*2.7)
    vv.append((16*math.cos(t)*r,13*math.sin(t)*r,.026))
yard=mesh('Industrial clearing',vv,[(0,i+1,(i+1)%64+1) for i in range(64)],[soil]);ground_uv(yard)
# Compact production blocks with connected routes and clear service roads.
for x in [-9,-5,-1]:
    for y in [-5,0,5]:assembler(x,y,.84)
for x in [3,6,9]:
    for y in [0,3,6]:place('Furnace',x,y,scale=.74)
for x in [3,5,7,9]:
    for y in [-9,-7.6]:
        slab(x,y,1.8,1.6)
        block('Solar panel',(x,y,.55),(1.7,1.35,.08),SOLAR)
        for d in [-.55,0,.55]:block('Solar cell vertical',(x+d,y,.61),(.025,1.35,.018),S)
        for d in [-.42,0,.42]:block('Solar cell horizontal',(x,y+d,.61),(1.7,.02,.018),S)
for x in [-1,1]:
    for y in [7,9]:tank(x,y,.55)
for x in [-7.7,-3.7,.3]:
    for y in [-4,1,6]:
        block('Substation foundation',(x,y,.09),(1.05,1.15,.18),CONCRETE)
        block('Substation switchgear',(x,y,.56),(.65,.8,.93),D)
        block('Switchgear face',(x,y+.41,.59),(.49,.04,.66),S)
        rod('Substation beacon',(x,y,1),(x,y,1.15),.10,Y)
for y in [-4.6,.4,5.4]:
    for x in [2.1,5.1,8.1]:place('Inserter',x,y,scale=.65)
for x in [4,6,8]:
    place('StorageCrate',x,8.7,scale=.66)
    rod('Distribution pipe',(x,7.7,.24),(x,9.4,.24),.07,Y)
for x in [-9,-6,-3]:
    place('StorageCrate',x,-9,scale=.65)
    place('PowerPole',x,-7.7,scale=.65)
for x in [-9,-5,-1]:
    for y in [-5,0,5]:
        place('Inserter',x+1.1,y+.5,scale=.65,yaw=math.pi)
        rod('Service pipe',(x+1.5,y-1.7,.22),(x+1.5,y+1.5,.22),.08,Y)
for y in [-3,2,7]:
    block('Concrete service road',(0,y,.082),(24,.44,.022),D)
place('ResearchBuilding',0,1,scale=.75)
for y in [-7,-2,3,8]:belt([(-11,y),(-3,y),(-3,y+1),(11,y+1)],'PlateItem')
belt([(-12,-9),(-12,10),(11,10),(11,-3)],'CopperItem')
rail([(-30,-10),(-16,-10),(-16,9),(-12,12),(12,12),(12,16),(9,16),(9,12)])
car(-22,-10,Y,True);car(-19.7,-10,S)
wall=[(-2,-11),(11,-11),(15,-7),(15,7),(11,11),(-2,11)]
for a,b in zip(wall,wall[1:]):
    length=math.dist(a,b);steps=math.ceil(length/.9)
    for i in range(steps):
        t=(i+.5)/steps;x=a[0]+(b[0]-a[0])*t;y=a[1]+(b[1]-a[1])*t
        ob=block('Fortification',(0,0,.55),(length/steps-.06,.45,1.1),S)
        ob.location.x=x;ob.location.y=y;ob.rotation_euler.z=math.atan2(b[1]-a[1],b[0]-a[0])
        block('Wall pillar',(x,y,.72),(.18,.62,1.44),D)
for x,y in [(11,-11),(14,-8),(15,-4),(15,0),(15,4),(14,8),(11,11)]:
    cylinder('Turret pedestal',(x,y,.9),.55,1.8,D)
    box('Turret housing',(x,y,1.95),(.9,.7,.55),Y,.06)
    block('Turret mantlet',(x+.48,y,1.97),(.12,.49,.35),D)
    for dy in [-.25,.25]:rod('Twin barrel',(x+.4,y+dy,2),(x+1.3,y+dy,2),.07,S)
    for dx in [-.5,.5]:block('Turret base bracket',(x+dx,y,.9),(.14,.75,.6),S)
    rod('Gun barrel',(x+.25,y,2),(x+1.1,y,2),.10,S)
    rod('Frozen muzzle flash',(x+1.1,y,2),(x+1.9,y,2.05),.075,HOT)
    for dy in [-.22,.22]:rod('Flash rays',(x+1.25,y,2),(x+1.72,y+dy,2.13),.035,HOT)
tower(-1,-10,.8);ring(-1,0,18,GREEN,dashed=True)
mesh('Radar sweep',[(-1,-10,.055),(1,-16,.055),(5,-15,.055)],[(0,1,2)],[GREEN])
for i in range(65):
    x=random.uniform(18,29);y=random.uniform(-10,11);s=random.uniform(.22,.43)
    stone('Native attacker',(x,y,.35),(s*1.3,s,.25),[ENEMY,RED])
    for dy in [-.28,0,.28]:
        for side in [-1,1]:
            rod('Native leg',(x,y+dy,.35),(x+side*s*1.6,y+dy+.18,.08),.035,ENEMY,6)
line('Threat boundary',[(27,-14),(27,14)],.07,.07,RED)
export('MapDefense')

import runpy
runpy.run_path(str(ROOT/'SourceArt/generate_expanded_ui.py'))
