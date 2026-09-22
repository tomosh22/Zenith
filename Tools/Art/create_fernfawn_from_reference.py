"""Adapt the CC0 CDmir deer into individually authored grove-deer sources.

Open the original deer1.blend using Blender --disable-autoexec, then run this
script. The source hash is pinned in source_references.json. Outputs remain in
Build/artifacts until the model, animation fit and PBR surface pass review.
"""
from pathlib import Path
import bpy, numpy as np, math, hashlib, json, sys
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'Build/artifacts/authored_assets/reference_sources/CDmir_Deer/deer1.blend'
arguments=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
NAME=arguments[0] if arguments else 'Fernfawn'
assert NAME in ['Fernfawn','Thicketbuck','Sylvastag'], 'Expected a grove-deer species name'
STAGE=['Fernfawn','Thicketbuck','Sylvastag'].index(NAME)+1
MODEL_SCALE=[1.,1.65,2.35][STAGE-1]
OUT=ROOT/'Build/artifacts/authored_assets/creature_work'/('FernfawnReference' if STAGE==1 else 'GroveDeer/'+NAME)
OUT.mkdir(parents=True,exist_ok=True)
assert Path(bpy.data.filepath).resolve()==SOURCE.resolve(), 'Open the pinned source with --disable-autoexec first'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()=='729f47b10f29e13d128aa2e0a2a5ba47be40cb6593e1e86ac31e236e93bef003'
sys.path.insert(0,str(Path(__file__).resolve().parent))
import read_zenith_rig as wire
RNG=np.random.default_rng(731904+STAGE)
N=2048
old_rig=bpy.data.objects['Armature'];old_rig.data.pose_position='REST'
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
old_heads={b.name:old_rig.matrix_world@b.head_local for b in old_rig.data.bones}
source_images={name:bpy.data.images[name] for name in ['body','head','deer-body-normal.png','deer-head-normal.png']}
SCENE=bpy.data.scenes.new(NAME+' anatomical source')
bpy.context.window.scene=SCENE;SCENE.unit_settings.system='METRIC'
SCENE.render.engine='CYCLES';SCENE.cycles.samples=1;SCENE.render.bake.margin=16

def select(ob):
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob

def point(p,head_weight=0):
    p=Vector(p)*.155
    p.x*=.86+.22*head_weight if STAGE==1 else (1.00 if STAGE==2 else 1.12)
    # The source is an adult: a slightly larger cranium and narrower trunk
    # give the first-stage animal a young silhouette without stretching limbs.
    center=old_heads['Head']*.155
    if STAGE==1:
        p.y-=(p.y-center.y)*.20*head_weight
        p.z+=(p.z-center.z)*.10*head_weight
    return p

def engine(p):return [p.x,p.z,-p.y]

def save_image(im):
    im.filepath_raw=str(OUT/(im.name+'.png'));im.file_format='PNG';im.save();im.pack();return im

def pixels_image(name,values,data=False):
    h,w=values.shape[:2];im=bpy.data.images.new(name,width=w,height=h,alpha=True)
    im.colorspace_settings.name='Non-Color' if data else 'sRGB'
    if values.shape[-1]==3:values=np.concatenate([values,np.ones((h,w,1))],axis=-1)
    im.pixels.foreach_set(np.asarray(values,np.float32).ravel());im.update();return save_image(im)

def pbr(name,base,normal,rm):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links
    bs=n.get('Principled BSDF')
    def texture(im):
        t=n.new('ShaderNodeTexImage');t.image=im;return t
    l.new(texture(base).outputs['Color'],bs.inputs['Base Color'])
    nm=n.new('ShaderNodeNormalMap');l.new(texture(normal).outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
    sep=n.new('ShaderNodeSeparateColor');l.new(texture(rm).outputs['Color'],sep.inputs[0]);l.new(sep.outputs['Green'],bs.inputs['Roughness']);l.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    return mat

flat=pixels_image('Neutral_tangent_normal',np.tile([.5,.5,1.],(4,4,1)),True)
def plain(name,colour,rough):
    return pbr(name,pixels_image(name+'_BaseColor',np.tile(colour,(8,8,1))),flat,
               pixels_image(name+'_MetallicRoughness',np.tile([1,rough,0],(8,8,1)),True))

# Source L/R uses the opposite X convention to the existing Zenith quadruped.
mapping={'Hip':'Spine00','Backbone':'Spine01','Backbone.001':'Spine02','Backbone.002':'Spine03','Head':'Head'}
for side,letter in [('L','R'),('R','L')]:
    for source,target in [('Frontleg','F'),('Backleg','H')]:
        mapping[source+'_'+side]='Leg'+target+letter+'Up'
        mapping[source+'_'+side+'.001']='Leg'+target+letter+'Lo'
        mapping[source+'_'+side+'.002']='Leg'+target+letter+'Lo'
    mapping['Head_'+side+'.001']='Head'

objects=[]
for name in ['Body','Head','Eyes']:
    source=bpy.data.objects[name];ob=source.copy();ob.data=source.data.copy();ob.name=NAME+' '+name;ob.parent=None;ob.animation_data_clear()
    SCENE.collection.objects.link(ob);select(ob)
    world=source.matrix_world.copy();ob.matrix_world.identity()
    for mod in list(ob.modifiers):
        if mod.type=='ARMATURE':ob.modifiers.remove(mod)
        else:bpy.ops.object.modifier_apply(modifier=mod.name)
    old_groups=[g.name for g in ob.vertex_groups]
    weights=[]
    for v in ob.data.vertices:
        raw={old_groups[g.group]:g.weight for g in v.groups}
        hw=sum(w for n,w in raw.items() if n.startswith('Head'))
        v.co=point(world@v.co,hw)
        remapped={}
        for n,w in raw.items():
            if n in mapping:remapped[mapping[n]]=remapped.get(mapping[n],0)+w
        if not remapped:raise ValueError('Unweighted source vertex '+name)
        total=sum(remapped.values());weights.append({n:w/total for n,w in remapped.items()})
    ob.vertex_groups.clear()
    for n in sorted(set(n for ws in weights for n in ws)):ob.vertex_groups.new(name=n)
    for i,ws in enumerate(weights):
        for n,w in ws.items():ob.vertex_groups[n].add([i],w,'REPLACE')
    # Retain authored UVs, topology and crease intent while removing faceting.
    sub=ob.modifiers.new('Anatomical surface subdivision','SUBSURF');sub.levels=2;sub.subdivision_type='CATMULL_CLARK';bpy.ops.object.modifier_apply(modifier=sub.name)
    for poly in ob.data.polygons:poly.use_smooth=True
    objects.append(ob)

# Bake colour and surface positions per original UV layout. Repaint the young
# animal's coat at texel resolution; embedded source texture detail is retained.
def coat_material(ob,source_colour,source_normal,name):
    src=bpy.data.materials.new(name+' bake source');src.use_nodes=True;n=src.node_tree.nodes;l=src.node_tree.links;n.clear()
    output=n.new('ShaderNodeOutputMaterial');emit=n.new('ShaderNodeEmission');l.new(emit.outputs[0],output.inputs['Surface'])
    tex=n.new('ShaderNodeTexImage');tex.image=source_colour;tex.image.colorspace_settings.name='sRGB'
    l.new(tex.outputs['Color'],emit.inputs['Color'])
    target=n.new('ShaderNodeTexImage');base=bpy.data.images.new(name+'_BaseColor',width=N,height=N);target.image=base;n.active=target
    # Each object uses a single anatomical texture atlas; the original unused
    # material slots are discarded to prevent accidental UV/material crossover.
    ob.data.materials.clear();ob.data.materials.append(src)
    for p in ob.data.polygons:p.material_index=0
    select(ob);bpy.ops.object.bake(type='EMIT')
    rgb=np.empty(N*N*4,np.float32);base.pixels.foreach_get(rgb);rgb=rgb.reshape(N,N,4)
    geom=n.new('ShaderNodeNewGeometry');encoded=n.new('ShaderNodeVectorMath');encoded.operation='MULTIPLY_ADD';encoded.inputs[1].default_value=(.5,.5,.5);encoded.inputs[2].default_value=(.5,.5,.5);l.new(geom.outputs['Position'],encoded.inputs[0]);l.new(encoded.outputs[0],emit.inputs['Color'])
    positions=bpy.data.images.new(name+' surface positions temporary',width=N,height=N,float_buffer=True);positions.colorspace_settings.name='Non-Color';target.image=positions;n.active=target
    bpy.ops.object.bake(type='EMIT');values=np.empty(N*N*4,np.float32);positions.pixels.foreach_get(values);p=values.reshape(N,N,4)[:,:,:3]*2-1
    if name=='Coat':
        # Remove broad illumination/pigment bands from the legacy body diffuse
        # while retaining fine photographic hair detail. Normalized convolution
        # prevents black UV gutters from brightening the island borders.
        lum=rgb[:,:,:3]@np.array([.2126,.7152,.0722]);mask=(rgb[:,:,:3].sum(axis=2)>1e-5).astype(float)
        freq=np.fft.fftfreq(N);kernel=np.exp(-2*math.pi**2*8**2*(freq[:,None]**2+freq[None,:]**2))
        blur=lambda a:np.fft.ifft2(np.fft.fft2(a)*kernel).real
        low=blur(lum)/np.maximum(blur(mask),1e-4)
        detail=1+.55*(np.clip(lum/np.maximum(low,.01),.52,1.55)-1)
        x=p[:,:,0];y=p[:,:,1];z=p[:,:,2];spots=np.zeros((N,N),np.float32)
        pigment=np.tile(np.array([[.30,.178,.086],[.25,.145,.064],[.19,.109,.048]][STAGE-1]),(N,N,1))
        neck_blend=np.clip((z-.50)/.16,0,1)*np.clip((-y-.16)/.08,0,1)
        pigment=pigment*(1-neck_blend[:,:,None])+np.array([.42,.29,.16])*neck_blend[:,:,None]
        pigment*= (1-.16*np.exp(-(x/.035)**2)*np.exp(-((z-.55)/.06)**2))[:,:,None]
        belly=np.exp(-(x/.078)**4)*np.exp(-((z-.357)/.058)**4)*np.exp(-(y/.32)**6)
        throat=np.exp(-(x/.043)**4)*np.exp(-((y+.275)/.062)**4)*np.exp(-((z-.54)/.14)**4)
        cream=np.maximum(belly,throat)*.82
        pigment=pigment*(1-cream[:,:,None])+np.array([.55,.43,.28])*cream[:,:,None]
        rgb[:,:,:3]=pigment*detail[:,:,None]
        for row in range(3):
            for j in range(7):
                cy=-.125+j*.054+(row%2)*.024+RNG.uniform(-.009,.009);cz=.455+row*.029+.009*math.sin(j*1.7)+RNG.uniform(-.005,.005)
                ry=.007+RNG.random()*.004;rz=.005+RNG.random()*.003
                radius=((y-cy)/ry)**2+((z-cz)/rz)**2
                spots=np.maximum(spots,np.clip((1-radius+.1*np.sin(y*900+z*550))*2,0,1))
        spots*=np.clip((np.abs(x)-.035)/.032,0,1)*[1.,.20,0.][STAGE-1]
        rgb[:,:,:3]=rgb[:,:,:3]*(1-spots[:,:,None]*.8)+np.array([.62,.49,.31])*spots[:,:,None]*.8
        hoof=np.clip((.042-z)/.008,0,1)
        rgb[:,:,:3]=rgb[:,:,:3]*(1-hoof[:,:,None])+np.array([.048,.037,.026])*hoof[:,:,None]
    elif STAGE>1:
        # Equalise legacy painted lighting over the facial fur, retaining the
        # source muzzle detail. This avoids a pale atlas-shaped seam at the neck.
        lum=rgb[:,:,:3]@np.array([.2126,.7152,.0722]);mask=(lum>1e-5).astype(float)
        freq=np.fft.fftfreq(N);kernel=np.exp(-2*math.pi**2*10**2*(freq[:,None]**2+freq[None,:]**2))
        blur=lambda a:np.fft.ifft2(np.fft.fft2(a)*kernel).real
        low=blur(lum)/np.maximum(blur(mask),1e-4)
        detail=1+.70*(np.clip(lum/np.maximum(low,.01),.40,1.65)-1)
        pigment=np.array([.38,.255,.132] if STAGE==2 else [.34,.22,.115])
        fur=pigment[None,None,:]*detail[:,:,None]
        muzzle=np.clip((-p[:,:,1]-.455)/.055,0,1)
        rgb[:,:,:3]=fur*(1-muzzle[:,:,None])+rgb[:,:,:3]*muzzle[:,:,None]
    base.pixels.foreach_set(rgb.ravel());base.update();save_image(base);bpy.data.images.remove(positions)
    source_normal.colorspace_settings.name='Non-Color'
    normal=source_normal.copy();normal.name=name+'_Normal'
    source_pixels=np.empty(normal.size[0]*normal.size[1]*4,np.float32);normal.pixels.foreach_get(source_pixels)
    normal_values=source_pixels.reshape(normal.size[1],normal.size[0],4)
    xy=(normal_values[:,:,:2]*2-1)*(.20 if name=='Coat' else .38)
    normal_values[:,:,:2]=xy*.5+.5;normal_values[:,:,2]=np.sqrt(np.maximum(0,1-(xy*xy).sum(axis=2)))*.5+.5
    normal.pixels.foreach_set(normal_values.ravel());normal.update();save_image(normal)
    rough=.86+np.clip(np.linalg.norm(source_pixels.reshape(normal.size[1],normal.size[0],4)[:,:,:2]-.5,axis=2),0,.2)*.35
    rm=pixels_image(name+'_MetallicRoughness',np.stack([np.ones_like(rough),rough,np.zeros_like(rough)],axis=-1),True)
    mat=pbr(name+' short fur',base,normal,rm);ob.data.materials.clear();ob.data.materials.append(mat)
    return mat

body_mat=coat_material(objects[0],source_images['body'],source_images['deer-body-normal.png'],'Coat')
head_mat=coat_material(objects[1],source_images['head'],source_images['deer-head-normal.png'],'Face')
eyes=objects[2];eyes.data.materials.clear();eyes.data.materials.append(plain('Dark wet cornea',[.006,.004,.002],.16))
for p in eyes.data.polygons:p.material_index=0

def guard_hair(surface,count,length_range):
    """Sparse silhouette fibres share the surface UV colour and interpolated skin."""
    surface.data.calc_loop_triangles();tris=surface.data.loop_triangles
    area=np.array([t.area for t in tris]);area/=area.sum()
    verts=[];faces=[];uvs=[];skin=[];layer=surface.data.uv_layers.active.data
    for idx in RNG.choice(len(tris),size=count,p=area):
        t=tris[idx];vs=[surface.data.vertices[i] for i in t.vertices]
        a=math.sqrt(RNG.random());b=RNG.random();bc=[1-a,a*(1-b),a*b]
        p=sum((v.co*w for v,w in zip(vs,bc)),Vector())
        if p.z<.055:continue
        # Keep muzzle, cornea and ear interior clean; the face atlas supplies
        # their short down. Longer fibres belong to the cheek and nape.
        if surface.name.endswith('Head') and (p.y<-.405 or abs(p.x)>.08):continue
        normal=sum((v.normal*w for v,w in zip(vs,bc)),Vector()).normalized()
        flow=Vector((0,1,-.3)) if p.z>.35 else Vector((0,.1,-1))
        tangent=flow-normal*flow.dot(normal)
        if tangent.length<.01:tangent=normal.cross(Vector((1,0,0)))
        tangent.normalize();cross=normal.cross(tangent).normalized()
        length=RNG.uniform(*length_range);width=RNG.uniform(.00007,.00013)
        root=p+normal*.00007;mid=root+tangent*length*.45+normal*length*.35;tip=root+tangent*length+normal*length*.24
        first=len(verts);verts.extend([root-cross*width,root+cross*width,mid-cross*width*.5,mid+cross*width*.5,tip])
        faces.extend([(first,first+1,first+3,first+2),(first+2,first+3,first+4)])
        uv=sum((layer[i].uv*w for i,w in zip(t.loops,bc)),Vector((0,0)))
        # A small, nonzero UV footprint permits a well-defined tangent basis.
        uvs.extend([uv+Vector(offset) for offset in [(-.0001,0),(.0001,0),(-.00005,.0004),(.00005,.0004),(0,.0009)]])
        weights={}
        for v,w in zip(vs,bc):
            for g in v.groups:
                key=surface.vertex_groups[g.group].name;weights[key]=weights.get(key,0)+w*g.weight
        strongest=sorted(weights.items(),key=lambda pair:-pair[1])[:4];total=sum(w for _,w in strongest)
        skin.append((first,[(n,w/total) for n,w in strongest]))
    mesh=bpy.data.meshes.new(surface.name+' fibres');mesh.from_pydata(verts,[],faces);mesh.update()
    hair=bpy.data.objects.new(mesh.name,mesh);SCENE.collection.objects.link(hair)
    mesh.materials.append(surface.data.materials[0]);uv=mesh.uv_layers.new(name='UVMap')
    for loop in mesh.loops:uv.data[loop.index].uv=uvs[loop.vertex_index]
    for group in surface.vertex_groups:hair.vertex_groups.new(name=group.name)
    for first,weights in skin:
        for n,w in weights:hair.vertex_groups[n].add(list(range(first,first+5)),w,'REPLACE')
    for poly in mesh.polygons:poly.use_smooth=True
    objects.append(hair)

guard_hair(objects[0],11000 if STAGE==1 else 23000,(.0018,.004) if STAGE==1 else (.0025,.006))
guard_hair(objects[1],2200,(.001,.0025))

# Anatomical rest points retain the same 18 names and hierarchy as the game's
# quadruped, but pivots now sit in this model's actual joints.
template=wire.skeleton(ROOT/'Games/Zenithmon/Assets/Creatures/Fernfawn/Fernfawn.zskel')
heads={target:point(old_heads[source],1 if target=='Head' else 0) for source,target in mapping.items() if not (source.endswith('.002') and 'leg' in source) and not source.startswith('Head_')}
for side,letter in [('L','R'),('R','L')]:
    p=point(old_heads['Head_'+side+'.001'],1)
    if STAGE>1:p.x*=.65;p.y-=.055;p.z+=.008
    heads['Horn'+letter]=p
hip=heads['Spine00'];heads['Tail00']=hip+Vector((0,.075,.015));heads['Tail01']=hip+Vector((0,.11,.01));heads['Tail02']=hip+Vector((0,.13,0))

def geometry(name,verts,faces,bone,material):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update();ob=bpy.data.objects.new(name,mesh);SCENE.collection.objects.link(ob)
    ob.vertex_groups.new(name=bone).add(list(range(len(verts))),1,'REPLACE');mesh.materials.append(material)
    for p in mesh.polygons:p.use_smooth=True
    select(ob);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')
    objects.append(ob);return ob

yy,xx=np.mgrid[0:256,0:256]/255
midrib=np.exp(-((yy-.5)/.016)**2)
veins=np.exp(-(np.sin((xx-np.abs(yy-.5)*.48)*math.pi*13)/.17)**2)
green=np.empty((256,256,3));green[:]=[.035,.145,.014]
green*= (.8+.24*np.sin(xx*math.pi)+.12*veins)[:,:,None]
green=green*(1-midrib[:,:,None]*.5)+np.array([.17,.26,.031])*midrib[:,:,None]*.5
leaf_mat=pbr('Fern pinnae',pixels_image('Fern_BaseColor',green),flat,pixels_image('Fern_MetallicRoughness',np.tile([1,.63,0],(8,8,1)),True))
stem_mat=plain('Living rachis',[.068,.11,.019],.65)
crown_starts=[]
if STAGE==1:
    crown_starts=[(side,letter,heads['Horn'+letter],1.) for side,letter in [(-1,'L'),(1,'R')]]
else:
    # Adult antlers are shaped swept branches, with an irregular ridged cortex
    # and smooth polished tips. Fern pinnae remain the lineage's shared feature.
    y,x=np.mgrid[0:1024,0:1024].astype(np.float32)/1024
    grain=RNG.random((1024,1024)).astype(np.float32)
    ridges=np.sin(math.tau*(x*43+.25*np.sin(y*13+x*math.tau*3)))
    variation=.92+.055*ridges+.07*np.sin(x*math.tau*7+y*17)+(grain-.5)*.09
    colour=np.array([.29,.20,.105])[None,None,:]*(1-y[:,:,None])+np.array([.69,.58,.39])[None,None,:]*y[:,:,None]
    colour*=variation[:,:,None]
    height=ridges*.008+(grain-.5)*.008
    dy,dx=np.gradient(height);normal=np.stack([-dx*26,-dy*26,np.ones_like(x)],axis=-1);normal/=np.linalg.norm(normal,axis=-1)[:,:,None]
    rough=np.clip(.80-y*.24+(grain-.5)*.06,.45,.9)
    antler_mat=pbr('Weathered antler cortex',pixels_image('Antler_BaseColor',colour),
        pixels_image('Antler_Normal',normal*.5+.5,True),
        pixels_image('Antler_MetallicRoughness',np.stack([np.ones_like(x),rough,np.zeros_like(x)],axis=-1),True))

    def antler_branch(name,path,radii,bone):
        verts=[];faces=[];uvs=[];steps=16
        for i,p in enumerate(path):
            tangent=(path[min(i+1,len(path)-1)]-path[max(0,i-1)]).normalized()
            reference=Vector((0,1,0)) if abs(tangent.y)<.85 else Vector((1,0,0))
            a=tangent.cross(reference).normalized();b=tangent.cross(a)
            for j in range(steps):
                angle=j*math.tau/steps
                radius=radii[i]*(1+.055*math.sin(angle*7+i*.18)+.025*math.sin(angle*11-i*.12))
                verts.append(p+radius*(a*math.cos(angle)+b*math.sin(angle)))
        for i in range(len(path)-1):
            for j in range(steps):
                faces.append((i*steps+j,i*steps+(j+1)%steps,(i+1)*steps+(j+1)%steps,(i+1)*steps+j))
                uvs.append([(j/steps,i/(len(path)-1)),((j+1)/steps,i/(len(path)-1)),((j+1)/steps,(i+1)/(len(path)-1)),(j/steps,(i+1)/(len(path)-1))])
        faces.extend([tuple(reversed(range(steps))),tuple(range((len(path)-1)*steps,len(path)*steps))]);uvs.extend([[ (.5,.5)]*steps]*2)
        ob=geometry(name,verts,faces,bone,antler_mat)
        for poly,coords in zip(ob.data.polygons,uvs):
            for loop,coord in zip(poly.loop_indices,coords):ob.data.uv_layers.active.data[loop].uv=coord
    for side,letter in [(-1,'L'),(1,'R')]:
        start=heads['Horn'+letter];bone='Horn'+letter
        height=.23 if STAGE==2 else .33;spread=.095 if STAGE==2 else .17
        path=[]
        for k in range(35):
            t=k/34;path.append(start+Vector((side*(spread*t+.025*math.sin(t*math.pi)),.055*t-.025*math.sin(t*math.pi),height*t)))
        antler_branch('Swept antler beam '+letter,path,[.012*(1-k/34)**.75+.0008 for k in range(35)],bone)
        crown_starts.append((side,letter,path[-4],.62 if STAGE==2 else .82))
        for j,k in enumerate([9,19] if STAGE==2 else [7,14,22,28]):
            base=path[k];branch=[];length=(.095 if STAGE==2 else .135)*(1-j*.10)
            for q in range(24):
                t=q/23;branch.append(base+Vector((side*length*.42*t,-length*.65*math.sin(t*math.pi*.6),length*t)))
            antler_branch('Antler tine '+letter+str(j),branch,[.006*(1-q/23)**.8+.00045 for q in range(24)],bone)
            if j%2==0:crown_starts.append((side,letter,branch[-3],.52 if STAGE==2 else .65))
        # Fuse the cortex at the branch crotches, removing intersecting tube
        # collars. Each whole antler belongs to one horn joint, so remeshing
        # cannot lose a blended skin region.
        parts=[ob for ob in objects if ob.data.materials[0]==antler_mat and ob.vertex_groups.get(bone)]
        select(parts[0])
        for ob in parts:ob.select_set(True)
        bpy.ops.object.join();antler=bpy.context.object
        objects=[ob for ob in objects if ob not in parts]+[antler]
        remesh=antler.modifiers.new('Joined antler cortex','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.0011;remesh.use_smooth_shade=True
        bpy.ops.object.modifier_apply(modifier=remesh.name)
        smooth=antler.modifiers.new('Rounded branch junctions','SMOOTH');smooth.factor=.7;smooth.iterations=3;bpy.ops.object.modifier_apply(modifier=smooth.name)
        decimate=antler.modifiers.new('Cortex delivery topology','DECIMATE');decimate.ratio=.55;bpy.ops.object.modifier_apply(modifier=decimate.name)
        antler.vertex_groups.clear();antler.vertex_groups.new(name=bone).add(list(range(len(antler.data.vertices))),1,'REPLACE')
        for layer in list(antler.data.uv_layers):antler.data.uv_layers.remove(layer)
        uv=antler.data.uv_layers.new(name='UVMap')
        for poly in antler.data.polygons:
            coords=[]
            for loop in poly.loop_indices:
                p=antler.data.vertices[antler.data.loops[loop].vertex_index].co-start
                coords.append([math.atan2(p.y,p.x)/math.tau+.5,p.z/(height+.03)])
            if max(u for u,v in coords)-min(u for u,v in coords)>.5:
                coords=[[u+1 if u<.5 else u,v] for u,v in coords]
            for loop,coord in zip(poly.loop_indices,coords):uv.data[loop].uv=coord
            poly.use_smooth=True

for side,letter,start,frond_size in crown_starts:
    centers=[];verts=[];rings=10
    for i in range(20):
        t=i/19;curl=max(0,(t-.76)/.24)
        p=start+Vector((side*.018*t,.004*t+.016*(1-math.cos(curl*math.pi)),.11*t-.012*math.sin(curl*math.pi)))*frond_size
        centers.append(p);radius=.0017*(1-t)+.00055
        for k in range(rings):verts.append(p+Vector((math.cos(k*math.tau/rings)*radius,math.sin(k*math.tau/rings)*radius,0)))
    faces=[tuple(reversed(range(rings)))]+[(i*rings+k,i*rings+(k+1)%rings,(i+1)*rings+(k+1)%rings,(i+1)*rings+k) for i in range(19) for k in range(rings)]+[tuple(range(19*rings,20*rings))]
    geometry('Unfurling fern rachis '+letter,verts,faces,'Horn'+letter,stem_mat)
    for j in range(2,15):
        t=j/19;center=centers[j];length=.032*math.sin(math.pi*t)**.6*frond_size
        for direction in [-1,1]:
            verts=[]
            for k in range(13):
                s=k/12;width=.0045*math.sin(math.pi*s)**.65*(1 if k%2==0 else .72)*frond_size
                mid=center+Vector((direction*length*s,-.004*math.sin(s*math.pi),.009*s))
                verts.extend([mid+Vector((0,.0007*math.sin(s*math.pi),width)),mid-Vector((0,.0007*math.sin(s*math.pi),width))])
            faces=[(2*k,2*k+1,2*k+3,2*k+2) for k in range(12)]
            leaf=geometry('Serrated fern pinna',verts,faces,'Horn'+letter,leaf_mat)
            for poly in leaf.data.polygons:
                for loop in poly.loop_indices:
                    vertex=leaf.data.loops[loop].vertex_index
                    leaf.data.uv_layers.active.data[loop].uv=(vertex//2/12,vertex%2)
            select(leaf);solid=leaf.modifiers.new('Pinna thickness','SOLIDIFY');solid.thickness=.0003;bpy.ops.object.modifier_apply(modifier=solid.name)

# Join only the authored creature, retaining material sections and vertex groups.
for ob in objects:
    primary=ob.data.uv_layers.active
    for layer in list(ob.data.uv_layers):
        if layer!=primary:ob.data.uv_layers.remove(layer)
    primary.name='UVMap'
bpy.ops.object.select_all(action='DESELECT')
for ob in objects:ob.select_set(True)
bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();body=bpy.context.object;body.name=NAME
for v in body.data.vertices:v.co*=MODEL_SCALE
heads={name:p*MODEL_SCALE for name,p in heads.items()}
data=bpy.data.armatures.new(NAME+' anatomical quadruped');rig=bpy.data.objects.new(NAME+' anatomical quadruped',data);SCENE.collection.objects.link(rig)
select(rig);bpy.ops.object.mode_set(mode='EDIT')
for bone in template:
    b=data.edit_bones.new(bone['name']);b.head=heads[bone['name']];b.tail=b.head+Vector((0,0,.04))
    if bone['parent']>=0:b.parent=data.edit_bones[template[bone['parent']]['name']]
bpy.ops.object.mode_set(mode='OBJECT');body.parent=rig;mod=body.modifiers.new('Anatomical skin','ARMATURE');mod.object=rig
select(body);tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
for material_index in range(len(body.data.materials)):
    coords=[body.data.uv_layers.active.data[loop].uv[:] for p in body.data.polygons if p.material_index==material_index for loop in p.loop_indices]
    if coords and max(np.ptp(np.array(coords),axis=0))<1e-5:raise ValueError('Collapsed UV atlas: '+body.data.materials[material_index].name)
for v in body.data.vertices:
    valid=sorted([(g.group,g.weight) for g in v.groups if g.weight>1e-8],key=lambda g:-g[1])[:4];total=sum(w for _,w in valid)
    if total<=0:raise ValueError('Unweighted export vertex')
    for group in body.vertex_groups:group.remove([v.index])
    for group,w in valid:body.vertex_groups[group].add([v.index],w/total,'REPLACE')
rig.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(OUT/(NAME+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True,export_skins=True)
bpy.data.libraries.write(str(OUT/(NAME+'.blend')),{SCENE},fake_user=True)
(OUT/'anatomical_rig.json').write_text(json.dumps([{'name':b['name'],'parent':b['parent'],'position':engine(heads[b['name']])} for b in template],indent=2))
(OUT/'provenance.json').write_text(json.dumps({'base':'cdmir-old-deer-male','license':'CC0-1.0','sourcePage':'https://opengameart.org/content/old-deer-male','species':NAME,'stage':STAGE,'changes':'Species proportions, surface subdivision, coat markings and guard hairs, modern metallic/roughness materials, individually composed antlers and fern crown, 18-joint anatomical rig and skin remapping','visualAcceptance':False},indent=2))
print('REFERENCE_DERIVED',NAME,len(body.data.vertices),'vertices',len(body.data.polygons),'triangles',flush=True)

# Offline review only. Actual engine captures are produced by the C++ harness.
floor_mat=plain('Review floor',[.10,.12,.085],.92)
bpy.ops.mesh.primitive_plane_add(size=200);bpy.context.object.data.materials.append(floor_mat)
for position,power,size in [((1,-1.6,2),110,1.4),((-1,.1,1.4),45,1.2),((0,1,1.8),80,1)]:
    bpy.ops.object.light_add(type='AREA',location=tuple(v*MODEL_SCALE for v in position));ob=bpy.context.object;ob.data.energy=power*MODEL_SCALE**2;ob.data.shape='DISK';ob.data.size=size*MODEL_SCALE;ob.rotation_euler=(Vector((0,0,.4))*MODEL_SCALE-ob.location).to_track_quat('-Z','Y').to_euler()
SCENE.world=bpy.data.worlds.new('Review daylight');SCENE.world.use_nodes=True;SCENE.world.node_tree.nodes.get('Background').inputs[0].default_value=(.20,.23,.27,1);SCENE.world.node_tree.nodes.get('Background').inputs[1].default_value=.4
lo=min(v.co.z for v in body.data.vertices);hi=max(v.co.z for v in body.data.vertices)
framing=hi-lo;target=Vector((0,0,(hi+lo)*.5))
bpy.ops.object.camera_add(location=target+Vector((1.25,-1.75,.35))*framing);cam=bpy.context.object;SCENE.camera=cam;cam.data.lens=61;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
SCENE.render.resolution_x=1200;SCENE.render.resolution_y=1200;SCENE.render.resolution_percentage=100;SCENE.cycles.samples=48;SCENE.cycles.use_denoising=True;SCENE.view_settings.view_transform='AgX';SCENE.render.filepath=str(OUT/(NAME+'_three_quarter.png'));bpy.ops.render.render(write_still=True)
cam.location=target+Vector((2.05,.03,.12))*framing;cam.data.lens=55;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();SCENE.render.filepath=str(OUT/(NAME+'_profile.png'));bpy.ops.render.render(write_still=True)
