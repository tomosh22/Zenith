"""Anatomical mineral-beetle study from the attributed Artec specimen scan.

Blender --background --factory-startup --disable-autoexec --python-exit-code 1
--python Tools/Art/create_scanned_mineral_beetles.py -- Prismite
Outputs stay separate from installed sources pending skin and visual review.
"""
from pathlib import Path
import bpy, numpy as np, sys, hashlib, json, shutil
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_mineral_beetles as art

ROOT=art.ROOT
SOURCE=ROOT/'Build/artifacts/authored_assets/reference_sources/Artec_Beetle'
assert hashlib.sha256((SOURCE/'beetle_obj.zip').read_bytes()).hexdigest()=='ef04e47bbb0712072dd8877b0a9f1d5a6738a0b457f22f78722103f579441140'
for filename,digest in {
    'Beetle.obj':'70a623ef67b82d87ff391e52fdd3fdf2620c7ed52941af5139351cecd93ba3d1',
    'Beetle_1.jpg':'ee81439e3851f0bf4d31f241f3cda8b676b2309763024ddb8c95bc14d45589d0',
    'license.txt':'4211d679dceb5d5e7d11541db839c2cbafcd7a5fa746c903da36b30c369178cc',
}.items():
    assert hashlib.sha256((SOURCE/'extracted'/filename).read_bytes()).hexdigest()==digest,filename+' differs from the pinned source'
art.OUT=ROOT/'Build/artifacts/authored_assets/creature_work/MineralBeetlesScan'
art.OUT.mkdir(parents=True,exist_ok=True)

def scanned_material(name):
    # The supplied photograph has no measured roughness or normals. Retain
    # its pigment detail, suppress its broad exposure range, and author a
    # conservative dielectric response. Do not mislabel these as measured PBR.
    im=bpy.data.images.load(str(SOURCE/'extracted/Beetle_1.jpg'),check_existing=False)
    im.colorspace_settings.name='Non-Color'
    im.scale(2048,2048);w,h=im.size
    pix=np.empty(w*h*4,np.float32);im.pixels.foreach_get(pix)
    encoded=pix.reshape(h,w,4)[:,:,:3]
    rgb=np.where(encoded<=.04045,encoded/12.92,((encoded+.055)/1.055)**2.4)
    lum=rgb@np.array([.2126,.7152,.0722],np.float32)
    # Compress bright photographic reflections without replacing spatial detail.
    rgb=rgb/(1+np.maximum(0,lum-.25)[:,:,None]*1.8)
    rgb=np.clip(rgb*.78,0,1)
    srgb=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055)
    rough=np.clip(.49+.12*(1-np.sqrt(np.clip(lum,0,1))),.49,.61)
    # Neutral normal: the scanner's 300k triangle surface supplies the relief.
    return art.pbr(name,art.image(name+' edited photo colour',srgb),art.flat,
        art.image(name+' authored MR',np.stack([np.ones_like(lum),rough,np.zeros_like(lum)],axis=2),True))

def compose(name,stage):
    bpy.ops.wm.obj_import(filepath=str(SOURCE/'extracted/Beetle.obj'))
    ob=bpy.context.object;ob.name=name+' adapted specimen'
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    raw=np.array([v.co[:] for v in ob.data.vertices],np.float64)
    names=['Spine00','Spine01','Spine02','Spine03','Head'];parents=[-1,0,1,2,3]
    pivots=[(11,0,0),(4,0,0),(-1,0,0),(-4,0,0),(-8,0,0)]
    for side,letter in [(1,'L'),(-1,'R')]:
        for row,(hip,knee) in enumerate([
            ((-4.5,0,2.7),(-6.7,.5,6.8)),
            ((-1.0,0,3.1),(-2.5,.6,6.4)),
            ((2.0,0,3.3),(7.7,.4,5.5))]):
            names.extend([f'Leg{letter}{row}Up',f'Leg{letter}{row}Lo']);parents.extend([[3,2,1][row],len(names)-2])
            pivots.extend([(hip[0],hip[1],side*hip[2]),(knee[0],knee[1],side*knee[2])])
    names.extend(['AntennaL','AntennaR']);parents.extend([4,4]);pivots.extend([(-11,.1,1.2),(-11,.1,-1.2)])
    scale=[.36,.76,1.13][stage-1]/30
    def transform(p):
        x,y,z=p
        # The specimen lies on its back in the OBJ. This is a proper rotation,
        # keeping its winding intact and facing -Y in the Blender source.
        return Vector((-z,x,-y+4.8))*scale
    for v,p in zip(ob.data.vertices,raw):v.co=transform(p)
    # A physical proxy rig gives Blender real bone volumes for heat weighting.
    # Only the resulting vertex groups are retained: export uses the engine's
    # identity-axis canonical rig constructed by deliver().
    proxy_data=bpy.data.armatures.new('Skin weighting proxy')
    proxy=bpy.data.objects.new('Skin weighting proxy',proxy_data);art.SCENE.collection.objects.link(proxy)
    art.select(proxy);bpy.ops.object.mode_set(mode='EDIT')
    tails={0:1,1:2,2:3,3:4}
    ends={4:(-11.8,0,0),17:(-18,.1,4.7),18:(-18,.1,-4.7)}
    for offset,side in [(5,1),(11,-1)]:
        for row,toe in enumerate([(-14,1.7,7),(6,1.7,8),(18,1.7,7)]):
            tails[offset+row*2]=offset+row*2+1
            ends[offset+row*2+1]=(toe[0],toe[1],side*toe[2])
    for i,n in enumerate(names):
        b=proxy_data.edit_bones.new(n);b.head=transform(pivots[i]);b.tail=transform(pivots[tails[i]] if i in tails else ends[i])
        if parents[i]>=0:b.parent=proxy_data.edit_bones[names[parents[i]]]
    bpy.ops.object.mode_set(mode='OBJECT');art.select(ob);proxy.select_set(True);bpy.context.view_layer.objects.active=proxy
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    world=ob.matrix_world.copy();ob.parent=None;ob.matrix_world=world
    for mod in list(ob.modifiers):
        if mod.type=='ARMATURE':ob.modifiers.remove(mod)
    bpy.data.objects.remove(proxy,do_unlink=True)
    labels={n:0 for n in names}
    for v in ob.data.vertices:
        active=sorted([(g.group,g.weight) for g in v.groups if g.weight>1e-7],key=lambda p:p[1],reverse=True)[:4]
        assert active,'Automatic skin weighting left an unweighted vertex'
        total=sum(w for j,w in active)
        for group_index in [g.group for g in v.groups]:ob.vertex_groups[group_index].remove([v.index])
        for j,w in active:ob.vertex_groups[j].add([v.index],w/total,'REPLACE')
        labels[ob.vertex_groups[active[0][0]].name]+=1
    ob.data.materials.clear();ob.data.materials.append(scanned_material(name+' scanned cuticle'))
    for p in ob.data.polygons:p.material_index=0;p.use_smooth=True
    ob.data.uv_layers.active.name='UVMap';art.objects.append(ob)
    # Grow sparse irregular mineral clusters along each wing case, using
    # actual ray intersections rather than floating a crystal on an ellipsoid.
    tree=BVHTree.FromPolygons([v.co for v in ob.data.vertices],[p.vertices[:] for p in ob.data.polygons])
    mineral=art.mineral(name+' weathered mineral');rng=np.random.default_rng(4371+stage)
    for side in [-1,1]:
        for j in range([3,6,10][stage-1]):
            old_x=rng.uniform(1.5,11.5);old_z=side*rng.uniform(1.2,3.4)
            q=transform((old_x,-15,old_z));hit,normal,idx,distance=tree.ray_cast(q,Vector((0,0,-1)))
            assert hit is not None
            height=scale*rng.uniform(.7,1.5)*stage;radius=height*rng.uniform(.28,.5)
            art.crystal(name+' rooted mineral',hit-normal*radius*.22,radius,height,normal,'Spine01',mineral,rng)
            for k in range(2):
                offset=Vector((rng.uniform(-.5,.5),rng.uniform(-.5,.5),-.12))*radius
                art.crystal(name+' mineral matrix',hit+offset-normal*radius*.18,radius*.7,height*rng.uniform(.18,.38),normal,'Spine01',mineral,rng)
    folder=art.OUT/name;folder.mkdir(exist_ok=True)
    (folder/'weight_regions.json').write_text(json.dumps(labels,indent=2))
    print('SCAN_WEIGHT_REGIONS',labels,flush=True)
    return names,parents,[transform(p) for p in pivots]

art.compose=compose
requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['Prismite']
for stage,name in enumerate(['Prismite','Geoborer','Crystallisk'],1):
    if name not in requested:continue
    provenance={'source':'adapted-artec-beetle-scan','recipe':'Tools/Art/create_scanned_mineral_beetles.py',
        'copyright':'Beetle by Artec 3D / Artec Group inc. https://www.artec3d.com/3d-models/beetle ; archive CC BY 3.0 https://creativecommons.org/licenses/by/3.0/ ; adapted geometry, materials and skin; no endorsement.',
        'externalAssets':[{'title':'Beetle','author':'Artec Group inc. / Artec 3D','url':'https://www.artec3d.com/3d-models/beetle',
        'download':'https://cdn.artec3d.com/content-hub-3dmodels/beetle_obj.zip?VersionId=xdGQbLE2fPh4NzyM4zv8ocFpD9l5oKzZ',
        'sha256':'ef04e47bbb0712072dd8877b0a9f1d5a6738a0b457f22f78722103f579441140',
        'archiveLicense':'CC BY 3.0','archiveLicenseUrl':'https://creativecommons.org/licenses/by/3.0/',
        'pageLicense':'CC BY 4.0','pageLicenseUrl':'https://creativecommons.org/licenses/by/4.0/'}],
        'changes':'Reoriented and scaled, photograph exposure compressed, authored roughness, canonical 19-joint skin, original mineral growth. Shiny is hue-only.',
        'limitations':'Captured photographic illumination is not fully removed. Roughness is authored, not measured. Animation and mineral response require visual review.',
        'visualAcceptance':False}
    art.deliver(name,stage,provenance)
    folder=art.OUT/name
    shutil.copy2(SOURCE/'extracted/license.txt',folder/'Artec_Beetle_LICENSE.txt')
    (folder/'ATTRIBUTION.md').write_text('Beetle by Artec 3D (Artec Group inc.)\nhttps://www.artec3d.com/3d-models/beetle\n\nArchive: CC BY 3.0, https://creativecommons.org/licenses/by/3.0/\nPublisher page: CC BY 4.0, https://creativecommons.org/licenses/by/4.0/\n\n'+provenance['changes']+'\nNo endorsement is implied. Original license notice retained.\n')
