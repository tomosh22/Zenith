"""Unapproved Fernfawn sculpt study fitted to the existing creature rig.

The generated mesh is retained as a weight/proportion reference. This is a
study outside the runtime Assets tree, not a delivery: silhouette and animation
review are mandatory before any ArtBrief box may be checked.
"""
import sys,math,re,json,struct,shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_missing_props as a
import read_zenith_rig as wire
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT=a.ROOT;OUT=ROOT/'Games/Zenithmon/Assets/Creatures'
STUDIES=ROOT/'Build/artifacts/authored_assets/studies'
a.TEXTURES=STUDIES/'_AuthoredMaterials';a.TEXTURES.mkdir(parents=True,exist_ok=True)
a.N=512;a.v,a.u=np.mgrid[0:512,0:512].astype(np.float32)/512;a.noise=a.rng.random((512,512)).astype(np.float32)
horn=a.material('Creature keratin',(.40,.33,.21),'wood',.57)
eye=a.material('Creature wet cornea',(.008,.006,.004),roughness=.07)
fawnfur=a.material('Fawn chestnut fur',(.29,.15,.066),'fur',.79)
fawnlight=a.material('Fawn cream undercoat',(.63,.51,.33),'fur',.82)

def deer_body(bones,stage):
    """A separately sculpted fawn, using the shipped joints only as a rig fit."""
    def flesh(label,pos,scale,bone,mat=fawnfur):
        return pin(a.sphere(label,to_b(pos),(scale[0],scale[2],scale[1]),mat,4),bone)
    # Chest, rib cage and haunches make a horizontal ungulate torso. The long
    # neck, tapered muzzle, hocks and split hooves carry the deer silhouette.
    body=flesh('Fawn rib cage',(0,.445,-.015),(.265,.275,.52),'Spine02')
    flesh('Shoulder girdle',(0,.44,.145),(.245,.29,.23),'Spine03')
    flesh('Fawn rump',(0,.425,-.20),(.27,.25,.24),'Spine00')
    neck=flesh('Rising neck',(0,.575,.23),(.12,.32,.145),'Head');neck.rotation_euler.x=-.34
    flesh('Sculpted cranium',(0,.706,.335),(.145,.16,.22),'Head')
    flesh('Tapered deer muzzle',(0,.668,.44),(.094,.076,.17),'Head')
    flesh('Soft jaw',(0,.648,.39),(.091,.046,.16),'Head',fawnlight)
    for side,letter in [(-1,'L'),(1,'R')]:
        for front,letter2 in [(True,'F'),(False,'H')]:
            x=side*.113;z=.16 if front else -.19
            up='Leg'+letter2+letter+'Up';low='Leg'+letter2+letter+'Lo'
            flesh('Upper leg muscle',(x,.315,z),(.064,.25,.082),up)
            flesh('Knee joint',(x,.185,z+.008 if front else z-.018),(.047,.064,.049),low)
            flesh('Slender cannon bone',(x,.102,z),(.028,.16,.037),low)
            flesh('Pastern',(x,.037,z+.014),(.033,.056,.043),low)
    flesh('Raised tail',(0,.43,-.327),(.058,.10,.14),'Tail00',fawnlight)
    for ob in a.scene.objects:ob.select_set(ob.type=='MESH')
    bpy.context.view_layer.objects.active=body;bpy.ops.object.join();body=bpy.context.object
    # Capture the fitting weights before joining the overlapping sculpt volumes.
    donor=[(v.co.copy(),[(body.vertex_groups[g.group].name,g.weight) for g in v.groups]) for v in body.data.vertices]
    tree=KDTree(len(donor))
    for i,(co,weights) in enumerate(donor):tree.insert(co,i)
    tree.balance()
    body.data.remesh_voxel_size=.004
    bpy.ops.object.voxel_remesh()
    smooth=body.modifiers.new('Sculpt surface relaxation','SMOOTH');smooth.factor=.6;smooth.iterations=4
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    dec=body.modifiers.new('Sculpt export density','DECIMATE');dec.ratio=.45;bpy.ops.object.modifier_apply(modifier=dec.name)
    body.vertex_groups.clear()
    for b in bones:body.vertex_groups.new(name=b['name'])
    for v in body.data.vertices:
        blend={}
        for co,idx,d in tree.find_n(v.co,4):
            factor=1/max(d,.0001)**2
            for bone,weight in donor[idx][1]:blend[bone]=blend.get(bone,0)+weight*factor
        selected=sorted(blend.items(),key=lambda p:-p[1])[:4];total=sum(w for _,w in selected)
        for bone,weight in selected:body.vertex_groups[bone].add([v.index],weight/total,'REPLACE')
    body.data.materials.clear();a.finish(body,'Sculpted fawn anatomy',fawnfur,0,True)
    # Small lateral eyes sit in the skull; no white cartoon eyeballs.
    for side in [-1,1]:
        flesh('Fawn eyelid',(side*.063,.737,.375),(.030,.029,.035),'Head',fawnfur)
        flesh('Fawn dark eye',(side*.076,.737,.381),(.012,.021,.022),'Head',eye)
        ear=flesh('Large fawn ear',(side*.100,.812,.287),(.072,.154,.041),'Head');ear.rotation_euler.y=side*.42
        inner=flesh('Fine inner ear',(side*.104,.819,.308),(.047,.109,.012),'Head',fawnlight);inner.rotation_euler.y=side*.42
        for front,letter2 in [(True,'F'),(False,'H')]:
            for toe in [-1,1]:flesh('Cloven hoof',(side*.113+toe*.009,.018,.181 if front else -.169),(.016,.034,.047),'Leg'+letter2+('L' if side<0 else 'R')+'Lo',horn)
        start=to_b((side*.045,.777,.32))
        end=start+Vector((side*.027,.005,.074))
        pin(a.beam('Living antler bud',start,end,.008,a.oak),'Head')
        for j in range(4):
            pos=start+(end-start)*(j+.4)/4
            for leafside in [-1,1]:
                ob=a.sphere('Fern antler leaflet',pos+Vector((leafside*.020,0,.008)),(.045,.012,.006),a.green,3)
                ob.rotation_euler.y=-leafside*.27;pin(ob,'Head')
    flesh('Moist deer nose',(0,.675,.518),(.058,.039,.022),'Head',eye)
    # Fawn spots conform to the rib cage, with alternating spacing.
    for side in [-1,1]:
        for row in range(3):
            for j in range(6):
                z=-.20+j*.062+(row%2)*.012
                y=.49+row*.031
                radial=max(.04,1-((y-.445)/.1375)**2-((z+.015)/.26)**2)
                x=side*.1325*math.sqrt(radial)
                flesh('Natural fawn spot',(x,y,z),(.0035,.014,.022),'Spine02',fawnlight)
    return body

def to_b(p):return Vector((p[0],-p[2],p[1]))

def pin(ob,bone):
    group=ob.vertex_groups.new(name=bone)
    group.add(list(range(len(ob.data.vertices))),1.,'REPLACE')
    return ob

def patch_extras(path,skeleton,name):
    raw=path.read_bytes();n,kind=struct.unpack_from('<II',raw,12);doc=json.loads(raw[20:20+n])
    doc['asset']['extras']={'zenithSharedSkeleton':skeleton,'zenithAnimations':[f'game:Creatures/{name}/{name}_{c}.zanim' for c in ['Idle','Walk','Attack','Special','Hit','Faint']]}
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4)
    rest=raw[20+n:];path.write_bytes(struct.pack('<III',0x46546c67,2,20+len(js)+len(rest))+struct.pack('<II',len(js),0x4e4f534a)+js+rest)

def author(name,element,archetype,stage,family):
    target=STUDIES/name
    target.mkdir(parents=True,exist_ok=True)
    source=target/(name+'.glb')
    if source.exists() and '--replace-authored' not in sys.argv:return
    reference=target/'rig_reference';reference.mkdir(parents=True,exist_ok=True)
    for ext in ['zmesh','zskel']:
        if not (reference/(name+'.'+ext)).exists():shutil.copy2(OUT/name/(name+'.'+ext),reference/(name+'.'+ext))
    data=wire.mesh(reference/(name+'.zmesh'));bones=wire.skeleton(reference/(name+'.zskel'))
    a.scene=bpy.data.scenes.new('Creature study '+name);bpy.context.window.scene=a.scene
    body=deer_body(bones,stage)
    bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=body;bpy.ops.object.join()
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    body=bpy.context.object;body.name=name
    rigdata=bpy.data.armatures.new(name+' shared rig');rig=bpy.data.objects.new(name+' rig',rigdata);a.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active=rig;body.select_set(False);rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for i,b in enumerate(bones):
        if not np.allclose(b['world'][:3,:3],np.eye(3),atol=1e-5):raise ValueError('Non-translation rig needs an explicit basis conversion')
        bone=rigdata.edit_bones.new(b['name']);bone.head=to_b(b['world'][:3,3]);bone.tail=bone.head+Vector((0,0,.05));bone.roll=0
        if b['parent']>=0:bone.parent=rigdata.edit_bones[bones[b['parent']]['name']]
    bpy.ops.object.mode_set(mode='OBJECT')
    body.parent=rig;mod=body.modifiers.new('Shared creature rig','ARMATURE');mod.object=rig
    body.select_set(True);bpy.context.view_layer.objects.active=body
    tri=body.modifiers.new('Export triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    bpy.ops.export_scene.gltf(filepath=str(source),export_format='GLB',use_selection=True,use_active_scene=True,
        export_animations=False,export_tangents=True,export_yup=True,export_skins=True)
    patch_extras(source,data['skeleton'],name)
    bpy.data.libraries.write(str(target/(name+'.blend')),{a.scene},fake_user=True)
    print('CREATURE STUDY',name,'family',family,'triangles',len(body.data.polygons),flush=True)

def main():
    only=next((s.split('=',1)[1].split(',') for s in sys.argv if s.startswith('--only=')),['Fernfawn'])
    text=(ROOT/'Games/Zenithmon/Source/Data/ZM_SpeciesData.cpp').read_text()
    for name,element,archetype,stage,family in re.findall(r'"(\w+)"\s*,\s*\{\s*ZM_TYPE_(\w+).*?ZM_ARCHETYPE_(\w+)\s*,\s*(\d+),\s*ZM_SPECIES_\w+\s*,\s*(\d+)',text):
        if name!='Fernfawn' or name not in only:continue
        author(name,element,archetype,int(stage),int(family))

if __name__=='__main__':main()
