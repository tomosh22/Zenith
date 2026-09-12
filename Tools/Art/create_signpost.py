"""Author AB-PROP-11 in Blender; run with Blender's Python API.

Creates a separate scene, preserving the user's current scene. Textures are
embedded PNGs: sRGB base colour, linear glTF G-roughness/B-metallic and +Y normals.
"""
import bpy
import numpy as np
import math
from pathlib import Path

ROOT = Path('C:/dev/Zenith')
OUT = ROOT / 'Games/Zenithmon/Assets/Props/SignPost'
OUT.mkdir(parents=True, exist_ok=True)
scene = bpy.data.scenes.new('AB-PROP-11 SignPost')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
rng = np.random.default_rng(110926)
N = 1024
v, u = np.mgrid[0:N, 0:N].astype(np.float32) / N
# Longitudinal oak grain, warped around knots, with pores and weathered fibres.
warp = v + .007*np.sin(u*22) + .004*np.sin(u*53+v*19)
for x,y in [(0.23,.36),(.73,.72),(.55,.17)]:
    warp += .019*np.exp(-((u-x)/.11)**2-((v-y)/.065)**2)*np.sin((v-y)*36)
fine = rng.random((N,N)).astype(np.float32)
# Stochastic growth bands avoid the regular sine stripes of synthetic wood.
grain=np.zeros_like(u)
for freq,amp in [(28,.45),(79,.24),(217,.16),(613,.10),(1301,.05)]:
    samples=rng.normal(0,1,freq+1)
    grain += amp*np.interp(np.mod(warp,1)*freq,np.arange(freq+1),samples)
pores=np.zeros_like(u)
for i in range(170):
    x,y=rng.random(2); length=rng.uniform(.012,.16)
    pores += np.exp(-((u-x)/length)**6-((warp-y)/rng.uniform(.0003,.0012))**2)
height = .5 + .09*grain - .12*pores + .035*fine
tone = np.clip(.68 + .09*grain - .13*pores + .035*(fine-.5),.2,.9)
colour = np.stack([tone*.65, tone*.52, tone*.39],axis=-1)
rough = np.clip(.78 + .06*grain + .09*pores + .035*fine, .58,.96)

def img(name, rgb, noncolour=False):
    im=bpy.data.images.new(name,width=N,height=N,alpha=True)
    if noncolour: im.colorspace_settings.name='Non-Color'
    rgba=np.ones((N,N,4),np.float32); rgba[:,:,:3]=rgb
    im.pixels.foreach_set(rgba.ravel()); im.update()
    im.filepath_raw=str(OUT/(name+'.png')); im.file_format='PNG'; im.save(); im.pack()
    return im

def material(name, col, h, r, metallic):
    mat=bpy.data.materials.new(name); mat.use_nodes=True
    nodes=mat.node_tree.nodes; links=mat.node_tree.links; bs=nodes.get('Principled BSDF')
    c=nodes.new('ShaderNodeTexImage'); c.image=img(name+'_BaseColor',col)
    links.new(c.outputs['Color'],bs.inputs['Base Color'])
    mr=nodes.new('ShaderNodeTexImage'); mr.image=img(name+'_MetallicRoughness',np.stack([np.ones_like(r),r,np.full_like(r,metallic)],-1),True)
    sep=nodes.new('ShaderNodeSeparateColor'); links.new(mr.outputs['Color'],sep.inputs[0])
    links.new(sep.outputs['Green'],bs.inputs['Roughness']); links.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    dy,dx=np.gradient(h); normal=np.stack([-dx*3,-dy*3,np.ones_like(h)],-1)
    normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    t=nodes.new('ShaderNodeTexImage'); t.image=img(name+'_Normal',normal*.5+.5,True)
    nm=nodes.new('ShaderNodeNormalMap'); links.new(t.outputs['Color'],nm.inputs['Color']); links.new(nm.outputs['Normal'],bs.inputs['Normal'])
    return mat

wood=material('Weathered oak',colour,height,rough,0)
ironcol=np.stack([.10+.06*fine,.085+.045*fine,.068+.035*fine],-1)
iron=material('Forged iron',ironcol,fine*.25,.48+.2*fine,.8)

def finish(obj,name,mat,bevel=.003,vertical=False):
    obj.name=name; obj.data.materials.append(mat)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    # Project UVs along the timber's grain, using physical dimensions.
    uv=obj.data.uv_layers.new(name='UVMap') if not obj.data.uv_layers else obj.data.uv_layers.active
    for poly in obj.data.polygons:
        for li in poly.loop_indices:
            co=obj.data.vertices[obj.data.loops[li].vertex_index].co
            if vertical: a,b=co.z/2, (co.x+co.y)*3
            else: a,b=co.x, co.z*2+co.y
            uv.data[li].uv=(a+.51,b+.49)
    mod=obj.modifiers.new('Worn edges','BEVEL'); mod.width=bevel; mod.segments=3
    bpy.ops.object.modifier_apply(modifier=mod.name)
    mod=obj.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    bpy.ops.object.modifier_apply(modifier=mod.name)
    mod=obj.modifiers.new('Export triangles','TRIANGULATE')
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def box(name,loc,size,mat,bevel=.003,vertical=False):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    ob=bpy.context.object; ob.dimensions=size
    return finish(ob,name,mat,bevel,vertical)

box('Solid oak upright',(0,.025,1),(.105,.10,2),wood,.007,True)
# Two blank arrow boards, genuinely thick wood with chamfered perimeter.
def arrow(name,z,right):
    profile=[(-.45,-.095),(.31,-.095),(.45,0),(.31,.095),(-.45,.095)]
    if not right: profile=[(-x,y) for x,y in profile][::-1]
    verts=[(x,y,z+zz) for y in [-.06,-.018] for x,zz in profile]
    faces=[tuple(range(5)),tuple(reversed(range(5,10)))]+[(i,i+5,(i+1)%5+5,(i+1)%5) for i in range(5)]
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(verts,[],faces); mesh.update()
    ob=bpy.data.objects.new(name,mesh); scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    finish(ob,name,wood,.004)
arrow('Upper blank wayfinding arm',1.76,True)
arrow('Lower blank wayfinding arm',1.46,False)
# Galvanically dark iron washers and hex bolts, one above the other per arm.
for z in [1.705,1.815,1.405,1.515]:
    for radius,depth,y,vertices,label in [(.017,.003,-.062,32,'Washer'),(.010,.007,-.067,6,'Hex bolt')]:
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=(0,y,z),rotation=(math.pi/2,0,0))
        finish(bpy.context.object,label,iron,.001)
# End-grain cap and a narrow foot ferrule protect the exposed timber.
box('Iron post cap',(0,.025,1.994),(.113,.108,.012),iron,.002)
for x in [-.055,.055]: box('Foot ferrule side',(x,.025,.085),(.004,.108,.17),iron,.001)
for y in [-.029,.079]: box('Foot ferrule face',(0,y,.085),(.106,.004,.17),iron,.001)
# One shared mesh with separate wood and iron primitives exercises the
# renderer's per-section material and index-range handling.
bpy.ops.object.select_all(action='SELECT')
bpy.context.view_layer.objects.active=next(iter(scene.objects))
bpy.ops.object.join()
bpy.context.object.name='SignPost'
bpy.ops.export_scene.gltf(filepath=str(OUT/'SignPost.glb'),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True)
bpy.data.libraries.write(str(OUT/'SignPost.blend'),{scene},fake_user=True)
result={'glb':str(OUT/'SignPost.glb'),'objects':len(scene.objects),'front':'glTF +Z; blank boards run along X','dimensions_m':[.9,2,.15]}
