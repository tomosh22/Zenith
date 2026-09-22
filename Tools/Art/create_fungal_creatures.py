"""Blender source authoring for Sporeling, Duskcap and Fungrove.

Individually composed fungal forms with modelled lamellae, branching mycelium,
ten named floater joints, and embedded glTF metallic/roughness PBR maps. Outputs
remain review candidates under Build/artifacts until inspected and installed.
"""
from pathlib import Path
import math, json, sys
import bpy
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Build/artifacts/authored_assets/creature_work/FungalLine'
OUT.mkdir(parents=True,exist_ok=True)
scene=bpy.data.scenes.new('Fungal lineage source');bpy.context.window.scene=scene
scene.unit_settings.system='METRIC';scene.render.engine='CYCLES'
scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.view_settings.view_transform='AgX'
objects=[];rng=np.random.default_rng(107109)

def select(ob):
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob

def image(name,rgb,data=False):
    rgb=np.asarray(rgb,np.float32)
    if rgb.ndim==1:rgb=np.tile(rgb,(8,8,1))
    h,w=rgb.shape[:2]
    if rgb.shape[-1]==3:rgb=np.concatenate([rgb,np.ones((h,w,1),np.float32)],axis=-1)
    im=bpy.data.images.new(name,width=w,height=h,alpha=True)
    im.colorspace_settings.name='Non-Color' if data else 'sRGB'
    im.pixels.foreach_set(rgb.astype(np.float32).ravel());im.update()
    im.filepath_raw=str(OUT/(name+'.png'));im.file_format='PNG';im.save();im.pack();return im

def pbr(name,base,normal,rm):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links
    bs=n.get('Principled BSDF')
    def tex(im):
        t=n.new('ShaderNodeTexImage');t.image=im;return t
    l.new(tex(base).outputs['Color'],bs.inputs['Base Color'])
    nm=n.new('ShaderNodeNormalMap');l.new(tex(normal).outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
    sep=n.new('ShaderNodeSeparateColor');l.new(tex(rm).outputs['Color'],sep.inputs[0])
    l.new(sep.outputs['Green'],bs.inputs['Roughness']);l.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    return mat

flat=image('Fungal_neutral_normal',[.5,.5,1],True)
def plain(name,colour,rough):
    return pbr(name,image(name+'_BaseColor',colour),flat,image(name+'_MetallicRoughness',[1,rough,0],True))

def field(n,seed):
    """Periodic multi-scale pigment variation with no baked lighting."""
    generator=np.random.default_rng(seed);result=np.zeros((n,n),np.float32)
    yy,xx=np.mgrid[0:n,0:n].astype(np.float32)/n
    for frequency,amplitude in [(3,.55),(7,.30),(19,.16),(47,.09),(113,.04),(251,.02)]:
        lattice=generator.uniform(-1,1,(frequency,frequency)).astype(np.float32)
        x=xx*frequency;y=yy*frequency;ix=x.astype(int);iy=y.astype(int)
        fx=x-ix;fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
        a=lattice[iy,ix]*(1-fx)+lattice[iy,(ix+1)%frequency]*fx
        b=lattice[(iy+1)%frequency,ix]*(1-fx)+lattice[(iy+1)%frequency,(ix+1)%frequency]*fx
        result+=amplitude*(a*(1-fy)+b*fy)
    return result,xx,yy

def relief_map(name,height,strength):
    dy,dx=np.gradient(height);normal=np.stack([-dx*strength,-dy*strength,np.ones_like(height)],axis=-1)
    normal/=np.linalg.norm(normal,axis=-1)[:,:,None]
    return image(name,normal*.5+.5,True)

def fungal_material(name,colour,seed,cap=False):
    n=2048 if cap else 1024;f,u,v=field(n,seed)
    generator=np.random.default_rng(seed+88);grain=generator.random((n,n)).astype(np.float32)
    if cap:
        radial=np.sin(math.tau*(u*139+.40*np.sin(u*math.tau*7)+v*.3))
        radial*=np.clip((v-.64)/.36,0,1)
        pigment=np.clip(.75+f*.65+(grain-.5)*.08,.25,1.1)
        rgb=np.asarray(colour)[None,None,:]*pigment[:,:,None]
        edge=np.clip((v-.83)/.17,0,1)**1.4
        rgb=rgb*(1-edge[:,:,None]*.58)+np.array([.62,.49,.34])*edge[:,:,None]*.58
        rough=np.clip(.66+f*.22+(grain-.5)*.06+edge*.10,.42,.88)
        height=f*.10+radial*.004+(grain-.5)*.003
    else:
        fibre=np.sin(math.tau*(u*170+f*.65+.25*np.sin(v*9+u*math.tau*9)))
        fibre2=np.sin(math.tau*(u*61+.35*np.sin(v*7)))
        pigment=np.clip(.88+f*.28+fibre*.045+fibre2*.04+(grain-.5)*.03,.4,1.1)
        rgb=np.asarray(colour)[None,None,:]*pigment[:,:,None]
        root=(1-v)**3
        rgb=rgb*(1-root[:,:,None]*.30)+np.array([.29,.20,.12])*root[:,:,None]*.30
        rough=np.clip(.70+f*.12+(grain-.5)*.08,.5,.85)
        height=f*.08+fibre*.004+fibre2*.005+(grain-.5)*.002
    return pbr(name,image(name+'_BaseColor',np.clip(rgb,0,1)),
               relief_map(name+'_Normal',height,55 if cap else 30),
               image(name+'_MetallicRoughness',np.stack([np.ones_like(f),rough,np.zeros_like(f)],axis=-1),True))

stipe=fungal_material('Ivory longitudinal hyphae',[.67,.59,.43],831)
gill=fungal_material('Fungal lamellae',[.61,.49,.34],837)
velum=fungal_material('Weathered velum fragments',[.73,.66,.51],839)
eye=plain('Dark moist fungal ocellus',[.008,.006,.004],.19)

def mesh(name,verts,faces,bone,mat,uvs=None,weights=None,smooth=True):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);data.materials.append(mat)
    for p in data.polygons:p.use_smooth=smooth
    uv=data.uv_layers.new()
    if uvs:
        for poly,coords in zip(data.polygons,uvs):
            for loop,coord in zip(poly.loop_indices,coords):uv.data[loop].uv=coord
    else:
        select(ob);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
    if weights is None:ob.vertex_groups.new(name=bone).add(list(range(len(verts))),1,'REPLACE')
    else:
        for name in sorted({k for ws in weights for k in ws}):ob.vertex_groups.new(name=name)
        for i,ws in enumerate(weights):
            total=sum(ws.values())
            for b,w in ws.items():
                if w:ob.vertex_groups[b].add([i],w/total,'REPLACE')
    objects.append(ob);return ob

def tube(name,points,radii,bone,mat,segments=12,ring_weights=None):
    verts=[];faces=[];uvs=[];weights=[]
    points=[Vector(p) for p in points]
    for i,p in enumerate(points):
        tangent=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized()
        ref=Vector((1,0,0)) if abs(tangent.x)<.8 else Vector((0,1,0))
        a=tangent.cross(ref).normalized();b=tangent.cross(a).normalized()
        for j in range(segments):
            angle=j*math.tau/segments;verts.append(p+radii[i]*(a*math.cos(angle)+b*math.sin(angle)))
            if ring_weights:weights.append(ring_weights[i])
    for i in range(len(points)-1):
        for j in range(segments):
            faces.append((i*segments+j,i*segments+(j+1)%segments,(i+1)*segments+(j+1)%segments,(i+1)*segments+j))
            uvs.append([(j/segments,i/(len(points)-1)),((j+1)/segments,i/(len(points)-1)),((j+1)/segments,(i+1)/(len(points)-1)),(j/segments,(i+1)/(len(points)-1))])
    faces += [tuple(reversed(range(segments))),tuple(range((len(points)-1)*segments,len(points)*segments))]
    uvs += [[(.5,.5)]*segments]*2
    return mesh(name,verts,faces,bone,mat,uvs,weights if ring_weights else None)

def body_weights(z,pivots):
    if z<=pivots[0]:return {'Spine00':1}
    for i in range(3):
        if z<=pivots[i+1]:
            t=(z-pivots[i])/(pivots[i+1]-pivots[i]);names=['Spine00','Spine01','Spine02','Head']
            return {names[i]:1-t,names[i+1]:t}
    return {'Head':1}

def stem(name,base,top,radius,pivots,bulge=.35):
    points=[];radii=[];weights=[]
    for i in range(37):
        t=i/36;p=Vector(base).lerp(Vector(top),t);p.x+=radius*.18*math.sin(t*math.pi)
        points.append(p);radii.append(radius*(.78+bulge*math.sin(t*math.pi)+.22*(1-t)**3));weights.append(body_weights(p.z,pivots))
    return tube(name,points,radii,'Spine00',stipe,48,weights)

def cap(name,center,radius,dome,material,seed,ripple=.025,steps=128):
    generator=np.random.default_rng(seed);center=Vector(center)
    def surface(r,theta,lower=False):
        wobble=1+.018*math.sin(theta*5+.7)+.011*math.sin(theta*11+1.8)
        x=radius*r*math.cos(theta)*wobble;y=radius*.94*r*math.sin(theta)*wobble
        z=(.12*radius*(1-r**1.2)-.013*radius) if lower else dome*max(0,1-r*r)**.68
        z+=radius*ripple*(math.sin(theta*5+.5)+.4*math.sin(theta*13))*r**2
        return center+Vector((x,y,z))
    for lower in [False,True]:
        rings=20 if not lower else 10;verts=[surface(0,0,lower)];faces=[];uvs=[]
        for k in range(1,rings+1):
            for j in range(steps):verts.append(surface(k/rings,j*math.tau/steps,lower))
        for j in range(steps):
            faces.append((0,1+j,1+(j+1)%steps));uvs.append([((j+.5)/steps,0),(j/steps,1/rings),((j+1)/steps,1/rings)])
        for k in range(rings-1):
            for j in range(steps):
                faces.append((1+k*steps+j,1+(k+1)*steps+j,1+(k+1)*steps+(j+1)%steps,1+k*steps+(j+1)%steps))
                uvs.append([(j/steps,(k+1)/rings),(j/steps,(k+2)/rings),((j+1)/steps,(k+2)/rings),((j+1)/steps,(k+1)/rings)])
        if lower:faces=[tuple(reversed(f)) for f in faces];uvs=[list(reversed(uv)) for uv in uvs]
        mesh(name+(' underside' if lower else ' cuticle'),verts,faces,'Head',gill if lower else material,uvs)
    # Narrow rounded margin closes the cap and gives the silhouette thickness.
    points=[surface(1,j*math.tau/128) for j in range(129)]
    tube(name+' involute margin',points,[radius*.012]*129,'Head',material,8)
    for j in range(96):
        angle=j*math.tau/96;start=[.10,.27,.16,.43][j%4];verts=[];faces=[];uvs=[]
        for k in range(15):
            r=start+(1-start)*k/14;theta=angle+.007*math.sin(r*10+j*.8)
            p=surface(r,theta,True);side=Vector((-math.sin(theta),math.cos(theta),0))*radius*.0019
            drop=radius*(.026+.029*math.sin(math.pi*r))
            verts.extend([p-side,p+side,p+side-Vector((0,0,drop)),p-side-Vector((0,0,drop))])
        for k in range(14):
            for q in range(4):
                faces.append((k*4+q,k*4+(q+1)%4,(k+1)*4+(q+1)%4,(k+1)*4+q));uvs.append([(q/4,k/14),((q+1)/4,k/14),((q+1)/4,(k+1)/14),(q/4,(k+1)/14)])
        mesh(name+' lamella',verts,faces,'Head',gill,uvs)
    # Broken velum scales sit on the cap; nonuniform placement and silhouettes.
    for j in range(80):
        r=math.sqrt(generator.uniform(.015,.80));theta=generator.uniform(0,math.tau)
        p=surface(r,theta);size=radius*generator.uniform(.012,.034)
        normal=Vector((math.cos(theta)*r,math.sin(theta)*r,.7)).normalized()
        a=normal.cross(Vector((0,1,0))).normalized();b=normal.cross(a)
        verts=[p+normal*size*.35]+[p+(a*math.cos(k*math.tau/7)+b*math.sin(k*math.tau/7))*size*generator.uniform(.75,1.15)+normal*.0005 for k in range(7)]
        mesh(name+' velum scale',verts,[(0,1+k,1+(k+1)%7) for k in range(7)],'Head',velum,smooth=False)

def tendrils(scale,root_z):
    anchors=[]
    for j in range(6):
        # glTF Y-up maps Blender -Y to engine +Z. Keep each named tendril on
        # the radial axis expected by its existing animation channel.
        angle=-j*math.tau/6;start=Vector((.052*scale*math.cos(angle),.052*scale*math.sin(angle),root_z))
        anchors.append(start);points=[]
        for k in range(32):
            t=k/31;radial=.065*scale*math.sin(t*2.0)
            points.append(start+Vector((radial*math.cos(angle)+.011*scale*math.sin(t*8+j),radial*math.sin(angle)+.009*scale*math.sin(t*11),-.14*scale*t)))
        tube('Branching mycelium root',points,[scale*(.009*(1-k/31)**1.4+.0008) for k in range(32)],'Tendril'+str(j),stipe,10)
        for k in [12,18,24]:
            p=points[k];branch=[]
            for t in np.linspace(0,1,10):branch.append(p+Vector((scale*.026*t*math.cos(angle+1),scale*.026*t*math.sin(angle+1),-scale*.025*t)))
            tube('Fine mycelial fork',branch,[scale*(.0018*(1-k/9)+.00035) for k in range(10)],'Tendril'+str(j),stipe,6)
    return anchors

def face(scale,z):
    for side in [-1,1]:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=20,location=(side*.025*scale,-.073*scale,z))
        ob=bpy.context.object;ob.name='Small moist ocellus';ob.scale=(.010*scale,.006*scale,.013*scale)
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        ob.data.materials.append(eye);ob.vertex_groups.new(name='Spine01').add(list(range(len(ob.data.vertices))),1,'REPLACE')
        for p in ob.data.polygons:p.use_smooth=True
        objects.append(ob)

def sporeling():
    pivots=[.18,.28,.39,.42];stem('Sporeling swollen stipe',(0,0,.17),(0,0,.46),.067,pivots,.35)
    mat=fungal_material('Sporeling warm cuticle',[.54,.25,.13],107,True)
    cap('Sporeling button cap',(0,0,.42),.24,.19,mat,107,.017)
    face(1,.31);return pivots,tendrils(1,.18),.66

def duskcap():
    pivots=[.26,.42,.61,.68]
    stem('Duskcap tapered stipe',(0,0,.24),(.022,.008,.70),.078,pivots,.18)
    mat=fungal_material('Duskcap violet umber cuticle',[.32,.19,.23],108,True)
    cap('Duskcap weathered parasol',(.022,.008,.68),.43,.17,mat,108,.038)
    face(1.15,.46);return pivots,tendrils(1.45,.26),.92

def fungrove():
    pivots=[.34,.56,.84,1.01]
    stem('Fungrove old twisted bole',(0,0,.30),(.01,.03,1.04),.12,pivots,.28)
    mat=fungal_material('Fungrove mature canopy',[.26,.16,.19],109,True)
    cap('Fungrove central canopy',(.01,.03,1.01),.51,.24,mat,109,.050)
    for x,y,z,r,seed in [(-.36,.10,.80,.29,110),(.34,.16,.85,.30,111)]:
        stem('Fungrove joined side stipe',(0,.04,.57),(x,y,z+.03),.038,pivots,.2)
        cap('Fungrove side canopy',(x,y,z),r,.13,mat,seed,.04,96)
    face(1.55,.62);return pivots,tendrils(2,.34),1.36

def deliver(name,compose):
    global objects
    for ob in list(scene.objects):bpy.data.objects.remove(ob,do_unlink=True)
    objects=[];pivots,anchors,framing=compose()
    select(objects[0])
    for ob in objects:ob.select_set(True)
    bpy.ops.object.join();body=bpy.context.object;body.name=name
    floor=min(v.co.z for v in body.data.vertices)
    for v in body.data.vertices:v.co.z-=floor
    data=bpy.data.armatures.new(name+' anatomical floater');rig=bpy.data.objects.new(name+' anatomical floater',data);scene.collection.objects.link(rig)
    select(rig);bpy.ops.object.mode_set(mode='EDIT')
    for i,n in enumerate(['Spine00','Spine01','Spine02','Head']):
        b=data.edit_bones.new(n);b.head=(0,0,pivots[i]-floor);b.tail=b.head+Vector((0,0,.04))
        if i:b.parent=data.edit_bones[['Spine00','Spine01','Spine02'][i-1]]
    for j,anchor in enumerate(anchors):
        b=data.edit_bones.new('Tendril'+str(j));b.head=anchor-Vector((0,0,floor));b.tail=b.head+Vector((0,0,.04));b.parent=data.edit_bones['Spine00']
    bpy.ops.object.mode_set(mode='OBJECT');body.parent=rig
    select(body);tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    mod=body.modifiers.new('Anatomical fungal skin','ARMATURE');mod.object=rig
    for v in body.data.vertices:
        if not v.groups or abs(sum(g.weight for g in v.groups)-1)>.0001:raise ValueError('Invalid authored weights')
    rig.select_set(True);folder=OUT/name;folder.mkdir(exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(folder/(name+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_skins=True,export_yup=True,export_tangents=True)
    bpy.data.libraries.write(str(folder/(name+'.blend')),{scene},fake_user=True)
    (folder/'provenance.json').write_text(json.dumps({'source':'original-fungal-authoring','recipe':'Tools/Art/create_fungal_creatures.py','externalSources':[],
        'features':'Original cap, stipe, lamellae, velum and mycelium geometry; ten-joint skin; synthesized pigment, fibre, normal and roughness maps.',
        'visualAcceptance':False,'triangles':len(body.data.polygons)},indent=2))
    print('FUNGAL_SOURCE',name,len(body.data.vertices),len(body.data.polygons),flush=True)
    floor_mat=plain('Fungal review floor',[.11,.12,.10],.9)
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.10));bpy.context.object.data.materials.append(floor_mat)
    for p,power,size in [((2,-3,4),500,3),((-3,-1,2),280,3),((0,2,3),450,2)]:
        bpy.ops.object.light_add(type='AREA',location=tuple(c*framing for c in p));lamp=bpy.context.object
        lamp.data.energy=power*framing*framing;lamp.data.size=size*framing;lamp.rotation_euler=(Vector((0,0,framing*.45))-lamp.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new(name+' review daylight');scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.20,.23,.27,1);scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.3
    bpy.ops.object.camera_add(location=(framing*1.5,-framing*2.7,framing*1.0));cam=bpy.context.object;scene.camera=cam;cam.data.lens=59
    cam.rotation_euler=(Vector((0,0,framing*.45))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=1100;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
    scene.render.filepath=str(folder/(name+'_offline.png'));bpy.ops.render.render(write_still=True)

requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
for name,compose in [('Sporeling',sporeling),('Duskcap',duskcap),('Fungrove',fungrove)]:
    if not requested or name in requested:deliver(name,compose)
