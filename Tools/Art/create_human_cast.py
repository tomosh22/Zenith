"""Editable costume/sculpt variants of the supplied anatomical T-pose source.

The original engine Male is preserved. Each missing cast member gets a separate
source under the existing shared-rig import directory. No skeleton is replaced.
Run Blender --background --python Tools/Art/create_human_cast.py.
"""
import sys,math,re,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_missing_props as a
import bpy
import numpy as np
from mathutils import Vector

ROOT=a.ROOT
OUT=ROOT/'Games/Zenithmon/Assets/Meshes/Humans'
a.TEXTURES=OUT/'_AuthoredMaterials';a.TEXTURES.mkdir(parents=True,exist_ok=True)
cloth={n:a.material('Cast '+n,c,'cloth',.83) for n,c in {
 'ivory':(.72,.70,.62),'navy':(.033,.060,.085),'moss':(.10,.16,.075),
 'russet':(.29,.095,.045),'plum':(.14,.047,.10),'ochre':(.40,.26,.066),
 'teal':(.037,.17,.18),'grey':(.19,.21,.22),'black':(.023,.028,.032)}.items()}
leather=a.material('Cast worn leather',(.12,.075,.038),'wood',.56)
hair={n:a.material('Cast hair '+n,c,'wood',.64) for n,c in {
 'BROWN':(.085,.039,.014),'BLACK':(.017,.014,.012),'AUBURN':(.19,.055,.02),
 'WHITE':(.69,.68,.61),'GREY':(.23,.22,.19),'BLONDE':(.38,.27,.10),'DYED':(.046,.090,.085)}.items()}
roles={
 'PlayerF':('navy','ivory','short_jacket','ponytail'),
 'Aster':('ivory','teal','labcoat','short'),
 'Maren':('moss','ivory','cardigan','bun'),
 'Vesper':('plum','black','short_jacket','swept'),
 'Fenna':('moss','ochre','fieldcoat','braid'),
 'Bram':('russet','ivory','apron','short'),
 'Maris':('navy','ivory','pea_coat','ponytail'),
 'Tessa':('ochre','grey','utility','short'),
 'Aquilo':('ivory','navy','short_jacket','swept'),
 'Morwenna':('plum','black','longcoat','long'),
 'Halvard':('navy','ivory','parka','short'),
 'Vardis':('grey','russet','longcoat','short'),
 'Cassia':('black','ivory','blazer','bun'),
 'Torben':('navy','grey','blazer','short'),
 'Lumen':('teal','ivory','blazer','swept'),
 'Sable':('plum','grey','longcoat','long'),
 'Elara':('ivory','navy','blazer','braid'),
 'Rambler':('moss','ochre','fieldcoat','short'),
 'Angler':('ochre','navy','utility','short'),
 'Netter':('teal','ivory','short_jacket','ponytail'),
 'Ridgewalker':('russet','grey','parka','short'),
 'Ace':('navy','ivory','blazer','swept'),
 'Scout':('moss','ivory','short_jacket','short'),
 'Ranger':('moss','ochre','fieldcoat','short'),
 'Duelist':('black','plum','blazer','swept'),
 'Wayfarer':('grey','ochre','longcoat','short'),
 'Camper':('russet','ivory','short_jacket','ponytail'),
 'Villager':('teal','grey','cardigan','short'),
 'Caretaker':('ivory','teal','tunic','bun'),
 'Shopkeep':('navy','ochre','apron','short'),
 'Elder':('grey','ivory','cardigan','short'),
 'Fieldhand':('ochre','moss','utility','short'),
 'Dockworker':('navy','grey','pea_coat','short'),
 'Warden':('moss','ochre','fieldcoat','short')}

def cloth_shell(label,rings,mat):
    # Open fitted tailoring loft with tapered hem and small vertical fabric folds.
    vertices=[];faces=[];n=48
    for z,rx,ry in rings:
        for i in range(n):
            t=math.tau*i/n
            wave=1+.014*math.cos(9*t)+.008*math.sin(z*37+13*t)
            vertices.append((math.cos(t)*rx*wave,math.sin(t)*ry*wave,z))
    for j in range(len(rings)-1):
        for i in range(n):faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
    ob=a.mesh(label,vertices,faces,mat)
    for p in ob.data.polygons:p.use_smooth=True
    mod=ob.modifiers.new('Tailored cloth thickness','SOLIDIFY');mod.thickness=.006
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob

def character(name,build,hair_key,attachment):
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'Zenith/Assets/Meshes/Humans/Male/Male.glb'))
    parts=[o for o in a.scene.objects if o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for ob in parts:ob.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();body=bpy.context.object
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    # The supplied source faces +X. Normalise to Blender -Y, feet at zero.
    for v in body.data.vertices:
        x,y,z=v.co;v.co=(y,-x,z)
    low=min(v.co.z for v in body.data.vertices);high=max(v.co.z for v in body.data.vertices)
    factor=1.8/(high-low)
    for v in body.data.vertices:v.co=(v.co.x*factor,v.co.y*factor,(v.co.z-low)*factor)
    primary,secondary,coat,style=roles[name];pm=cloth[primary];sm=cloth[secondary]
    female=name in ['PlayerF','Maren','Fenna','Maris','Tessa','Morwenna','Cassia','Elara','Netter','Caretaker']
    width={'AVERAGE':1.,'SLIGHT':.91,'STOCKY':1.13,'TALL':1.04}[build]
    for vert in body.data.vertices:
        x,y,z=vert.co
        # Keep shoulder, elbow, wrist and knee planes within the shared rig's
        # tolerance. Build variation changes girth; it does not stretch limbs.
        if abs(x)<.32 and .82<z<1.43:vert.co.x*=width
        if .2<z<1.0:vert.co.y*=1.12 if build=='STOCKY' else .96 if build=='SLIGHT' else 1
        if female and z>1.52:
            vert.co.x*=.93
            if z<1.62:vert.co.y*=.96
        if name in ['Aster','Elder','Halvard','Torben'] and z>1.54:vert.co.x*=1.04
    original=len(body.data.materials)
    for mat in [pm,sm,leather,hair[hair_key]]:body.data.materials.append(mat)
    for poly in body.data.polygons:
        centre=sum((body.data.vertices[i].co for i in poly.vertices),Vector())/len(poly.vertices)
        x,y,z=centre
        if z<.13:poly.material_index=original+2
        elif z<.93:poly.material_index=original+1
        elif z<1.50 and abs(x)<.67:poly.material_index=original
        elif z>1.72 or (z>1.61 and y>.025):poly.material_index=original+3
        poly.use_smooth=True
    body.name=name+' anatomical base and fitted clothing'
    # Garment hems, closures, collars and pockets are actual geometry, with
    # explicit gaps around the hands and hips for the binder's T-pose solve.
    hip=.205*width
    if coat in ['labcoat','longcoat','fieldcoat','pea_coat','parka','tunic']:
        bottom=.72 if coat in ['longcoat','labcoat'] else .89
        cloth_shell('Tailored coat skirt',[(bottom,hip*1.13,.15),(.90,hip*1.02,.13),(1.05,hip*.91,.125),(1.19,hip,.14)],pm)
    if coat=='apron':
        a.mesh('Heavy work apron',[(-hip,-.155,.70),(hip,-.155,.70),(hip*.91,-.15,1.14),(.14,-.15,1.37),(-.14,-.15,1.37),(-hip*.91,-.15,1.14)],[(0,1,2,3,4,5)],sm,.006)
        for side in [-1,1]:a.beam('Apron strap',(side*.12,-.155,1.36),(side*.16,.02,1.47),.019,leather)
    if coat in ['labcoat','blazer','longcoat','pea_coat']:
        for side in [-1,1]:
            a.mesh('Folded lapel',[(side*.045,-.145,1.16),(side*.145,-.145,1.39),(side*.06,-.135,1.47),(side*.018,-.145,1.27)],[(0,1,2,3)],sm,.006)
    if coat=='parka':
        for i in range(17):
            theta=i*math.pi/16
            a.sphere('Shearling collar',(.15*math.cos(theta),.085*math.sin(theta),1.47),(.055,.055,.066),cloth['ivory'],2,.1)
    for z in np.arange(.99,1.38,.09):
        button=a.cyl('Coat fastening',(.012,-.15,float(z)),.010,.006,a.brass,12);button.rotation_euler.x=math.pi/2
    for side in [-1,1]:
        a.box('Pocket flap',(side*hip*.62,-.151,1.06),(.092,.014,.055),sm,.008)
        a.box('Pocket welt',(side*hip*.62,-.155,1.095),(.097,.009,.008),leather,.002)
    # Hair silhouettes remain within ~2% of the anatomical head height.
    hm=hair[hair_key]
    if style in ['ponytail','braid','bun','long']:
        if style=='bun':a.sphere('Pinned hair bun',(0,.13,1.69),(.14,.13,.15),hm,4,.035)
        else:
            count=8 if style=='braid' else 5
            for i in range(count):
                z=1.69-i*(.035 if style=='braid' else .048)
                xx=(.012 if i%2 else -.012) if style=='braid' else 0
                a.sphere('Braided lock' if style=='braid' else 'Falling hair lock',(xx,.145,z),(.10 if style!='long' else .23,.07,.105),hm,3,.04)
    if attachment in ['HAT','CAP']:
        a.sphere('Fitted cap crown',(0,0,1.744),(.285,.25,.13),pm,4)
        a.sphere('Hat brim',(0,-.085,1.71),(.34 if attachment=='HAT' else .24,.34 if attachment=='HAT' else .20,.019),leather,3)
    if attachment=='GLASSES':
        for side in [-1,1]:
            a.torus('Spectacle rim',(side*.045,-.092,1.65),.032,.003,a.iron,(math.pi/2,0,0))
            a.beam('Spectacle temple',(side*.078,-.085,1.65),(side*.085,.065,1.65),.003,a.iron)
        a.beam('Spectacle bridge',(-.012,-.092,1.65),(.012,-.092,1.65),.003,a.iron)
    if attachment=='BACKPACK':
        a.box('Canvas rucksack',(0,.20,1.22),(.31,.18,.40),sm,.06)
        a.box('Rucksack top flap',(0,.27,1.38),(.30,.05,.11),leather,.022)
        for side in [-1,1]:
            a.beam('Shoulder webbing',(side*.13,-.14,1.18),(side*.14,.05,1.47),.014,leather)
            a.box('Strap buckle',(side*.10,.306,1.26),(.025,.012,.04),a.brass,.004)
    if attachment=='SATCHEL':
        a.box('Leather satchel',(.19,-.06,.93),(.12,.16,.23),leather,.024)
        a.beam('Cross body strap',(-.16,-.16,1.43),(.19,-.15,.97),.012,leather)
    # The source remains an unrigged T-pose, exactly what HumanModelExport owns.

def main():
    only=next((s.split('=',1)[1].split(',') for s in sys.argv if s.startswith('--only=')),None)
    text=(ROOT/'Games/Zenithmon/Source/Data/ZM_HumanData.cpp').read_text()
    for name,build,hair_key,attachment in re.findall(r'"(\w+)"\s*,\s*ZM_HUMAN_BUILD_(\w+).*?ZM_HUMAN_HAIR_(\w+).*?ZM_HUMAN_ATTACHMENT_(\w+)',text):
        if name=='PlayerM' or (only and name not in only):continue
        target=OUT/name
        if (target/(name+'.glb')).exists() and '--replace-authored' not in sys.argv:continue
        target.mkdir(parents=True,exist_ok=True)
        a.scene=bpy.data.scenes.new('Cast '+name);bpy.context.window.scene=a.scene
        a.rng=np.random.default_rng(int.from_bytes(hashlib.sha256(name.encode()).digest()[:8],'little'))
        character(name,build,hair_key,attachment)
        bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=next(o for o in a.scene.objects if o.type=='MESH')
        bpy.ops.object.join();ob=bpy.context.object;ob.name=name
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        mod=ob.modifiers.new('Export triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.export_scene.gltf(filepath=str(target/(name+'.glb')),export_format='GLB',use_selection=True,
            use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True)
        bpy.data.libraries.write(str(target/(name+'.blend')),{a.scene},fake_user=True)
        print('AUTHORED HUMAN',name,len(ob.data.polygons),'triangles',flush=True)

if __name__=='__main__':main()
