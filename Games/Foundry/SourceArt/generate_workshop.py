"""Original Foundry kit and static reference-directed workshop, seed 912.
Run with Blender 5.2 --background --python this_file.py. Metres, glTF +Y up.
"""
import bpy
import math
import random
import shutil
from pathlib import Path
from mathutils import Vector
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Assets/Workshop'
OUT.mkdir(parents=True, exist_ok=True)
random.seed(912)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.cursor.location = (0, 0, 0)

# Fresh-worktree support. These are ignored boot defaults, never replacements.
engine = ROOT.parents[1] / 'Zenith/Assets'
for face in ['px', 'nx', 'py', 'ny', 'pz', 'nz']:
    target = engine / 'Textures/Cubemap' / (face + '.png')
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        im = bpy.data.images.new('Neutral sky ' + face, width=16, height=16)
        im.pixels[:] = [.45, .58, .65, 1] * 256
        im.filepath_raw = str(target)
        im.file_format = 'PNG'
        im.save()
target = engine / 'Textures/Particles/particleSwirl.png'
if not target.exists():
    target.parent.mkdir(parents=True, exist_ok=True)
    im = bpy.data.images.new('Unused particle default', width=4, height=4)
    im.pixels[:] = [1, 1, 1, 1] * 16
    im.filepath_raw = str(target)
    im.file_format = 'PNG'
    im.save()
target = engine / 'Fonts/LiberationMono-Regular.ttf'
if not target.exists():
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT.parents[1] / 'Middleware/imgui-docking/misc/fonts/Cousine-Regular.ttf', target)


def material(name, rgb, metallic=0, roughness=.7, emission=None):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*rgb, 1)
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*rgb, 1)
    bs.inputs['Metallic'].default_value = metallic
    bs.inputs['Roughness'].default_value = roughness
    if emission:
        bs.inputs['Emission Color'].default_value = (*emission, 1)
        bs.inputs['Emission Strength'].default_value = 1
    return m

Y = material('MK1 golden enamel', (.83, .49, .055), .25, .42)
Y2 = material('Enamel edge highlights', (.97, .67, .15), .22, .4)
D = material('Carbon steel', (.047, .060, .065), .5, .55)
RUBBER = material('Conveyor rubber', (.065, .074, .078), .05, .9)
S = material('Machined alloy', (.47, .55, .57), .65, .38)
B = material('Earth and timber', (.24, .18, .10))
SAND = material('Sandstone shore', (.45, .38, .23))
W = material('Deep teal water', (.045, .23, .30), .05, .3)
SHALLOW = material('Shallow water', (.11, .39, .42), .05, .4)
FOAM = material('Water glints', (.48, .68, .66), .05, .45)
C = material('Copper ore', (.67, .27, .12), .28, .7)
C2 = material('Fresh copper facets', (.88, .42, .21), .32, .5)
I = material('Iron ore', (.39, .46, .54), .25, .68)
I2 = material('Fresh iron facets', (.62, .69, .73), .35, .52)
P = material('Porcelain trim', (.75, .78, .69), .15, .5)
GLASS = material('Blue laboratory glazing', (.13, .30, .40), .45, .22)
HOT = material('Furnace coals', (.95, .21, .015), 0, .5, (1, .2, .008))
FIRE = material('Incandescent core', (1, .76, .22), 0, .5, (1, .68, .16))
RED = material('Vermilion pivot caps', (.59, .105, .035), .3, .46)
WIRE = material('Copper power cable', (.95, .49, .055), .35, .4)
LEAVES = [material('Pine foliage ' + str(i), rgb) for i, rgb in enumerate([
    (.065,.17,.095), (.10,.235,.13), (.17,.29,.13), (.22,.32,.14)])]
GRASS = [material('Meadow leaves ' + str(i), rgb) for i, rgb in enumerate([
    (.20,.30,.095), (.30,.39,.12), (.40,.43,.16)])]


def mesh(name, verts, faces, mats, indices=None):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    ob = bpy.data.objects.new(name, data)
    scene.collection.objects.link(ob)
    for m in mats:
        data.materials.append(m)
    if indices:
        for poly, idx in zip(data.polygons, indices):
            poly.material_index = idx
    return ob


def finish(ob, name, m, bevel=.035):
    ob.name = name
    ob.data.materials.append(m)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = ob.modifiers.new('Machined bevel', 'BEVEL')
        mod.width, mod.segments = bevel, 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mod = ob.modifiers.new('Face weighted normals', 'WEIGHTED_NORMAL')
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob


def box(name, p, size, m, bevel=.035):
    bpy.ops.mesh.primitive_cube_add(size=1, location=p)
    ob = bpy.context.object
    ob.dimensions = size
    return finish(ob, name, m, bevel)


def cylinder(name, p, radius, height, m, sides=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=height, location=p)
    return finish(bpy.context.object, name, m, .012)


def rod(name, a, b, radius, m, sides=8):
    a, b = Vector(a), Vector(b)
    ob = cylinder(name, (a+b)/2, radius, (b-a).length, m, sides)
    ob.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    return ob


def stone(name, p, size, mats):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=p)
    ob = bpy.context.object
    ob.scale = size
    ob.rotation_euler = (random.uniform(-.3,.3), random.uniform(-.3,.3), random.random()*6.28)
    finish(ob, name, mats[0], 0)
    for m in mats[1:]:
        ob.data.materials.append(m)
    for poly in ob.data.polygons:
        poly.material_index = random.randrange(len(mats))
    return ob


kit = {}
def begin():
    return set(scene.objects)


def end(name, before):
    obs = [o for o in scene.objects if o not in before]
    bpy.ops.object.select_all(action='DESELECT')
    for ob in obs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.make_single_user(object=False, obdata=True, material=False, animation=False)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.object.join()
    ob = bpy.context.object
    ob.name = name
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.export_scene.gltf(filepath=str(OUT/(name+'.glb')), export_format='GLB',
        use_selection=True, export_animations=False, export_yup=True)
    kit[name] = ob
    ob.hide_render = True
    ob.hide_set(True)


def place(name, x, y, z=0, yaw=0, scale=1):
    ob = bpy.data.objects.new(name, kit[name].data.copy())
    scene.collection.objects.link(ob)
    ob.location = (x, y, z)
    ob.rotation_euler.z = yaw
    ob.scale = (scale,)*3 if isinstance(scale, (int,float)) else scale
    return ob


def bolts(xs, y, z):
    for x in xs:
        cylinder('Hex fixing', (x,y,z), .044, .045, S, 6)


# Reusable machines face Blender +Y (engine -Z), toward the fixed camera.
a = begin()
box('Heavy drill skid', (0,0,.15), (2.7,2.35,.3), D, .08)
for x in [-1.05,1.05]:
    box('Outrigger shoe', (x,.95,.21), (.48,.62,.32), D)
    rod('Drill diagonal brace', (x,.95,.32), (x,-.34,1.90), .115, Y2)
    rod('Hydraulic piston', (x,.7,.48), (x,-.04,1.38), .056, S)
    rod('Front yellow stabilizer',(x,1.20,.25),(x*.52,.18,1.72),.145,Y)
    rod('Stabilizer piston',(x,1.12,.38),(x*.7,.55,1.2),.06,S)
    box('Front foot plate',(x,1.18,.17),(.46,.48,.16),Y2,.025)
box('Drill motor enclosure', (0,-.45,1.38), (1.85,1.35,.90), Y, .11)
box('Armoured top plate', (0,-.45,1.86), (1.63,1.12,.08), Y2)
for x in [-.65,-.4,-.15,.1,.35]:
    box('Louver grille', (x,-.45,1.914), (.12,.68,.026), D, .008)
bolts([-.75,.75], -.91, 1.94)
bolts([-.75,.75], .06, 1.94)
for x in [-.62,.62]:
    box('Drill roof latch',(x,-.42,1.98),(.12,.43,.09),S,.015)
box('Drill front fascia',(0,.25,1.40),(1.78,.12,.32),Y2,.035)
for x in [-.48,0,.48]:
    rod('Motor face bolt',(x,.28,1.42),(x,.36,1.42),.055,S,6)
rod('Crossbar', (-1.16,-.1,1.7), (1.16,-.1,1.7), .105, S)
cylinder('Boring spindle', (0,.50,.85), .21, 1.4, S)
for z in [.25,.44,.63,.82,1.01]:
    cylinder('Auger flight', (0,.50,z), .39,.105,D)
box('Ore discharge housing', (1.24,-.15,.67), (.48,.95,.75), D)
box('Ore discharge lip', (1.50,-.15,.39), (.54,.88,.12), Y2)
for x in [-.72,.72]:
    rod('Drive shaft', (x,-1.15,.85),(x,-.78,.85),.19,D)
    rod('Drive cap',(x,-1.17,.85),(x,-1.12,.85),.12,S)
end('MiningDrill', a)

a = begin()
box('Smelter foot',(0,0,.13),(2.10,2.06,.26),D,.08)
box('Cast furnace body',(0,0,1.02),(1.9,1.72,1.68),Y,.15)
box('Crown plate',(0,0,1.91),(1.78,1.56,.12),Y2,.05)
for x in [-.73,.73]:
    for y in [-.62,.62]:
        cylinder('Crown rivet',(x,y,2.005),.052,.07,S,6)
    box('Roof reinforcement',(x,0,2.01),(.08,1.32,.07),Y,.012)
    box('Firebox shoulder',(x,.92,1.08),(.17,.16,1.08),Y2,.025)
for x in [-1.04,1.04]:
    cylinder('External furnace valve',(x,.30,.48),.17,.40,D)
box('Front control housing',(-.55,.91,1.57),(.33,.17,.29),D,.025)
box('Control indicator',(-.55,1.01,1.59),(.17,.025,.08),GRASS[2],.004)
box('Black firebox rim',(0,.94,.72),(1.20,.23,1.08),D,.09)
box('Refractory lip',(0,1.075,.66),(.93,.09,.78),RED,.035)
box('Burning cavity',(0,1.135,.69),(.76,.028,.60),HOT,.01)
for x,z,s in [(-.24,.59,.21),(0,.68,.32),(.21,.6,.19)]:
    stone('Fixed flame',(x,1.16,z),(s*.42,.036,s),[FIRE,HOT])
box('Feed tray',(0,1.35,.29),(.85,.72,.13),S)
for x in [-.40,.40]:
    box('Tray rail',(x,1.35,.42),(.09,.7,.22),D)
for x in [-.97,.97]:
    for z in [.63,.86,1.09]:
        box('Side cooling ribs',(x,-.05,z),(.09,1.23,.08),D,.01)
    box('Casing strap',(x,0,1.4),(.10,1.65,.15),Y2)
    for y in [-.66,.66]:
        stone('Casing rivet',(x,y,1.52),(.045,.045,.045),[S])
cylinder('Stack mount',(0,-.40,2.03),.36,.18,D)
cylinder('Chimney',(0,-.40,2.40),.25,.65,D)
cylinder('Exhaust rim',(0,-.40,2.76),.31,.10,S)
cylinder('Black exhaust opening',(0,-.40,2.82),.21,.012,RUBBER)
box('Thermometer',(.57,.88,1.52),(.30,.04,.20),P,.02)
box('Meter inset',(.57,.91,1.52),(.19,.014,.095),D,.005)
end('Furnace',a)

a = begin()
box('Inserter footing',(0,0,.13),(.55,.6,.26),D)
cylinder('Rotating pedestal',(0,0,.37),.21,.3,Y)
rod('Lower arm',(0,0,.5),(.23,0,1.12),.10,Y2)
rod('Upper arm',(.23,0,1.12),(.96,0,.84),.08,Y)
rod('Hydraulic link',(.02,-.11,.60),(.75,-.11,.91),.035,S)
for x,z in [(0,.48),(.23,1.12),(.96,.84)]:
    rod('Pivot axle',(x,-.13,z),(x,.13,z),.14,D)
    rod('Red pivot cap',(x,-.145,z),(x,-.125,z),.095,RED)
for y in [-.14,.14]:
    rod('Pickup fork',(.96,y,.84),(1.12,y,.48),.045,D)
end('Inserter',a)

for name, mats in [('IronItem',[I,I2]),('CopperItem',[C,C2])]:
    a=begin()
    stone(name,(0,0,.12),(.21,.17,.16),mats)
    end(name,a)
a=begin()
box('Iron plate',(0,0,.09),(.35,.24,.13),S,.035)
box('Plate highlight',(0,0,.165),(.29,.18,.018),I2,.012)
end('PlateItem',a)

# Open bins have actual walls, corner posts, panels and different stored contents.
for name,item in [('StorageCrate','IronItem'),('CopperCrate','CopperItem')]:
    a=begin()
    box('Crate skid',(0,0,.11),(1.6,1.42,.22),D)
    box('Crate well',(0,0,.25),(1.36,1.16,.18),D)
    for x in [-.73,.73]:
        box('Side wall',(x,0,.69),(.14,1.39,.92),Y)
        box('Steel top rim',(x,0,1.2),(.16,1.43,.10),S)
    for y in [-.63,.63]:
        box('End wall',(0,y,.69),(1.5,.14,.92),Y)
        box('Steel top rim',(0,y,1.2),(1.52,.16,.10),S)
        for x in [-.55,.55]:
            box('Corner strap',(x,y,.7),(.10,.16,.93),D,.012)
    for i in range(20):
        place(item,random.uniform(-.55,.55),random.uniform(-.45,.45),random.uniform(.62,.94))
    end(name,a)

a=begin()
box('Pole concrete shoe',(0,0,.1),(.57,.57,.2),S)
box('Pole steel socket',(0,0,.28),(.30,.30,.46),D)
box('Timber mast',(0,0,1.68),(.14,.16,3.1),B)
box('Crossarm',(0,0,3.14),(.85,.19,.13),Y)
for x in [-.32,.32]:
    cylinder('Ceramic insulator',(x,0,3.30),.075,.22,P)
    cylinder('Insulator flange',(x,0,3.31),.10,.065,D)
end('PowerPole',a)

# A glazed triangulated lantern dome, not the previous flat-roofed drum.
a=begin()
cylinder('Octagonal laboratory footing',(0,0,.16),1.68,.32,D,8)
cylinder('Laboratory lower body',(0,0,.85),1.46,1.38,P,8)
for i in range(8):
    t=i*math.pi/4
    x,y=1.40*math.cos(t),1.40*math.sin(t)
    rod('Structural buttress',(x,y,.25),(x,y,1.47),.09,S)
cylinder('Dome curb',(0,0,1.54),1.47,.13,D,12)
verts=[(0,0,2.94)]
for z,r,offset in [(2.70,.62,0),(2.17,1.16,math.pi/12),(1.64,1.41,0)]:
    verts += [(r*math.cos(i*math.pi/6+offset),r*math.sin(i*math.pi/6+offset),z) for i in range(12)]
faces=[]
for i in range(12):faces.append((0,1+i,1+(i+1)%12))
for ring in range(2):
    a0=1+ring*12;b0=a0+12
    for i in range(12):
        j=(i+1)%12
        faces.extend([(a0+i,b0+i,b0+j),(a0+i,b0+j,a0+j)])
mesh('Faceted science dome',verts,faces,[GLASS,S],[0 if i%7 else 1 for i in range(len(faces))])
edges=sorted({tuple(sorted((f[i],f[(i+1)%len(f)]))) for f in faces for i in range(len(f))})
for a0,b0 in edges:rod('Dome frame',verts[a0],verts[b0],.023,P,6)
cylinder('Roof cap',(0,0,2.94),.21,.13,S)
box('Research entry surround',(0,1.42,.72),(.85,.21,1.10),D,.06)
box('Orange laboratory door',(0,1.55,.70),(.60,.08,.91),RED,.035)
box('Door readout',(0,1.61,.93),(.35,.025,.13),D,.01)
box('Green status bar',(0,1.63,.94),(.24,.015,.055),GRASS[2],.005)
for x in [-1.2,1.2]:
    box('Lab service locker',(x,.65,.72),(.35,.6,.7),Y)
end('ResearchBuilding',a)

# Rocks, bushes and fuller irregular trees are shared meshes, not repeated boxes.
for name,mats in [('Rock',[S,I,D]),('IronRock',[I,I2]),('CopperRock',[C,C2])]:
    a=begin()
    stone(name,(0,0,.36),(.65,.52,.56),mats)
    end(name,a)
a=begin()
cylinder('Pine trunk',(0,0,.75),.105,1.5,B,7)
for tier,(z,r,h) in enumerate([(.65,.83,1.1),(1.2,.72,1.05),(1.73,.56,.95),(2.2,.35,.72)]):
    verts=[(0,0,z+h)]
    for i in range(10):
        t=i*math.tau/10
        rr=r*(1 if i%2 else .80)
        verts.append((rr*math.cos(t),rr*math.sin(t),z+(.12 if i%2 else 0)))
    faces=[(0,i+1,(i+1)%10+1) for i in range(10)]
    faces.append(tuple(range(10,0,-1)))
    mesh('Angular pine branches',verts,faces,LEAVES,[(i+tier)%4 for i in range(len(faces))])
end('Pine',a)
a=begin()
for x,y,z,s in [(-.24,0,.25,.35),(.22,.1,.3,.37),(0,-.18,.3,.33),(0,.08,.53,.28)]:
    stone('Broadleaf shrub',(x,y,z),(s,s*.8,s),GRASS)
end('Bush',a)
a=begin()
for i in range(7):
    t=i*math.tau/7
    h=random.uniform(.22,.48)
    mesh('Meadow leaf',[(0,0,0),(.16*math.cos(t-.3),.16*math.sin(t-.3),h*.45),(.34*math.cos(t),.34*math.sin(t),h),(.14*math.cos(t+.3),.14*math.sin(t+.3),h*.5)],[(0,1,2),(0,2,3)],GRASS,[i%3,(i+1)%3])
end('Grass',a)

# Straight conveyor module is retained as an editable/exported reusable source.
a=begin()
box('Conveyor support',(0,0,.18),(2,1,.26),D)
box('Rubber bed',(0,0,.34),(2,.79,.08),RUBBER,.01)
for y in [-.48,.48]:box('Raised guard',(0,y,.42),(2,.06,.09),Y2,.015)
for x in [-.8,-.4,0,.4,.8]:box('Rubber tread',(x,0,.39),(.06,.78,.025),S,.005)
end('Conveyor',a)

# Terrain albedo, generated locally in Blender and embedded in the GLB.
# Multi-scale noise breaks up the toy-board appearance without a borrowed texture.
n=1024
rng=np.random.default_rng(912)
noise=np.zeros((n,n),dtype=np.float32)
for cells,amplitude in [(8,.43),(24,.25),(75,.16),(240,.09),(1024,.07)]:
    coarse=rng.random((cells,cells),dtype=np.float32)
    axis=np.arange(n)*cells/n
    lo=np.floor(axis).astype(int)%cells; hi=(lo+1)%cells;f=axis%1;f=f*f*(3-2*f)
    xx=coarse[:,lo]*(1-f)+coarse[:,hi]*f
    layer=xx[lo,:]*(1-f[:,None])+xx[hi,:]*f[:,None]
    noise+=layer*amplitude
color=np.empty((n,n,4),dtype=np.float32)
for k,(base,amp) in enumerate([(.34,.18),(.39,.14),(.17,.09)]):
    color[:,:,k]=base+(noise-.5)*amp*2
color[:,:,3]=1
im=bpy.data.images.new('Seeded moss and earth',width=n,height=n)
im.pixels.foreach_set(color.ravel())
im.filepath_raw=str(ROOT/'SourceArt/MeadowAlbedo.png');im.file_format='PNG';im.save();im.pack()
GROUND=material('Mottled meadow',(.35,.40,.18),0,.95)
bs=GROUND.node_tree.nodes.get('Principled BSDF')
tex=GROUND.node_tree.nodes.new('ShaderNodeTexImage');tex.image=im
GROUND.node_tree.links.new(tex.outputs['Color'],bs.inputs['Base Color'])

def ground_uv(ob):
    uv=ob.data.uv_layers.new(name='UVMap')
    for poly in ob.data.polygons:
        for li in poly.loop_indices:
            p=ob.data.vertices[ob.data.loops[li].vertex_index].co
            uv.data[li].uv=(p.x/12,p.y/12)

a=begin()
ob=box('Terrain source tile',(0,0,-.10),(2,2,.2),GROUND,0);ground_uv(ob)
end('TerrainTile',a)
a=begin();box('Shore source tile',(0,0,-.14),(2,2,.12),SAND,.03);end('ShoreTile',a)
a=begin()
for i in range(18):place('IronRock',random.uniform(-.85,.85),random.uniform(-.85,.85),-.05,scale=random.uniform(.2,.5))
end('OreTile',a)

# Whole terrain uses a continuous shoreline: no rectangular water cutouts.
def shore(y):
    return 16.8 + 1.1*math.sin(y*.46) + .5*math.sin(y*1.1)

def ribbon(name, inner, outer, z, m):
    verts=[]
    for i in range(161):
        y=-24+i*.3
        verts.extend([(shore(y)+inner,y,z),(shore(y)+outer,y,z)])
    ob=mesh(name,verts,[(2*i,2*i+1,2*i+3,2*i+2) for i in range(160)],[m])
    return ob
verts=[]
for i in range(161):
    y=-24+i*.3
    verts.extend([(-27,y,0),(shore(y),y,0)])
ob=mesh('Continuous meadow',verts,[(2*i,2*i+1,2*i+3,2*i+2) for i in range(160)],[GROUND]);ground_uv(ob)
ribbon('Warm eroded bank',0,.52,-.045,SAND)
ribbon('Shallows',.52,1.45,-.115,SHALLOW)
ribbon('Deep water',1.45,20,-.17,W)
# Thin grid stays subordinate to the terrain and stops at the water.
GRID=material('Construction grid',(.23,.29,.12))
for y in range(-19,20,2):box('Grid east-west',((-25+shore(y))/2,y,.005),(shore(y)+25,.012,.008),GRID,0)
for x in range(-24,17,2):
    for y in range(-20,20):
        if x<min(shore(y),shore(y+1))-.15:box('Grid north-south',(x,y+.5,.005),(.012,1,.008),GRID,0)

# Rounded belt paths produce continuous beds, guard rails and tread placement.
def rounded_path(points, radius=.64):
    result=[Vector(points[0])]
    for i in range(1,len(points)-1):
        p=Vector(points[i]);a=(Vector(points[i-1])-p).normalized();b=(Vector(points[i+1])-p).normalized()
        r=min(radius,(Vector(points[i-1])-p).length*.42,(Vector(points[i+1])-p).length*.42)
        q0=p+a*r;q1=p+b*r
        for j in range(9):
            t=j/8;result.append(q0*(1-t)**2+p*2*t*(1-t)+q1*t*t)
    result.append(Vector(points[-1]))
    # Uniformly sample along the polyline for evenly spaced details and items.
    samples=[]
    for a,b in zip(result,result[1:]):
        count=max(1,math.ceil((b-a).length/.12))
        for j in range(count):samples.append(a+(b-a)*j/count)
    samples.append(result[-1]);return samples


# Real openings replace overlapping strips at each three-way connection.
# Port directions are in Blender XY; all belt decks remain at the same height.
JUNCTIONS=[(-5.4,-2.8,('N','S','E')),(-4.0,1.4,('N','S','E')),(7.4,1.4,('N','W','E'))]
JUNCTION_HALF=.72

def outside_junctions(poly):
    polygons=[poly]
    for x,y,_ in JUNCTIONS:
        remaining=[]
        for polygon in polygons:
            inside=polygon
            for axis,bound,sign in [(0,x-JUNCTION_HALF,1),(0,x+JUNCTION_HALF,-1),
                                    (1,y-JUNCTION_HALF,1),(1,y+JUNCTION_HALF,-1)]:
                if not inside:break
                kept=[];outside=[]
                for a,b in zip(inside,inside[1:]+inside[:1]):
                    da=(a[axis]-bound)*sign;db=(b[axis]-bound)*sign
                    (kept if da>=0 else outside).append(a)
                    if (da<0)!=(db<0):
                        t=da/(da-db);p=tuple(a[k]+t*(b[k]-a[k]) for k in range(3))
                        kept.append(p);outside.append(p)
                if len(outside)>=3:remaining.append(outside)
                inside=kept
        polygons=remaining
    return polygons

def near_junction(p,margin=0):
    return any(abs(p.x-x)<JUNCTION_HALF+margin and abs(p.y-y)<JUNCTION_HALF+margin
               for x,y,_ in JUNCTIONS)

def junction(x,y,ports):
    box('Junction chassis',(x,y,.20),(1.44,1.44,.30),D,.035)
    box('Junction transfer deck',(x,y,.355),(1.44,1.44,.03),RUBBER,0)
    for direction,dx,dy in [('N',0,-1),('S',0,1),('E',1,0),('W',-1,0)]:
        # Connected edges have two short rail shoulders and an unobstructed throat.
        if direction in ports:
            for offset in [-.49,.49]:
                box('Junction port rail',(x+dx*.60+(offset if dy else 0),
                    y+dy*.60+(offset if dx else 0),.44),
                    (.08,.24,.06) if dy else (.24,.08,.06),Y2,.008)
        else:
            box('Junction closed rail',(x+dx*.68,y+dy*.68,.44),
                (1.44,.08,.06) if dy else (.08,1.44,.06),Y2,.008)
    # Flush steel guide arrow makes the branch read as a transfer, not a crossing.
    mesh('Transfer direction',[(x-.24,y-.13,.377),(x+.04,y-.13,.377),
        (x+.04,y-.25,.377),(x+.30,y,.377),(x+.04,y+.25,.377),
        (x+.04,y+.13,.377),(x-.24,y+.13,.377)],[(0,1,2,3,4,5,6)],[Y2])

def conveyor(points,item='IronItem',spacing=.73):
    pts=rounded_path(points)
    normals=[]
    for i,p in enumerate(pts):
        d=pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)];d.normalize();normals.append(Vector((-d.y,d.x)))
    # Three long strips, with thick outer chassis walls.
    for name,lo,hi,z,m in [('Rubber belt',-.43,.43,.37,RUBBER),('Left rail',-.53,-.45,.44,Y2),('Right rail',.45,.53,.44,Y2),('Left chassis',-.57,-.53,.28,D),('Right chassis',.53,.57,.28,D)]:
        verts=[]
        for p,nm in zip(pts,normals):verts.extend([(p.x+nm.x*lo,p.y+nm.y*lo,z),(p.x+nm.x*hi,p.y+nm.y*hi,z)])
        clipped=[];faces=[]
        for i in range(len(pts)-1):
            for poly in outside_junctions([verts[j] for j in (2*i,2*i+1,2*i+3,2*i+2)]):
                first=len(clipped);clipped.extend(poly);faces.append(tuple(range(first,len(clipped))))
        mesh(name,clipped,faces,[m])
    dist=0;next_tread=0;next_item=.24;next_support=.2
    for i in range(1,len(pts)):
        dist+=(pts[i]-pts[i-1]).length;p=pts[i];nm=normals[i];angle=math.atan2(-nm.x,nm.y)
        if near_junction(p,.46):
            # Reserve room for the entire tread, cargo and support, not just its centre.
            next_tread=max(next_tread,dist+.24);next_item=max(next_item,dist+spacing)
            next_support=max(next_support,dist+1.8)
            continue
        if dist>=next_tread:
            ob=box('Belt tread',(p.x,p.y,.39),(.045,.82,.025),D,.003);ob.rotation_euler.z=angle;next_tread+=.24
        if dist>=next_item:
            place(item,p.x,p.y,.43,yaw=angle+random.uniform(-.18,.18));next_item+=spacing
        if dist>=next_support:
            ob=box('Conveyor trestle',(p.x,p.y,.16),(.16,1.13,.30),D,.015);ob.rotation_euler.z=angle
            next_support+=1.8

# Main composition: three extractors, looped ore feed, three hot smelters,
# crossing copper line, upper bins, lower depot and the domed lab.
for y in [-5.5,-1.3,4.0]:place('MiningDrill',-12.9,y,scale=1.1)
conveyor([(-11.3,-5.5),(-6.4,-5.5),(-5.4,-4.5),(-5.4,-1.3),(-11.3,-1.3)])
conveyor([(-5.4,-2.8),(-4.0,-2.8),(-4.0,1.4),(8.85,1.4),(9.6,2.15),(9.6,5.6)],'PlateItem',.69)
conveyor([(-11.3,4.0),(-5.0,4.0),(-4.0,3.0),(-4.0,1.4)],'CopperItem',.75)
conveyor([(7.4,1.4),(7.4,-4.8)],'CopperItem',.72)
for x,y,ports in JUNCTIONS:junction(x,y,ports)
for x in [-1.9,1.2,4.3]:
    place('Furnace',x,-1.0,scale=1.08)
    place('Inserter',x-1.25,-.60,yaw=math.pi*.15,scale=1.07)
    place('Inserter',x,1.25,yaw=-math.pi/2,scale=1.02)
for name,x,y in [('StorageCrate',9,-5.0),('CopperCrate',9,-2.8),('StorageCrate',11.2,5.1),('CopperCrate',11.2,3.5)]:
    place(name,x,y,scale=1.13)
place('ResearchBuilding',13,-.1,scale=1.1)
place('Inserter',8.2,-3.7,yaw=-.6)
# Depot power cabinet and drums. Keep the belt junction unobstructed.
box('Depot slab',(9.6,5.2,.08),(4.8,2.5,.16),D,.1)
box('Depot cabinet',(8.2,5.1,.84),(1.15,1.15,1.5),Y,.09)
box('Cabinet door',(8.2,5.71,.83),(.85,.065,1.14),Y2)
for y in [4.5,5.6]:
    cylinder('Oil drum',(12.7,y,.57),.35,1.02,Y)
    for z in [.18,.92]:cylinder('Drum band',(12.7,y,z),.365,.07,D)

# Irregular broad ore fields, with dirt beneath and variable-sized outcrops.
ORE_SOIL=material('Iron-bearing earth',(.25,.255,.22))
COPPER_SOIL=material('Copper-bearing earth',(.45,.24,.13))
def ore_patch(cx,cy,rx,ry,kind,soil,count):
    verts=[(cx,cy,.013)]
    for i in range(28):
        t=i*math.tau/28;r=random.uniform(.9,1.12)
        verts.append((cx+math.cos(t)*rx*r,cy+math.sin(t)*ry*r,.013))
    mesh('Organic ore seam',verts,[(0,i+1,(i+1)%28+1) for i in range(28)],[soil])
    for i in range(count):
        t=random.random()*math.tau;r=math.sqrt(random.random());x=cx+math.cos(t)*rx*r;y=cy+math.sin(t)*ry*r
        size=random.uniform(.25,.82)*(1.25 if x<cx else .8)
        place(kind,x,y,-.04,yaw=random.random()*6.28,scale=(size,size*random.uniform(.7,1.1),size*random.uniform(.65,1.1)))
ore_patch(-15.5,-3.5,4.4,4.8,'IronRock',ORE_SOIL,260)
ore_patch(-15.7,5.2,4.2,2.9,'CopperRock',COPPER_SOIL,150)

poles=[(-.4,-7.3),(4.6,-7.1),(11.6,-6.5),(12.1,2.5),(7.6,6.1),(1.1,5.8),(-2.3,9.4)]
for x,y in poles:place('PowerPole',x,y)
for ia,ib in [(0,1),(1,2),(2,3),(3,4),(4,5),(5,6),(0,2)]:
    x,y=poles[ia];xx,yy=poles[ib];prev=(x,y,3.38)
    for i in range(1,25):
        t=i/24;cur=(x+(xx-x)*t,y+(yy-y)*t,3.38-.58*4*t*(1-t))
        rod('Sagging power cable',prev,cur,.030,WIRE,6);prev=cur

# Vegetation frames the workshop instead of leaving a flat empty board.
for i in range(235):
    x=random.uniform(-22,19);y=random.uniform(-13,13)
    if x>shore(y)-.6:continue
    if (-18<x<16 and -8<y<7.5):continue
    place('Pine',x,y,scale=random.uniform(.7,1.45),yaw=random.random()*6.28)
# Large boundary boulders and rocky water coves.
for x,y,s in [(-5,8.7,2),(-1,9.4,2.6),(4.8,9.5,1.8),(17.4,-8,2.6),(17.5,6.2,2.8),(-20,-8,1.8)]:
    place('Rock',x,y,scale=(s,s*.85,s*.8))
for i in range(90):
    y=random.uniform(-15,15);x=shore(y)+random.uniform(-.45,1.4)
    place('Rock',x,y,-.1,scale=random.uniform(.25,.95),yaw=random.random()*6.28)
    if i%3==0:
        ob=box('Broken water glint',(x+.5,y,-.10),(random.uniform(.16,.5),.05,.018),FOAM,.01)
        ob.rotation_euler.z=random.uniform(-.3,.3)
for i in range(850):
    x=random.uniform(-22,18);y=random.uniform(-12,13)
    if x>shore(y)-.5:continue
    # Preserve the machine silhouettes and conveyor corridors.
    if -15<x<15 and -6.5<y<6.5:
        if not (y<-6 or (x>-.5 and y>3.3 and x<6)):continue
    place('Bush' if i%4==0 else 'Grass',x,y,scale=random.uniform(.4,1.1),yaw=random.random()*6.28)

# Join only the placed diorama, preserving the hidden editable kit.
bpy.ops.object.select_all(action='DESELECT')
placed=[ob for ob in scene.objects if ob not in kit.values()]
for ob in placed:ob.select_set(True)
bpy.context.view_layer.objects.active=placed[0]
bpy.ops.object.join()
bpy.context.object.name='Workshop'
bpy.ops.export_scene.gltf(filepath=str(OUT/'Workshop.glb'),export_format='GLB',use_selection=True,export_animations=False,export_yup=True)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt/FoundryWorkshop.blend'))
print('FOUNDRY_EXPORT_COMPLETE',len(kit),'kit models',OUT)

# Keep icon exports reproducible alongside the modular art.
import runpy
runpy.run_path(str(ROOT / 'SourceArt/generate_ui.py'))


# Author the GPU exhaust billboard alongside the other reproducible assets.
runpy.run_path(str(ROOT / 'SourceArt/generate_smoke.py'))
