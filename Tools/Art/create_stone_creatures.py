"""Individually composed stone-line sources; run inside Blender's Python.

Outputs are review candidates under Build/artifacts until inspected and installed.
The pinned CC0 photographic maps remain unmodified. No runtime files are written.
"""
from pathlib import Path
import hashlib, json, math, sys
import bpy
import numpy as np
from mathutils import Vector, noise

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Build/artifacts/authored_assets/creature_work/StoneLine'
SRC = ROOT / 'Build/artifacts/authored_assets/reference_sources/PolyHaven_RockFace'
OUT.mkdir(parents=True, exist_ok=True)
HASHES = {
    'diff': '2a16bcca46d1e76f436997120b252a3edb0dc46128090cf58c0d3abb82f1c245',
    'arm': 'f1d4d804b6aa63863bbd32346f9a6605957417c10225f7cb0bf8fc8b13b3cf37',
    'disp': '6f1b83b97237e34accad628e685114613cb4f3c88193b1ff399711c784b79aef',
    'nor_gl': '3e86b0e4e966e2008cb24f393d0b5f32c2bb409900af136a62dbe89650f166d4',
}
for key, digest in HASHES.items():
    assert hashlib.sha256((SRC / f'rock_face_{key}_4k.png').read_bytes()).hexdigest() == digest

scene = bpy.data.scenes.new('Stone lineage source')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.render.engine = 'CYCLES'
scene.cycles.samples = 40
scene.cycles.use_denoising = True
scene.view_settings.view_transform = 'AgX'

def select(ob):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob

def save_image(im):
    im.filepath_raw = str(OUT / (im.name + '.png'))
    im.file_format = 'PNG'
    im.save()
    im.pack()
    return im

def image(name, rgb, data=False):
    rgb = np.asarray(rgb, np.float32)
    if rgb.ndim == 1: rgb = np.tile(rgb, (8, 8, 1))
    h, w = rgb.shape[:2]
    if rgb.shape[-1] == 3: rgb = np.concatenate([rgb, np.ones((h,w,1))], axis=-1)
    im = bpy.data.images.new(name, width=w, height=h, alpha=True)
    im.colorspace_settings.name = 'Non-Color' if data else 'sRGB'
    im.pixels.foreach_set(np.asarray(rgb, np.float32).ravel()); im.update()
    return save_image(im)

maps = {}
for key in HASHES:
    im = bpy.data.images.load(str(SRC / f'rock_face_{key}_4k.png'), check_existing=False)
    im.name = 'RockFace_' + key
    im.colorspace_settings.name = 'sRGB' if key == 'diff' else 'Non-Color'
    im.scale(2048, 2048)
    pixels = np.empty(2048*2048*4,np.float32); im.pixels.foreach_get(pixels)
    if key == 'diff':
        # Loaded sRGB pixels are exposed as scene-linear floats, while a new
        # byte image stores the provided samples directly. Re-encode RGB only;
        # otherwise this optimization accidentally applies a second sRGB decode.
        rgb=pixels.reshape(2048,2048,4)[:,:,:3]
        rgb[:]=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
    # Fresh byte images avoid carrying the source's 16-bit PNG storage into
    # each runtime GLB. The original download stays intact for future rebakes.
    maps[key] = image('RockFace_2K_'+key,pixels.reshape(2048,2048,4),key != 'diff')
height = np.empty(2048*2048*4, np.float32)
maps['disp'].pixels.foreach_get(height)
height = height.reshape(2048,2048,4)[:,:,0]

def pbr(name, base, normal, arm):
    mat = bpy.data.materials.new(name); mat.use_nodes = True
    nodes = mat.node_tree.nodes; links = mat.node_tree.links
    bs = nodes.get('Principled BSDF')
    def tex(im):
        n = nodes.new('ShaderNodeTexImage'); n.image = im
        return n
    links.new(tex(base).outputs['Color'], bs.inputs['Base Color'])
    nm = nodes.new('ShaderNodeNormalMap')
    links.new(tex(normal).outputs['Color'], nm.inputs['Color'])
    links.new(nm.outputs['Normal'], bs.inputs['Normal'])
    packed = tex(arm); sep = nodes.new('ShaderNodeSeparateColor')
    links.new(packed.outputs['Color'], sep.inputs[0])
    links.new(sep.outputs['Green'], bs.inputs['Roughness'])
    links.new(sep.outputs['Blue'], bs.inputs['Metallic'])
    # The glTF exporter recognizes this named socket for occlusion R.
    group = bpy.data.node_groups.get('glTF Material Output')
    if not group:
        group = bpy.data.node_groups.new('glTF Material Output', 'ShaderNodeTree')
        group.interface.new_socket(name='Occlusion', in_out='INPUT', socket_type='NodeSocketFloat')
    ao = nodes.new('ShaderNodeGroup'); ao.node_tree = group
    links.new(sep.outputs['Red'], ao.inputs['Occlusion'])
    return mat

stone = pbr('Weathered photographed iron sandstone', maps['diff'], maps['nor_gl'], maps['arm'])
flat = image('Neutral_normal', [.5,.5,1], True)
def plain(name, colour, rough):
    return pbr(name, image(name+'_BaseColor', colour), flat,
               image(name+'_MetallicRoughness', [1,rough,0], True))
rng = np.random.default_rng(29027)
yy,xx = np.mgrid[0:512,0:512].astype(np.float32)/512
grain = rng.random((512,512)).astype(np.float32)
cloud = .5+.24*np.sin(xx*19+np.sin(yy*13))+.19*np.sin(yy*31+xx*9)
quartz_rgb = np.array([.56,.53,.46],np.float32)[None,None,:]*(.73+.27*cloud+.09*grain)[:,:,None]
quartz = pbr('Milky quartz mineral',image('Quartz_BaseColor',quartz_rgb),flat,
             image('Quartz_MetallicRoughness',np.stack([np.ones_like(xx),.32+.14*grain,np.zeros_like(xx)],axis=-1),True))
dark = plain('Deep mineral fissure', [.019,.013,.009], .89)
eye = plain('Polished smoky quartz eyes', [.032,.019,.006], .21)
iris = plain('Amber mineral iris', [.24,.098,.009], .25)
objects = []

def pin(ob, bone, mat):
    ob.data.materials.clear(); ob.data.materials.append(mat)
    ob.vertex_groups.new(name=bone).add(list(range(len(ob.data.vertices))), 1, 'REPLACE')
    objects.append(ob)
    return ob

def sphere(name, center, radii, bone, mat, segments=48, rings=24):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=center)
    ob = bpy.context.object; ob.name = name; ob.scale = radii
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for p in ob.data.polygons: p.use_smooth = True
    return pin(ob, bone, mat)

def rock(name, center, radii, bone, seed, angular=.12, power=.83):
    """Photographic relief over a weathered, independently shaped stone volume."""
    ob = sphere(name, (0,0,0), (1,1,1), bone, stone, 96, 48)
    for v in ob.data.vertices:
        q = v.co.copy()
        u = (math.atan2(q.y, q.x) / math.tau + .5) % 1
        vv = math.acos(max(-1,min(1,q.z))) / math.pi
        # Slow fracture planes and irregular weathering change the silhouette;
        # the photographic height adds finer non-uniform stone relief.
        h = float(height[int((1-vv)*2047), int(u*2047)])
        low = noise.noise_vector(q*2.1+Vector((seed,seed*.37,seed*.13))).x
        r = 1 + angular*low + .055*(h-.5)
        shaped = Vector(tuple(math.copysign(abs(c)**power,c) for c in q)) * r
        shaped.z = max(shaped.z,-.89)
        v.co = Vector(center) + Vector(tuple(shaped[i]*radii[i] for i in range(3)))
    return ob

def shard(name, center, radius, length, bone, seed, tilt=(0,0,0)):
    verts = []; faces = []
    for z, factor in [(0,.74),(length*.74,1)]:
        for j in range(6):
            angle = j*math.tau/6
            verts.append((math.cos(angle)*radius*factor,math.sin(angle)*radius*factor,z))
    faces.append(tuple(reversed(range(6))))
    verts.append((0,0,length))
    for j in range(6):
        faces.append((j,(j+1)%6,6+(j+1)%6,6+j))
        faces.append((6+j,6+(j+1)%6,12))
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(verts,[],faces); mesh.update()
    ob = bpy.data.objects.new(name,mesh); scene.collection.objects.link(ob)
    ob.location = center; ob.rotation_euler = tilt
    select(ob); bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    pin(ob,bone,quartz)
    select(ob); bevel=ob.modifiers.new('Worn crystal edges','BEVEL'); bevel.width=radius*.06; bevel.segments=2
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.02); bpy.ops.object.mode_set(mode='OBJECT')
    return ob

def vein(surface, points, width, bone):
    # Trace the actual front surface, so the line reads as one mineral fault
    # within the stone instead of detached ornaments in front of it.
    bpy.context.view_layer.update()
    for layer,mat in [(1,quartz)]:
        verts=[]
        for i,(x,z) in enumerate(points):
            pair=[]
            for sign in [-1,1]:
                px=x+sign*width*layer*(.60+.23*math.sin(i*.43)+.18*math.sin(i*1.91+sign))
                hit,p,normal,_=surface.ray_cast(Vector((px,-3,z)),Vector((0,1,0)))
                if not hit:break
                pair.append(p+Vector((0,-.00065,0)))
            if len(pair)==2:verts.extend(pair)
        if len(verts)<4:raise ValueError('Fault missed its stone surface')
        faces=[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(verts)//2-1)]
        mesh=bpy.data.meshes.new('Exposed mineral fault');mesh.from_pydata(verts,[],faces);mesh.update()
        ob=bpy.data.objects.new('Quartz-filled natural fracture',mesh);scene.collection.objects.link(ob);pin(ob,bone,mat)
        uv=mesh.uv_layers.new()
        for poly in mesh.polygons:
            for loop in poly.loop_indices:
                idx=mesh.loops[loop].vertex_index;uv.data[loop].uv=(idx%2,idx//2/(len(verts)//2-1))

def eyes(head, center, radius, gap, bone):
    x,y,z = center
    for side in [-1,1]:
        # Subtract a real shallow socket before placing the mineral eye inset.
        bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,
                                           radius=radius*1.5,location=(x+side*gap,y-.012,z))
        cutter=bpy.context.object
        select(head); mod=head.modifiers.new('Recessed eye socket','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=cutter
        bpy.ops.object.modifier_apply(modifier=mod.name); bpy.data.objects.remove(cutter,do_unlink=True)
        head.vertex_groups[bone].add(list(range(len(head.data.vertices))),1,'REPLACE')
        sphere('Inset smoky quartz eye',(x+side*gap,y+.005,z),(radius,radius*.65,radius*.78),bone,eye)
        sphere('Amber iris',(x+side*gap,y-radius*.49,z),(radius*.45,radius*.10,radius*.43),bone,iris,32,16)
        sphere('Dark eye centre',(x+side*gap,y-radius*.56,z),(radius*.12,radius*.045,radius*.29),bone,eye,24,12)

def rubblet():
    rock('Rubblet planted weathered sole',(0,.006,.093),(.225,.186,.101),'Spine00',27,.10)
    rock('Rubblet lower cobble',(0,.008,.235),(.27,.207,.165),'Spine01',63,.14)
    head=rock('Rubblet rounded cap',(.009,-.013,.398),(.242,.198,.169),'Spine02',85,.10,.94)
    eyes(head,(0,-.204,.397),.027,.079,'Spine02')
    vein(head,[(.026+.016*math.sin(i*.16)+.004*math.sin(i*.73),.26+i*.008) for i in range(37)],.004,'Spine02')
    vein(head,[(.035+i*.004,.43+i*.004) for i in range(14)],.0017,'Spine02')
    shard('Rubblet single quartz bud',(.017,-.013,.545),.025,.061,'Nub',27,(.06,.18,.2))
    return [.045,.18,.34,.54], .63

def cairngo():
    rock('Cairngo broad foundation',(-.026,.015,.115),(.37,.286,.128),'Spine00',72,.17,.78)
    rock('Cairngo balancing shelf',(.043,.014,.322),(.338,.257,.137),'Spine01',95,.19,.76)
    crown=rock('Cairngo offset crown',(-.017,-.021,.58),(.27,.222,.177),'Spine02',113,.17,.81)
    head=rock('Cairngo upper cairn',(.001,-.029,.773),(.214,.185,.12),'Spine02',51,.12,.87)
    eyes(head,(0,-.199,.776),.030,.084,'Spine02')
    vein(crown,[(-.05+i*.002+.017*math.sin(i*.19)+.004*math.sin(i*.83),.43+i*.007) for i in range(41)],.007,'Spine02')
    vein(head,[(.016+i*.001+.013*math.sin(i*.17)+.004*math.sin(i*.7),.68+i*.006) for i in range(36)],.006,'Spine02')
    vein(crown,[(.01-i*.008,.63-i*.003) for i in range(20)],.0028,'Spine02')
    for i in range(3): shard('Cairngo quartz crest',(-.023+i*.029,-.004,.877),.020,.055+i*.013,'Nub',90+i,(.12,(i-1)*.27,0))
    # Smaller naturally broken fragments balance the central three tiers.
    rock('Cairngo shoulder chip',(-.29,.006,.337),(.095,.115,.069),'Spine01',64,.22,.79)
    return [.06,.27,.48,.86], 1.00

def monolode():
    rock('Monolode fractured plinth',(0,.039,.143),(.50,.37,.16),'Spine00',91,.24,.70)
    rock('Monolode great lower slab',(-.018,.011,.428),(.40,.313,.213),'Spine01',120,.21,.63)
    head=rock('Monolode standing heart',(.024,-.014,.887),(.33,.274,.387),'Spine02',153,.20,.60)
    eyes(head,(.015,-.281,.949),.036,.113,'Spine02')
    rock('Monolode broken left buttress',(-.347,.074,.788),(.133,.19,.276),'Spine01',172,.23,.60)
    rock('Monolode broken right buttress',(.358,.050,.699),(.115,.20,.243),'Spine01',183,.23,.63)
    vein(head,[(-.09+.16*i/70+.024*math.sin(i*.17)+.007*math.sin(i*.64),.53+i*.010) for i in range(71)],.011,'Spine02')
    vein(head,[(-.02-i*.006,.92+i*.006) for i in range(35)],.004,'Spine02')
    for i in range(7):
        shard('Monolode exposed quartz lode',(-.075+(i%4)*.045,-.035+(i//4)*.065,1.22),
              .026,.12+(i%3)*.033,'Nub',200+i,((i//4-.5)*.3,(i%4-1.5)*.24,0))
    return [.08,.31,.62,1.20],1.4

def deliver(name, compose):
    global objects
    for ob in list(scene.objects): bpy.data.objects.remove(ob,do_unlink=True)
    objects=[]
    pivots, framing=compose()
    # Keep the four blob names and parent chain, with articulated stone layers.
    select(objects[0])
    for ob in objects: ob.select_set(True)
    bpy.ops.object.join(); body=bpy.context.object;body.name=name
    floor=min(v.co.z for v in body.data.vertices)
    for v in body.data.vertices:v.co.z-=floor
    data=bpy.data.armatures.new(name+' four-bone stone rig')
    rig=bpy.data.objects.new(name+' four-bone stone rig',data);scene.collection.objects.link(rig)
    select(rig);bpy.ops.object.mode_set(mode='EDIT')
    names=['Spine00','Spine01','Spine02','Nub']
    for i,n in enumerate(names):
        b=data.edit_bones.new(n);b.head=(0,0,pivots[i]-floor);b.tail=b.head+Vector((0,0,.04))
        if i:b.parent=data.edit_bones[names[i-1]]
    bpy.ops.object.mode_set(mode='OBJECT')
    body.parent=rig;mod=body.modifiers.new('Stone articulation','ARMATURE');mod.object=rig
    select(body);tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    # Boolean-generated socket vertices need deterministic skin inheritance.
    for v in body.data.vertices:
        if not v.groups: raise ValueError('Socket boolean left an unweighted vertex')
    rig.select_set(True)
    folder=OUT/name;folder.mkdir(exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(folder/(name+'.glb')),export_format='GLB',use_selection=True,
        use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True,export_skins=True)
    bpy.data.libraries.write(str(folder/(name+'.blend')),{scene},fake_user=True)
    (folder/'provenance.json').write_text(json.dumps({'source':'polyhaven-rock-face','license':'CC0-1.0',
        'sourcePage':'https://polyhaven.com/a/rock_face','photographicMapHashes':HASHES,
        'changes':'Original stone creature geometry, quartz seams, recessed eyes, four-joint skin; photographic maps downsampled to 2K.',
        'visualAcceptance':False,'triangles':len(body.data.polygons)},indent=2))
    print('STONE_SOURCE',name,len(body.data.vertices),len(body.data.polygons),flush=True)
    # Offline review lighting; actual engine evidence is captured separately.
    floor_mat=plain('Review grey', [.10,.115,.12], .90)
    bpy.ops.mesh.primitive_plane_add(size=200);bpy.context.object.data.materials.append(floor_mat)
    for p,power,size in [((2,-3,4),500,3),((-3,-1,2),250,3),((0,2,3),400,2)]:
        bpy.ops.object.light_add(type='AREA',location=tuple(c*framing for c in p));lamp=bpy.context.object
        lamp.data.energy=power*framing*framing;lamp.data.size=size*framing
        lamp.rotation_euler=(Vector((0,0,framing*.45))-lamp.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.new(name+' soft daylight');scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.18,.21,.24,1)
    scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.35
    bpy.ops.object.camera_add(location=(framing*1.55,-framing*2.7,framing*1.15))
    cam=bpy.context.object;cam.data.lens=59;scene.camera=cam
    cam.rotation_euler=(Vector((0,0,framing*.45))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=1000;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.render.filepath=str(folder/(name+'_offline.png'));bpy.ops.render.render(write_still=True)

requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
for name,compose in [('Rubblet',rubblet),('Cairngo',cairngo),('Monolode',monolode)]:
    if not requested or name in requested:deliver(name,compose)
