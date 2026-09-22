"""Reproducible Blender authoring for missing ArtBrief world props.

Run blender --background --python Tools/Art/create_missing_props.py.
Existing GLBs are preserved unless --replace-authored is passed. This flag only
rebuilds this script's named deliveries, never the eight pre-existing imports.
Each asset gets an editable blend and embedded
glTF 2.0 textures. Blender Z-up converts to glTF Y-up; fronts face glTF +Z.
"""
import bpy
import bmesh
import numpy as np
import math
import json
import re
import sys
import hashlib
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Games/Zenithmon/Assets/Props'
TEXTURES = OUT / '_AuthoredMaterials'
TEXTURES.mkdir(parents=True, exist_ok=True)
N = 1024
rng = np.random.default_rng(22092026)
v, u = np.mgrid[0:N, 0:N].astype(np.float32) / N
noise = rng.random((N, N)).astype(np.float32)

def image(name, values, linear):
    im = bpy.data.images.new(name, width=N, height=N, alpha=True)
    if linear:
        im.colorspace_settings.name = 'Non-Color'
    rgba = np.ones((N, N, 4), np.float32)
    rgba[:, :, :3] = values
    im.pixels.foreach_set(rgba.ravel())
    im.update()
    im.filepath_raw = str(TEXTURES / (name + '.png'))
    im.file_format = 'PNG'
    im.save()
    im.pack()
    return im

def material(name, colour, kind='stone', roughness=.75, metallic=0):
    # Multi-scale stochastic texture with grain, mineral inclusions or pitting.
    field = np.zeros_like(u)
    for freq, amp in [(7,.36),(19,.26),(51,.19),(139,.12),(373,.07)]:
        grid = rng.random((freq + 1, freq + 1)).astype(np.float32)
        x, y = u * freq, v * freq
        ix, iy = x.astype(int), y.astype(int)
        fx, fy = x - ix, y - iy
        fx, fy = fx*fx*(3-2*fx), fy*fy*(3-2*fy)
        field += amp*((grid[iy,ix]*(1-fx)+grid[iy,ix+1]*fx)*(1-fy)
                      +(grid[iy+1,ix]*(1-fx)+grid[iy+1,ix+1]*fx)*fy)
    h = field*.25 + noise*.035
    tone = .75 + field*.42 + (noise-.5)*.14
    if kind == 'wood':
        warp = v + .012*np.sin(u*15) + .005*np.sin(u*37+v*12)
        grain = np.zeros_like(u)
        for f,a in [(30,.4),(90,.3),(271,.2),(833,.1)]:
            grain += a*np.interp(np.mod(warp,1)*f,np.arange(f+1),rng.normal(0,1,f+1))
        pores = np.maximum(0, np.sin(warp*2550+u*11)-.88)*(.5+.5*np.sin(u*45))
        h += .08*grain - .25*pores
        tone += .18*grain - .5*pores
    elif kind in ('stone','rock'):
        # Fine feldspar/quartz and dark mica interspersed in a weathered matrix.
        mineral = np.where(noise>.92,.2,np.where(noise<.07,-.18,0))
        tone += mineral
        h += mineral*.12
        if kind == 'rock':
            tone += (field-.5)*.9
            # Quartz/feldspar patches and dark mica have different reflectance.
            crystals=np.where(noise>.82,.25,np.where(noise<.16,-.23,0))
            tone+=crystals
            h+=crystals*.05
    elif kind == 'metal':
        pits = (noise>.94)*field
        h -= pits*.18
        tone += .1*np.sin(v*1600)*noise - .2*pits
    elif kind == 'leaf':
        vein = np.exp(-((v-.5)/.017)**2)
        side = np.maximum(0,np.cos((u+np.abs(v-.5)*.6)*140))**18
        h += vein*.07 + side*.012
        tone += vein*.2
    elif kind == 'cloth':
        warp=np.sin(u*math.tau*192); weft=np.sin(v*math.tau*192)
        weave=(warp+weft)*.5
        h+=weave*.025
        tone+=weave*.045
    elif kind == 'fur':
        strand=np.sin(v*math.tau*230+np.sin(u*22)*2.2)
        tone=.87+field*.22+strand*.035+(noise-.5)*.05
        h=field*.008+strand*.006+noise*.004
    rgb = np.clip(tone[:,:,None]*np.array(colour),.002,.98)
    rough = np.clip(roughness + (field-.5)*.15 + (noise-.5)*.05,.04,.98)
    dy,dx = np.gradient(h)
    normal_strength=100 if kind=='rock' else 6
    normal = np.stack([-dx*normal_strength,-dy*normal_strength,np.ones_like(h)],axis=-1)
    normal /= np.linalg.norm(normal,axis=-1,keepdims=True)
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bs = nodes.get('Principled BSDF')
    for suffix, values, linear in [('BaseColor',rgb,False),
        ('Normal',normal*.5+.5,True),
        ('MetallicRoughness',np.stack([np.ones_like(u),rough,np.full_like(u,metallic)],-1),True)]:
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = image(name+'_'+suffix,values,linear)
        if suffix == 'BaseColor':
            links.new(tex.outputs['Color'],bs.inputs['Base Color'])
        elif suffix == 'Normal':
            nm = nodes.new('ShaderNodeNormalMap')
            links.new(tex.outputs['Color'],nm.inputs['Color'])
            links.new(nm.outputs['Normal'],bs.inputs['Normal'])
        else:
            sep = nodes.new('ShaderNodeSeparateColor')
            links.new(tex.outputs['Color'],sep.inputs[0])
            links.new(sep.outputs['Green'],bs.inputs['Roughness'])
            links.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    return mat

oak = material('Silvered oak',(.30,.22,.145),'wood')
endwood = material('Cut heartwood',(.39,.28,.17),'wood')
iron = material('Blackened wrought iron',(.075,.067,.056),'metal',.53,.85)
brass = material('Aged brass',(.48,.32,.12),'metal',.38,.82)
granite = material('Weathered granite',(.37,.365,.34))
basalt = material('Volcanic basalt',(.085,.080,.074),roughness=.88)
sandstone = material('Bedded sandstone',(.49,.29,.15),roughness=.86)
slate = material('Split slate',(.145,.17,.18),roughness=.72)
cork = material('Cork pinboard',(.47,.32,.18),roughness=.93)
green = material('Meadow leaves',(.10,.22,.032),'leaf',.68)
reed = material('Reed stems',(.28,.32,.09),'wood',.72)
dry = material('Dry sedge',(.43,.34,.15),'leaf',.87)
snow = material('Granular snow',(.87,.90,.91),roughness=.83)
petal = material('Daisy ivory',(.83,.79,.61),'leaf',.63)
pollen = material('Ochre pollen',(.65,.40,.035),roughness=.92)
shellmat = material('Shell nacre',(.72,.64,.51),roughness=.29)
amber = material('Lantern amber glass',(.63,.32,.065),roughness=.14)
emerald = material('Emerald phial',(.027,.38,.14),roughness=.12)
glass = material('Clear bottle glass',(.78,.90,.84),roughness=.08)
glass.surface_render_method = 'DITHERED'
glass.node_tree.nodes.get('Principled BSDF').inputs['Alpha'].default_value = .18
red = material('Orb enamel',(.49,.027,.014),roughness=.21)

scene = None

def finish(ob, name, mat, bevel=0, smooth=False):
    ob.name = name
    ob.data.materials.append(mat)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if not ob.data.uv_layers:
        ob.data.uv_layers.new(name='UVMap')
    # Box projection in physical space avoids collapsed UVs on end faces.
    uv = ob.data.uv_layers.active
    for p in ob.data.polygons:
        axis = max(range(3),key=lambda i:abs(p.normal[i]))
        axes = [(1,2),(0,2),(0,1)][axis]
        for li in p.loop_indices:
            co = ob.data.vertices[ob.data.loops[li].vertex_index].co
            uv.data[li].uv = (co[axes[0]]+.371,co[axes[1]]+.213)
        p.use_smooth = smooth
    if bevel:
        mod=ob.modifiers.new('Soft worn edges','BEVEL'); mod.width=bevel; mod.segments=3
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mod=ob.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob

def box(name,loc,size,mat,bevel=.005):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    ob=bpy.context.object; ob.dimensions=size
    return finish(ob,name,mat,bevel)

def cyl(name,loc,radius,depth,mat,vertices=32):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc)
    return finish(bpy.context.object,name,mat,min(.002,radius*.12),True)

def beam(name,a,b,r,mat):
    a,b=Vector(a),Vector(b)
    ob=cyl(name,(a+b)*.5,r,(b-a).length,mat,16)
    ob.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    return ob

def sphere(name,loc,size,mat,detail=3,irregular=0):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=detail,radius=1,location=loc)
    ob=bpy.context.object
    for vert in ob.data.vertices:
        p=vert.co
        wobble=1+irregular*(.45*math.sin(p.x*5+p.y*3)+.3*math.sin(p.y*9+p.z*5)+.2*math.sin(p.z*17+p.x*11))
        p*=wobble
    ob.scale=Vector(size)*.5
    return finish(ob,name,mat,0,True)

def mesh(name,verts,faces,mat,bevel=0):
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.update()
    ob=bpy.data.objects.new(name,me); scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    return finish(ob,name,mat,bevel)

def torus(name,loc,major,minor,mat,rotation=(0,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=10,location=loc,
        major_radius=major,minor_radius=minor,rotation=rotation)
    return finish(bpy.context.object,name,mat,0,True)

def bolt(loc,rotation=(math.pi/2,0,0)):
    ob=cyl('Square forged nail',loc,.009,.008,iron,6); ob.rotation_euler=rotation

def lantern():
    # Off-axis hung lantern, loop and swept bracket: unlike the fixed-head lamp.
    cyl('Flared foot',(0,0,.06),.075,.12,iron)
    cyl('Tapered standard',(0,0,1.14),.027,2.16,iron)
    for z in [.16,.25,1.9,2.2]: cyl('Decorative collar',(0,0,z),.039,.035,brass)
    pts=[(0,0,2.2),(.05,0,2.31),(.16,0,2.36),(.27,0,2.32),(.29,0,2.24)]
    for a,b in zip(pts,pts[1:]): beam('Forged crook',a,b,.013,iron)
    torus('Suspension ring',(.285,0,2.205),.038,.006,iron,(math.pi/2,0,0))
    cx=.285
    cyl('Lantern foot',(cx,0,1.75),.095,.025,iron)
    cyl('Lantern shoulder',(cx,0,2.04),.105,.025,iron)
    for x in [-.062,.062]:
        for y in [-.062,.062]: beam('Corner glazing bar',(cx+x,y,1.76),(cx+x,y,2.03),.007,iron)
    box('Amber glass panes',(cx,0,1.895),(.12,.12,.255),amber,.008)
    bpy.ops.mesh.primitive_cone_add(vertices=32,radius1=.11,radius2=.025,depth=.105,location=(cx,0,2.105))
    finish(bpy.context.object,'Vented lantern roof',iron,.002)
    cyl('Roof vent',(cx,0,2.168),.025,.028,brass)

def fence_wood():
    for x in [-.95,.95]:
        box('Oak mortised post',(x,0,.49),(.1,.12,.98),oak,.007)
        box('Chamfered cap',(x,0,.984),(.1,.12,.025),endwood,.007)
    for z in [.33,.72]:
        box('Continuous rail',(0,0,z),(2,.07,.115),oak,.006)
        for x in [-.95,.95]: bolt((x,-.041,z))

def stone_wall():
    # Separate offset courses, dressed end stones meet the exact 2 m tile seam.
    for row in range(5):
        edges=([-1,-.75,-.26,.23,.74,1] if row%2 else [-1,-.51,0,.51,1])
        for left,right in zip(edges,edges[1:]):
            ob=box('Hand fitted dry stone',((left+right)/2,0,.085+row*.171),
                (right-left-.012,.20+rng.uniform(-.012,.012),.166),granite,.023)
            # Broad split faces and chipped edges; dry wall stones are not
            # smooth ellipsoids. Preserve the exact module-end planes.
            for vertex in ob.data.vertices:
                p=vertex.co
                p.y+=.005*math.sin(p.x*43+row*2+p.z*19)
                p.z+=.003*math.sin(p.x*31+row)
    for i in range(8): box('Coping stone',(-.875+i*.25,0,.876),(.246,.22,.046),slate,.013)

def townboard():
    for x in [-.66,.66]:
        box('Noticeboard post',(x,.015,.99),(.09,.10,1.98),oak,.006)
        box('Post shoe',(x,.015,.10),(.1,.11,.2),iron,.004)
    box('Cork notice surface',(0,0,1.38),(1.25,.043,.89),cork,.006)
    for x in [-.665,.665]: box('Side frame',(x,-.018,1.38),(.075,.085,1.02),oak)
    for z in [.91,1.85]: box('Frame crossbar',(0,-.018,z),(1.39,.085,.075),oak)
    for x in [-.665,.665]:
        for z in [.91,1.85]: bolt((x,-.065,z))
    for side in [-1,1]:
        for row in range(3):
            for col in range(12):
                ob=box('Individual split roof shingle',(-.733+col*.133,side*(.045+row*.08),2.17-row*.056),(.132,.125,.018),slate,.003)
                ob.rotation_euler.x=-side*.6
    box('Ridge cap',(0,0,2.185),(1.6,.05,.03),iron)

def plank_bridge():
    for x in [-1.18,1.18]: box('Structural oak bearer',(x,0,.15),(.18,4,.24),oak,.01)
    for i in range(23):
        y=-2+(i+.5)*4/23
        box('Deck plank',(0,y,.31+rng.uniform(-.004,.004)),(3,4/23-.008,.10),oak,.006)
        for x in [-1.18,1.18]: bolt((x,y,.366),(0,0,0))
    for x in [-1.41,1.41]:
        for y in [-1.93,0,1.93]: box('Railing stanchion',(x,y,.66),(.10,.14,.68),oak,.006)
        for z in [.64,.96]: box('Continuous handrail',(x,0,z),(.12,4,.08),oak,.005)

def stone_bridge():
    # True voussoir arch: underside is open, not a solid cuboid with a decal.
    for j in range(18):
        a=math.pi*j/18+(.008 if j else 0); b=math.pi*(j+1)/18-(.008 if j<17 else 0)
        verts=[(x,2.5*math.cos(t),.12+radius*math.sin(t))
               for x in [-1.7,1.7] for radius in [.58,.83] for t in [a,b]]
        mesh('Arch voussoir',verts,[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)],granite,.01)
    for x in [-1.56,1.56]:
        for j in range(16):
            y=-2.5+(j+.5)*5/16
            z=.12+.83*math.sqrt(max(0,1-(y/2.5)**2))
            box('Parapet coping',(x,y,z+.19),(.28,5/16-.008,.38),granite,.03)

def ledge(high):
    w,d,h=(2.4,1.2,1) if high else (2,1,.6)
    # Continuous eroded cliff, a receding lower face and an overhanging lip.
    verts=[]; faces=[]; nx=40; nz=18
    for back in [False,True]:
        for iz in range(nz+1):
            t=iz/nz
            for ix in range(nx+1):
                x=-w/2+w*ix/nx
                erosion=.025*math.sin(x*13+t*8)+.012*math.sin(x*31-t*11)
                y=d*.5 if back else -d*(.22+.28*t*t)+erosion
                z=h*t + (.018*math.sin(x*11) if iz==nz else 0)
                verts.append((x,y,z))
    layer=(nx+1)*(nz+1)
    for back in [0,1]:
        for iz in range(nz):
            for ix in range(nx):
                a=back*layer+iz*(nx+1)+ix
                face=(a,a+1,a+nx+2,a+nx+1)
                faces.append(face if not back else face[::-1])
    for ix in range(nx):
        a=nz*(nx+1)+ix
        faces.extend([(a,a+layer,a+layer+1,a+1),(ix,ix+1,ix+1+layer,ix+layer)])
    for iz in range(nz):
        a=iz*(nx+1); b=a+nx
        faces.extend([(a,a+nx+1,a+nx+1+layer,a+layer),(b,b+layer,b+layer+nx+1,b+nx+1)])
    mesh('Undercut bedrock ledge',verts,faces,sandstone,.008)
    for i in range(7): sphere('Weathered lip',(-w/2+(i+.5)*w/7,-d*.31,h*.97),(w/5,d*.47,.13),granite,3,.12)

def rock(name,dimensions):
    # An angular fracture hull gives the stone broad split faces. A perturbed
    # smooth sphere reads as clay under the game's grazing light.
    bm=bmesh.new()
    for j in range(46):
        p=rng.normal(size=3);p/=np.linalg.norm(p)
        p*=rng.uniform(.76,1.10)
        p[2]=max(-.58,p[2])
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm,input=list(bm.verts),use_existing_faces=False)
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context='VERTS')
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    me=bpy.data.meshes.new(name);bm.to_mesh(me);bm.free();me.update()
    ob=bpy.data.objects.new(name,me);scene.collection.objects.link(ob)
    low=np.min([list(v.co) for v in me.vertices],axis=0)
    high=np.max([list(v.co) for v in me.vertices],axis=0)
    for vertex in me.vertices:
        p=(np.array(vertex.co)-low)/(high-low)*np.array(dimensions)
        vertex.co=(p[0]-dimensions[0]/2,p[1]-dimensions[1]/2,p[2])
    me.update()
    stone=material(name+' mineral surface',(.33,.325,.30),'rock',.82)
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
    finish(ob,name,stone,min(dimensions)*.017,False)

def leaf(a,angle,height,mat):
    x,y,z=a; dx,dy=math.cos(angle),math.sin(angle)
    width=height*.05
    verts=[]
    for i in range(5):
        t=i/4; bend=height*.42*t*t
        w=width*math.sin(math.pi*(t*.95+.025))
        verts.extend([(x+dx*bend-dy*w,y+dy*bend+dx*w,z+height*t),
                      (x+dx*bend,y+dy*bend,z+height*t+.007),
                      (x+dx*bend+dy*w,y+dy*bend-dx*w,z+height*t)])
    faces=[]
    for i in range(4):
        for j in range(2): faces.append((i*3+j,i*3+j+1,(i+1)*3+j+1,(i+1)*3+j))
    ob=mesh('Curved leaf blade',verts,faces,mat)
    # Explicit back faces work with glTF and the engine's opaque culling.
    mod=ob.modifiers.new('Leaf thickness','SOLIDIFY'); mod.thickness=.0007
    bpy.ops.object.modifier_apply(modifier=mod.name)

def dressing(biome):
    stone=basalt if biome=='Volcanic' else sandstone if biome=='Canyon' else granite
    for i in range(13):
        x,y=rng.uniform(-1.2,1.2,2); s=rng.uniform(.13,.48)
        sphere('Scattered mineral rubble',(x,y,s*.28),(s,s*.8,s*.65),stone,3,.18)
    if biome in ['Coast','Wetland']:
        beam('Weathered fallen log',(-1,-.65,.16),(.65,-.25,.22),.13,oak)
        for i in range(4): beam('Broken driftwood branch',(-.7+i*.3,-.58+i*.06,.18),(-.8+i*.3,-.9,.30),.026,endwood)
    if biome=='Snow':
        for i in range(7):
            x,y=rng.uniform(-1,1,2)
            sphere('Wind deposited snow',(x,y,.19),(.8,.6,.28),snow,3,.1)
    if biome=='Volcanic':
        for i in range(7):
            x,y=rng.uniform(-1,1,2)
            sphere('Fractured basalt shard',(x,y,.38),(.25,.27,.8),basalt,1,.25)
    if biome=='Coast':
        for i in range(8):
            x,y=rng.uniform(-1.15,1.15,2)
            ob=sphere('Shell fragment',(x,y,.045),(.13,.095,.06),shellmat,3,.03)
            ob.rotation_euler.z=rng.uniform(0,6.28)
    for clump in range(9):
        x,y=rng.uniform(-1.05,1.05,2)
        count=14 if biome in ['Meadow','Wetland','Coast'] else 6
        for i in range(count):
            height=rng.uniform(.20,.56) if biome!='Wetland' else rng.uniform(.6,1.3)
            leaf((x+rng.uniform(-.07,.07),y+rng.uniform(-.07,.07),.02),rng.uniform(0,6.28),height,
                 green if biome=='Meadow' else reed if biome=='Wetland' else dry)
        if biome=='Wetland':
            beam('Reed stalk',(x,y,0),(x+.04,y,1.23),.007,reed)
            cyl('Cattail seed head',(x+.04,y,1.27),.022,.17,endwood)
        if biome=='Meadow':
            z=rng.uniform(.34,.6)
            beam('Flower stem',(x,y,0),(x,y,z),.004,green)
            sphere('Flower centre',(x,y,z),(.037,.037,.018),pollen,2)
            for p in range(8):
                a=p*math.tau/8
                ob=sphere('Daisy petal',(x+.026*math.cos(a),y+.026*math.sin(a),z),(.045,.016,.009),petal,2)
                ob.rotation_euler.z=a
        if biome in ['Snow','Canyon','Volcanic']:
            beam('Bare twig',(x,y,0),(x+.09,y,.54),.009,oak)
            for s in [-1,1]: beam('Twig fork',(x+.05,y,.3),(x+s*.14,y+.02,.47),.005,oak)

def item(name):
    cyl('Pickup plinth',(0,0,.065),.25,.13,slate,48)
    torus('Plinth rim',(0,0,.12),.205,.012,brass)
    if name=='ItemPhial':
        sphere('Transparent phial body',(0,0,.46),(.32,.32,.52),glass,4)
        sphere('Visible liquid',(0,0,.405),(.287,.287,.39),emerald,4)
        cyl('Phial neck',(0,0,.735),.09,.18,glass)
        for z in [.66,.77]: torus('Brass neck band',(0,0,z),.09,.011,brass)
        cyl('Cork stopper',(0,0,.855),.084,.12,cork)
        torus('Foot band',(0,0,.28),.119,.012,brass)
    elif name=='ItemOrb':
        sphere('Collectable enamel orb',(0,0,.36),(.61,.61,.61),red,5)
        torus('Orb equator',(0,0,.36),.306,.014,brass)
        sphere('Latch',(0,-.302,.36),(.09,.025,.09),brass,3)
    else:
        # Empty shallow socket; no floating object or coloured silhouette.
        torus('Empty socket',(0,0,.16),.15,.035,slate)

builders={'LanternPost':lantern,'FenceWood':fence_wood,'FenceStone':stone_wall,
          'TownBoard':townboard,'BridgePlank':plank_bridge,'BridgeStone':stone_bridge,
          'LedgeLow':lambda:ledge(False),'LedgeHigh':lambda:ledge(True),
          'RockSmall':lambda:rock('Knee high granite',(.8,.8,.7)),
          'RockLarge':lambda:rock('Waist high granite',(1.6,1.5,1.4)),
          'Boulder':lambda:rock('Granite boulder',(2.4,2.2,2))}
for biome in ['Meadow','Volcanic','Coast','Wetland','Snow','Canyon']:
    builders['Dressing'+biome]=lambda b=biome:dressing(b)
for name in ['ItemPhial','ItemOrb','ItemTaken']:
    builders[name]=lambda n=name:item(n)

def main():
    global scene, rng
    report=[]
    for name, build in builders.items():
        only=next((a.split('=',1)[1].split(',') for a in sys.argv if a.startswith('--only=')),None)
        if only and name not in only:
            continue
        target=OUT/name
        if (target/(name+'.glb')).exists() and '--replace-authored' not in sys.argv:
            print('PRESERVED',name,flush=True)
            continue
        target.mkdir(parents=True,exist_ok=True)
        scene=bpy.data.scenes.new('Authored '+name)
        bpy.context.window.scene=scene
        scene.unit_settings.system='METRIC'
        rng=np.random.default_rng(int.from_bytes(hashlib.sha256(name.encode()).digest()[:8],'little'))
        build()
        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active=next(iter(scene.objects))
        bpy.ops.object.join()
        ob=bpy.context.object; ob.name=name
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        mod=ob.modifiers.new('Export triangles','TRIANGULATE')
        bpy.ops.object.modifier_apply(modifier=mod.name)
        # Ground base at 0. Exact tiling axes stay in original world metres.
        low=min(v.co.z for v in ob.data.vertices)
        for vertex in ob.data.vertices: vertex.co.z-=low
        bpy.context.view_layer.update()
        bpy.ops.export_scene.gltf(filepath=str(target/(name+'.glb')),export_format='GLB',
            use_selection=True,use_active_scene=True,export_animations=False,
            export_tangents=True,export_yup=True)
        bpy.data.libraries.write(str(target/(name+'.blend')),{scene},fake_user=True)
        report.append({'name':name,'triangles':len(ob.data.polygons),'dimensions_blender':list(ob.dimensions),
                       'materials':len(ob.data.materials),'source':str(target/(name+'.glb'))})
        print('AUTHORED',json.dumps(report[-1]),flush=True)
    report_path=OUT/'authored_delivery.json'
    previous={r['name']:r for r in json.loads(report_path.read_text())} if report_path.exists() else {}
    previous.update({r['name']:r for r in report})
    report_path.write_text(json.dumps(list(previous.values()),indent=2))

if __name__ == "__main__":
    main()
