"""Detailed grove-fawn source development, preserving the shipped quadruped rig.

Blender 5.2: --background --python Tools/Art/create_fernfawn.py
Sources stay in the review workspace until skin import and visual acceptance.
The previous study and existing runtime creature bundle are not overwritten.
"""
from pathlib import Path
import sys, math, json, struct
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
import read_zenith_rig as wire

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Build/artifacts/authored_assets/creature_work/Fernfawn'
OUT.mkdir(parents=True,exist_ok=True)
RNG=np.random.default_rng(2026092201)
N=2048
SCENE=bpy.data.scenes.new('Fernfawn detailed source')
bpy.context.window.scene=SCENE
SCENE.unit_settings.system='METRIC'
SCENE.render.engine='CYCLES';SCENE.cycles.samples=1
SCENE.render.bake.margin=12

def B(p):return Vector((p[0],-p[2],p[1]))
def select(ob):
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
    bpy.context.view_layer.objects.active=ob
def skin(ob,bone):
    ob.vertex_groups.new(name=bone).add(list(range(len(ob.data.vertices))),1,'REPLACE')
    return ob
def ellipsoid(name,center,radii,bone,detail=4):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=detail,radius=1,location=B(center))
    ob=bpy.context.object;ob.name=name;ob.scale=(radii[0],radii[2],radii[1])
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for p in ob.data.polygons:p.use_smooth=True
    return skin(ob,bone)
def geometry(name,verts,faces,bone=None):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    ob=bpy.data.objects.new(name,data);SCENE.collection.objects.link(ob)
    for p in data.polygons:p.use_smooth=True
    if bone:skin(ob,bone)
    return ob
def sweep(name,stations,bone,rings=24):
    verts=[]
    for i,(p,rx,ry) in enumerate(stations):
        center=B(p)
        direction=B(stations[min(i+1,len(stations)-1)][0])-B(stations[max(0,i-1)][0])
        axis=direction.normalized();u=Vector((1,0,0));v=axis.cross(u).normalized()
        for k in range(rings):
            angle=k*math.tau/rings
            verts.append(center+u*(rx*math.cos(angle))+v*(ry*math.sin(angle)))
    faces=[tuple(reversed(range(rings)))]
    for i in range(len(stations)-1):
        for k in range(rings):faces.append((i*rings+k,i*rings+(k+1)%rings,(i+1)*rings+(k+1)%rings,(i+1)*rings+k))
    faces.append(tuple((len(stations)-1)*rings+k for k in range(rings)))
    return geometry(name,verts,faces,bone)

def coat(points):
    """Linear RGB coat painting in animal coordinates, including flat fawn spots."""
    p=np.asarray(points);x=p[:,0];y=p[:,2];z=-p[:,1]
    c=np.tile([.29,.092,.027],(len(p),1))
    variation=.91+.09*np.sin(z*40+x*16)+.04*np.sin(y*75+z*20)
    c*=variation[:,None]
    belly=np.exp(-((y-.393)/.041)**2)*np.exp(-(z/.27)**6)*np.exp(-(x/.093)**4)
    throat=np.exp(-(x/.044)**4)*np.exp(-((z-.245)/.065)**4)*np.exp(-((y-.59)/.11)**4)
    jaw=np.exp(-(x/.047)**4)*np.exp(-((z-.393)/.080)**4)*np.exp(-((y-.672)/.021)**4)
    cream=np.clip(np.maximum.reduce([belly,throat*.85,jaw]),0,.95)
    c=c*(1-cream[:,None])+np.array([.62,.46,.29])*cream[:,None]
    dorsal=np.exp(-(x/.031)**2)*np.exp(-((y-.57)/.055)**4)*np.exp(-(z/.29)**6)
    c*=1-dorsal[:,None]*.30
    orbital=((y-.739)/.017)**2+((z-.340)/.024)**2
    lid=np.exp(-((orbital-1)/.34)**2)*np.clip((np.abs(x)-.034)/.016,0,1)
    c*=1-lid[:,None]*.60
    spots=np.zeros(len(p))
    for row in range(3):
        for j in range(7):
            sz=-.22+j*.052+(row%2)*.021
            sy=.493+row*.031-.01*math.sin(j*.8)
            sz+=.005*math.sin(j*5.7+row*2.1);sy+=.005*math.cos(j*3.3+row)
            rad=((z-sz)/(.0085+.003*math.sin(j+row)))**2+((y-sy)/(.006+.002*math.cos(j)))**2
            edge=.15*np.sin(z*940+y*750)+.1*np.sin(z*1370-y*1040)
            spots=np.maximum(spots,np.clip((1.2-rad+edge)*1.9,0,1))
    spots*=np.clip((np.abs(x)-.042)/.038,0,1)
    c=c*(1-spots[:,None]*.88)+np.array([.69,.55,.35])*spots[:,None]*.88
    return np.clip(c,.008,.85)

def image(name,values,linear=False):
    h,w=values.shape[:2];im=bpy.data.images.new(name,width=w,height=h,alpha=True)
    im.colorspace_settings.name='Non-Color' if linear else 'sRGB'
    if values.shape[-1]==3:values=np.concatenate([values,np.ones((h,w,1))],axis=-1)
    im.pixels.foreach_set(np.asarray(values,np.float32).ravel());im.update()
    save_image(im);return im
def save_image(im):
    im.filepath_raw=str(OUT/(im.name+'.png'));im.file_format='PNG';im.save();im.pack()
def pbr(name,albedo,normal=None,rm=None,rough=.75):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links
    bs=n.get('Principled BSDF')
    def tex(im):
        t=n.new('ShaderNodeTexImage');t.image=im;return t
    l.new(tex(albedo).outputs['Color'],bs.inputs['Base Color'])
    if normal is None:normal=image(name+'_Normal',np.tile([.5,.5,1.],(4,4,1)),True)
    if rm is None:rm=image(name+'_MetallicRoughness',np.tile([1,rough,0],(4,4,1)),True)
    nm=n.new('ShaderNodeNormalMap');l.new(tex(normal).outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
    sep=n.new('ShaderNodeSeparateColor');l.new(tex(rm).outputs['Color'],sep.inputs[0]);l.new(sep.outputs['Green'],bs.inputs['Roughness']);l.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    return mat
def solid(name,color,rough):return pbr(name,image(name+'_BaseColor',np.tile(color,(8,8,1))),rough=rough)
def unwrap(ob):
    select(ob);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(72),island_margin=.008)
    bpy.ops.object.mode_set(mode='OBJECT')

def bake_coat(body):
    unwrap(body)
    source=bpy.data.materials.new('Coat bake source');source.use_nodes=True
    body.data.materials.clear();body.data.materials.append(source)
    n=source.node_tree.nodes;l=source.node_tree.links;n.clear()
    output=n.new('ShaderNodeOutputMaterial')
    geom=n.new('ShaderNodeNewGeometry');sep=n.new('ShaderNodeSeparateXYZ');l.new(geom.outputs['Position'],sep.inputs[0])
    lower=n.new('ShaderNodeMath');lower.operation='GREATER_THAN';lower.inputs[1].default_value=.37;l.new(sep.outputs['Z'],lower.inputs[0])
    upper=n.new('ShaderNodeMath');upper.operation='LESS_THAN';upper.inputs[1].default_value=.61;l.new(sep.outputs['Z'],upper.inputs[0])
    mask=n.new('ShaderNodeMath');mask.operation='MULTIPLY';l.new(lower.outputs[0],mask.inputs[0]);l.new(upper.outputs[0],mask.inputs[1])
    maps=[]
    for value in [(1,1,.055),(1,.055,1)]:
        m=n.new('ShaderNodeVectorMath');m.operation='MULTIPLY';m.inputs[1].default_value=value;l.new(geom.outputs['Position'],m.inputs[0]);maps.append(m)
    mix=n.new('ShaderNodeMixRGB');l.new(mask.outputs[0],mix.inputs[0]);l.new(maps[0].outputs[0],mix.inputs[1]);l.new(maps[1].outputs[0],mix.inputs[2])
    noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=1400;noise.inputs['Detail'].default_value=2;l.new(mix.outputs[0],noise.inputs['Vector'])
    ramp=n.new('ShaderNodeMapRange');ramp.inputs['To Min'].default_value=.67;ramp.inputs['To Max'].default_value=1.22;l.new(noise.outputs['Fac'],ramp.inputs['Value'])
    # Bake surface positions, then paint at texel resolution. Vertex colours
    # visibly triangulate small markings after the geometry is decimated.
    encoded=n.new('ShaderNodeVectorMath');encoded.operation='MULTIPLY_ADD'
    encoded.inputs[1].default_value=(.5,.5,.5);encoded.inputs[2].default_value=(.5,.5,.5)
    l.new(geom.outputs['Position'],encoded.inputs[0])
    emit=n.new('ShaderNodeEmission');l.new(encoded.outputs[0],emit.inputs['Color']);l.new(emit.outputs[0],output.inputs['Surface'])
    positions=bpy.data.images.new('Surface positions temporary',width=N,height=N,float_buffer=True)
    positions.colorspace_settings.name='Non-Color';target=n.new('ShaderNodeTexImage');target.image=positions;n.active=target
    select(body);bpy.ops.object.bake(type='EMIT')
    pixels=np.empty(N*N*4,np.float32);positions.pixels.foreach_get(pixels)
    points=pixels.reshape(-1,4)[:,:3]*2-1
    colours=coat(points)
    # Fine pigment variation follows the same coat direction as the normal map.
    strand=np.sin(points[:,0]*11000+np.sin(points[:,1]*93)*2+points[:,2]*190)
    colours*= (.96+.04*strand)[:,None]
    albedo=image('Fernfawn_Coat_BaseColor',colours.reshape(N,N,3))
    bpy.data.images.remove(positions)
    bs=n.new('ShaderNodeBsdfPrincipled');bump=n.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.25;bump.inputs['Distance'].default_value=.00018
    l.new(noise.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs['Normal'],bs.inputs['Normal']);l.new(bs.outputs[0],output.inputs['Surface'])
    normal=bpy.data.images.new('Fernfawn_Coat_Normal',width=N,height=N);normal.colorspace_settings.name='Non-Color';target.image=normal;n.active=target
    bpy.ops.object.bake(type='NORMAL');save_image(normal)
    gray=RNG.random((N,N));rm=np.stack([np.ones_like(gray),.73+gray*.13,np.zeros_like(gray)],axis=-1)
    rm=image('Fernfawn_Coat_MetallicRoughness',rm,True)
    mat=pbr('Chestnut spotted short coat',albedo,normal,rm)
    body.data.materials.clear();body.data.materials.append(mat)
    return albedo,rm

def build_body(bones):
    parts=[]
    def ell(*args):
        ob=ellipsoid(*args);parts.append(ob);return ob
    def tube(*args):
        ob=sweep(*args);parts.append(ob);return ob
    body=ell('Rib cage',(0,.463,-.025),(.111,.115,.227),'Spine01')
    ell('Shoulder girdle',(0,.47,.134),(.100,.135,.103),'Spine02')
    ell('Haunch',(0,.449,-.198),(.109,.13,.103),'Spine00')
    tube('Rising neck',[((0,.486,.155),.077,.084),((0,.565,.200),.061,.072),((0,.637,.233),.045,.057),((0,.709,.281),.041,.049)],'Head')
    ell('Cranium',(0,.723,.306),(.048,.055,.062),'Head')
    tube('Tapered muzzle',[((0,.719,.327),.038,.034),((0,.702,.375),.030,.029),((0,.687,.435),.024,.019)],'Head')
    ell('Cheek and jaw',(0,.681,.345),(.037,.030,.066),'Head')
    for side,letter in [(-1,'L'),(1,'R')]:
        ell('Upper orbital lid',(side*.039,.749,.331),(.004,.003,.020),'Head')
        ell('Lower orbital lid',(side*.039,.728,.335),(.003,.002,.019),'Head')
        for front,prefix,z in [(True,'F',.173),(False,'H',-.182)]:
            x=side*.102
            up='Leg'+prefix+letter+'Up';lo='Leg'+prefix+letter+'Lo'
            top=.515 if front else .495
            tube(prefix+letter+' leg',[
                ((x*.24,top,z-.009),.017,.025),((x*.60,.455,z),.030,.037),((x*.86,.39,z),.028,.032),((x,.313,z-.009),.020,.025),
                ((x,.223,z+(.012 if front else -.040)),.017,.022),
                ((x,.171,z+(.013 if front else -.032)),.018,.020),
                ((x,.080,z+.012),.0095,.012),((x,.035,z+.023),.013,.016)],up)
            # Two-joint blend follows the existing shared-rig leg planes.
            ob=parts[-1];ob.vertex_groups.clear();g0=ob.vertex_groups.new(name=up);g1=ob.vertex_groups.new(name=lo)
            for v in ob.data.vertices:
                w=float(np.clip((v.co.z-.135)/.085,0,1));g0.add([v.index],w,'REPLACE');g1.add([v.index],1-w,'REPLACE')
    tube('Short lifted tail',[((0,.454,-.26),.032,.035),((0,.474,-.299),.028,.030),((0,.5,-.335),.012,.015)],'Tail00')
    bpy.ops.object.select_all(action='DESELECT')
    for ob in parts:ob.select_set(True)
    bpy.context.view_layer.objects.active=body;bpy.ops.object.join();body=bpy.context.object
    donor=[];tree=KDTree(len(body.data.vertices))
    for v in body.data.vertices:
        donor.append([(body.vertex_groups[g.group].name,g.weight) for g in v.groups]);tree.insert(v.co,v.index)
    tree.balance();body.data.remesh_voxel_size=.0022;bpy.ops.object.voxel_remesh()
    smooth=body.modifiers.new('Sculpt relaxation','SMOOTH');smooth.factor=.68;smooth.iterations=7;bpy.ops.object.modifier_apply(modifier=smooth.name)
    blend=body.vertex_groups.new(name='Temporary anatomical blend')
    for v in body.data.vertices:
        # Broader shoulder/haunch relaxation removes primitive intersection
        # creases without shrinking the thin lower legs or facial features.
        weight=float(np.clip((v.co.z-.32)/.065,0,1)*np.clip((.675-v.co.z)/.055,0,1))
        if weight:blend.add([v.index],weight,'REPLACE')
    smooth=body.modifiers.new('Shoulder and haunch sculpt blend','SMOOTH');smooth.factor=.9;smooth.iterations=110;smooth.vertex_group=blend.name;bpy.ops.object.modifier_apply(modifier=smooth.name)
    dec=body.modifiers.new('Export surface density','DECIMATE');dec.ratio=.17;bpy.ops.object.modifier_apply(modifier=dec.name)
    body.vertex_groups.clear()
    for b in bones:body.vertex_groups.new(name=b['name'])
    for v in body.data.vertices:
        weights={}
        for _,idx,distance in tree.find_n(v.co,5):
            factor=1/max(distance,.001)**2
            for bone,w in donor[idx]:weights[bone]=weights.get(bone,0)+w*factor
        chosen=sorted(weights.items(),key=lambda x:-x[1])[:4];total=sum(w for _,w in chosen)
        for bone,w in chosen:
            if w>1e-9:body.vertex_groups[bone].add([v.index],w/total,'REPLACE')
    for face in body.data.polygons:face.use_smooth=True
    body.name='Fernfawn continuous anatomy';return body

def fur(body,albedo,rm):
    body.data.calc_loop_triangles();triangles=body.data.loop_triangles
    areas=np.array([t.area for t in triangles]);areas/=areas.sum()
    picks=RNG.choice(len(triangles),size=34000,p=areas)
    verts=[];faces=[];uvs=[];weights=[]
    uv=body.data.uv_layers.active.data
    for idx in picks:
        tri=triangles[idx];a,b,c=[body.data.vertices[v] for v in tri.vertices]
        r=math.sqrt(RNG.random());s=RNG.random();bc=[1-r,r*(1-s),r*s]
        p=a.co*bc[0]+b.co*bc[1]+c.co*bc[2];normal=(a.normal*bc[0]+b.normal*bc[1]+c.normal*bc[2]).normalized()
        if p.z<.04:continue
        direction=Vector((0,1,-.15)) if .37<p.z<.61 else Vector((0,.1,-1))
        tangent=direction-normal*direction.dot(normal)
        if tangent.length<.05:tangent=normal.cross(Vector((1,0,0)))
        tangent.normalize();side=normal.cross(tangent).normalized()
        length=RNG.uniform(.0015,.0038);width=RNG.uniform(.00008,.00016)
        root=p+normal*.00008;mid=root+tangent*length*.4+normal*length*.33;tip=root+tangent*length+normal*length*.20
        base=len(verts);verts.extend([root-side*width,root+side*width,mid-side*width*.55,mid+side*width*.55,tip])
        faces.extend([(base,base+1,base+3,base+2),(base+2,base+3,base+4)])
        tex=sum((uv[li].uv*weight for li,weight in zip(tri.loops,bc)),Vector((0,0)))
        uvs.extend([tex+Vector((-.0001,0)),tex+Vector((.0001,0)),tex+Vector((-.00006,.0005)),tex+Vector((.00006,.0005)),tex+Vector((0,.001))])
        w={}
        for vertex,bary in zip([a,b,c],bc):
            for group in vertex.groups:
                name=body.vertex_groups[group.group].name;w[name]=w.get(name,0)+group.weight*bary
        selected=sorted(w.items(),key=lambda x:-x[1])[:4];total=sum(v for _,v in selected)
        weights.append((base,[(name,value/total) for name,value in selected]))
    ob=geometry('Fine directional guard hairs',verts,faces)
    layer=ob.data.uv_layers.new(name='UVMap')
    for loop in ob.data.loops:layer.data[loop.index].uv=uvs[loop.vertex_index]
    for g in body.vertex_groups:ob.vertex_groups.new(name=g.name)
    for base,w in weights:
        for bone,value in w:ob.vertex_groups[bone].add(list(range(base,base+5)),value,'REPLACE')
    ob.data.materials.append(pbr('Short coat guard hair',albedo,rm=rm,rough=.84))
    return ob

def details():
    def micro(name,colour,rough,scale=1):
        y,x=np.mgrid[0:256,0:256];height=RNG.random((256,256))*.1+np.sin(y*.34+np.sin(x*.12)*.5)*.03
        rgb=np.array(colour)[None,None,:]*(.8+height[:,:,None]*3)
        dy,dx=np.gradient(height);normal=np.stack([-dx*scale,-dy*scale,np.ones_like(dx)],axis=-1)
        normal/=np.linalg.norm(normal,axis=2,keepdims=True)
        rm=np.stack([np.ones_like(x),np.clip(rough+height*.2,0,1),np.zeros_like(x)],axis=-1)
        return pbr(name,image(name+'_BaseColor',rgb),image(name+'_Normal',normal*.5+.5,True),image(name+'_MetallicRoughness',rm,True))
    hoof=micro('Striated dark keratin',(.032,.022,.017),.55,1.6)
    wet=solid('Wet dark cornea',(.008,.005,.003),.19)
    nose_material=micro('Supple rhinarium',(.012,.008,.006),.38,2.5)
    iris=solid('Warm brown iris',(.10,.042,.010),.22)
    outer=solid('Ear outer chestnut',(.255,.080,.025),.83)
    inner=micro('Ear inner fine down',(.37,.25,.17),.86,1.5)
    leafmat=solid('Living fern green',(.049,.135,.013),.62)
    stemmat=solid('Fern stem',(.07,.103,.019),.70)
    for side,letter in [(-1,'L'),(1,'R')]:
        for front,prefix,z in [(True,'F',.196),(False,'H',-.159)]:
            for toe in [-1,1]:
                cx=side*.102+toe*.0075
                outline=[(-.0065,-.015),(.0065,-.015),(.0075,.014),(.0045,.023),(-.0045,.023),(-.0075,.014)]
                verts=[B((cx+x*.84,.031,z+zz*.62-.003)) for x,zz in outline]+[B((cx+x,.003,z+zz)) for x,zz in outline]
                faces=[tuple(reversed(range(6))),tuple(range(6,12))]+[(j,(j+1)%6,(j+1)%6+6,j+6) for j in range(6)]
                ob=geometry('Flat-soled cloven hoof',verts,faces,'Leg'+prefix+letter+'Lo');select(ob)
                bevel=ob.modifiers.new('Worn keratin edge','BEVEL');bevel.width=.002;bevel.segments=3;bpy.ops.object.modifier_apply(modifier=bevel.name)
                # Recalculate outward normals after constructing the two shells.
                bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT')
                unwrap(ob);ob.data.materials.append(hoof)
        ob=ellipsoid('Recessed lateral eye',(side*.036,.738,.334),(.011,.010,.016),'Head',4);unwrap(ob);ob.data.materials.append(wet)
        ob=ellipsoid('Iris',(side*.046,.738,.336),(.0015,.0065,.009),'Head',3);unwrap(ob);ob.data.materials.append(iris)
        ob=ellipsoid('Horizontal pupil',(side*.047,.738,.337),(.001,.002,.0075),'Head',3);unwrap(ob);ob.data.materials.append(wet)
        # Shaped, cupped pinna; the leaf outline replaces the old smooth ovals.
        start=B((side*.043,.767,.282));end=B((side*.135,.872,.258));axis=(end-start).normalized();across=axis.cross(Vector((0,-1,0))).normalized()
        verts=[];faces=[]
        for i in range(17):
            t=i/16;width=.033*math.sin(math.pi*t)**.78+.001
            for j in range(9):
                w=(j-4)/4
                p=start.lerp(end,t)+across*(w*width)+Vector((0,.012*(1-w*w)*math.sin(math.pi*t),0))
                verts.append(p)
        for i in range(16):
            for j in range(8):faces.append((i*9+j,i*9+j+1,(i+1)*9+j+1,(i+1)*9+j))
        ear=geometry('Cupped ear '+letter,verts,faces,'Head');ear.data.materials.append(inner);ear.data.materials.append(outer)
        select(ear);mod=ear.modifiers.new('Thin pinna','SOLIDIFY');mod.thickness=.003;mod.material_offset=1;bpy.ops.object.modifier_apply(modifier=mod.name)
        sub=ear.modifiers.new('Ear contour','SUBSURF');sub.levels=2;bpy.ops.object.modifier_apply(modifier=sub.name);unwrap(ear)
        # Two unfurling fern fronds retain the grove-antler line's identity.
        stations=[]
        for i in range(12):
            t=i/11;stations.append(((side*(.035+.016*t),.774+.116*t,.308-.014*t*t),.0025*(1-t)+.0006,.0025*(1-t)+.0006))
        stem=sweep('Living fern antler '+letter,stations,'Horn'+letter,10);unwrap(stem);stem.data.materials.append(stemmat)
        for j in range(8):
            t=(j+1)/10;base=B((side*(.035+.016*t),.774+.116*t,.308-.014*t*t))
            for branch in [-1,1]:
                length=.022*(1-t*.70);points=[base,base+Vector((branch*length,.001,.008)),base+Vector((branch*length*.7,-.004,.012)),base+Vector((branch*length*.25,-.005,.007))]
                leaflet=geometry('Fern pinna',points,[(0,1,2,3)],'Horn'+letter);select(leaflet)
                mod=leaflet.modifiers.new('Leaf thickness','SOLIDIFY');mod.thickness=.0004;bpy.ops.object.modifier_apply(modifier=mod.name)
                unwrap(leaflet);leaflet.data.materials.append(leafmat)
    nose=ellipsoid('Soft rhinarium',(0,.691,.440),(.022,.013,.006),'Head',4)
    for v in nose.data.vertices:
        v.co.x*=.75+.25*float(np.clip((v.co.z-.682)/.016,0,1))
    unwrap(nose);nose.data.materials.append(nose_material)

def rig_and_export(bones):
    meshes=[ob for ob in SCENE.objects if ob.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for ob in meshes:ob.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0];bpy.ops.object.join();body=bpy.context.object;body.name='Fernfawn'
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    data=bpy.data.armatures.new('Fernfawn shared quadruped');rig=bpy.data.objects.new('Fernfawn shared quadruped',data);SCENE.collection.objects.link(rig)
    select(rig);bpy.ops.object.mode_set(mode='EDIT')
    for b in bones:
        if not np.allclose(b['world'][:3,:3],np.eye(3),atol=1e-5):raise ValueError('Unsupported non-translation shared rest basis')
        bone=data.edit_bones.new(b['name']);bone.head=B(b['world'][:3,3]);bone.tail=bone.head+Vector((0,0,.05))
        if b['parent']>=0:bone.parent=data.edit_bones[bones[b['parent']]['name']]
    bpy.ops.object.mode_set(mode='OBJECT');body.parent=rig;mod=body.modifiers.new('Shared quadruped skin','ARMATURE');mod.object=rig
    select(body);tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    rig.select_set(True)
    path=OUT/'Fernfawn.glb'
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True,export_skins=True)
    raw=path.read_bytes();n,_=struct.unpack_from('<II',raw,12);doc=json.loads(raw[20:20+n])
    doc['asset']['extras']={'zenithSharedSkeleton':'game:Creatures/Fernfawn/Fernfawn.zskel','zenithAnimations':[f'game:Creatures/Fernfawn/Fernfawn_{c}.zanim' for c in ['Idle','Walk','Attack','Special','Hit','Faint']]}
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4);rest=raw[20+n:]
    path.write_bytes(struct.pack('<III',0x46546c67,2,20+len(js)+len(rest))+struct.pack('<II',len(js),0x4e4f534a)+js+rest)
    bpy.data.libraries.write(str(OUT/'Fernfawn.blend'),{SCENE},fake_user=True)
    print('FERNFAWN_SOURCE',len(body.data.vertices),'vertices',len(body.data.polygons),'triangles',len(body.data.materials),'materials',flush=True)
    return body

def render_review(body):
    # Ground and lights are review-only and are added after exporting the source.
    floor=solid('Review earth',(.105,.12,.082),.9)
    bpy.ops.mesh.primitive_plane_add(size=200);bpy.context.object.data.materials.append(floor)
    for position,power,size in [((1,-1.6,2.2),90,2),((-1,-.1,1.6),22,1.3),((0,1,1.8),70,1.1)]:
        bpy.ops.object.light_add(type='AREA',location=position);ob=bpy.context.object;ob.data.energy=power;ob.data.shape='DISK';ob.data.size=size
        ob.rotation_euler=(Vector((0,0,.45))-ob.location).to_track_quat('-Z','Y').to_euler()
    SCENE.world=bpy.data.worlds.new('Fernfawn review world');SCENE.world.use_nodes=True;SCENE.world.node_tree.nodes.get('Background').inputs[0].default_value=(.18,.20,.23,1);SCENE.world.node_tree.nodes.get('Background').inputs[1].default_value=.30
    bpy.ops.object.camera_add(location=(1.18,-1.52,.83));cam=bpy.context.object;cam.rotation_euler=(Vector((0,-.04,.46))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=66;SCENE.camera=cam
    SCENE.render.resolution_x=1200;SCENE.render.resolution_y=1200;SCENE.render.resolution_percentage=100;SCENE.cycles.samples=48;SCENE.cycles.use_denoising=True
    SCENE.view_settings.view_transform='AgX';SCENE.render.filepath=str(OUT/'Fernfawn_three_quarter.png');bpy.ops.render.render(write_still=True)
    cam.location=(1.45,.04,.63);cam.rotation_euler=(Vector((0,-.035,.45))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=53;SCENE.render.filepath=str(OUT/'Fernfawn_profile.png');bpy.ops.render.render(write_still=True)

def main():
    bones=wire.skeleton(ROOT/'Games/Zenithmon/Assets/Creatures/Fernfawn/Fernfawn.zskel')
    body=build_body(bones);print('ANATOMY',len(body.data.vertices),flush=True)
    albedo,rm=bake_coat(body);print('COAT BAKED',flush=True)
    fur(body,albedo,rm);details();body=rig_and_export(bones);render_review(body)

if __name__=='__main__':main()
