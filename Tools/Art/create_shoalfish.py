"""Author Minnet, Shoalfin and Torrentfin from a pinned fish scan and new fins.

Run in Blender with --background --factory-startup --disable-autoexec
--python-exit-code 1 --python Tools/Art/create_shoalfish.py -- [species names].
Sources remain working candidates until pose and runtime review.
"""
from pathlib import Path
import bpy,numpy as np,math,sys,json,hashlib,shutil
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_lancefish as art
ROOT=art.ROOT
SOURCE=ROOT/'Build/artifacts/authored_assets/reference_sources/Artec_Fish'
FILES=SOURCE/'extracted/Fish obj'
assert hashlib.sha256((SOURCE/'fish_obj.zip').read_bytes()).hexdigest()=='66fe006cc74962321418b73cb40a271a4d3129d2b6c35119a77b9b1886bea015'
for filename,digest in {
    'Fish.obj':'bbc974d47b94f4bd56782c3b7f04e21a511fd724c99db9c56254ff4c96f84623',
    'Fish_0.jpg':'e55b6d97aa8bee4043980c5bfe2203cdd7d697191fbbad24a0bf1e43be86a0db',
    'license.txt':'4211d679dceb5d5e7d11541db839c2cbafcd7a5fa746c903da36b30c369178cc',
}.items():assert hashlib.sha256((FILES/filename).read_bytes()).hexdigest()==digest,filename
art.OUT=ROOT/'Build/artifacts/authored_assets/creature_work/Shoalfish';art.OUT.mkdir(parents=True,exist_ok=True)
NAMES=['Spine00','Spine01','Spine02','Head','FinDorsal','FinPecL','FinPecR','FinCaudal']
PARENTS=[-1,0,1,2,2,1,1,0]

def save_bake(im):
    im.filepath_raw=str(art.OUT/(im.name+'.png'));im.file_format='PNG';im.save();im.pack();return im

def weight(p,L):
    t=p[1]/L+.5
    knots=[.08,.29,.57,.84];bones=['Head','Spine02','Spine01','Spine00']
    if t<=knots[0]:return {bones[0]:1.}
    if t>=knots[-1]:return {bones[-1]:1.}
    for i in range(3):
        if knots[i]<=t<=knots[i+1]:
            f=(t-knots[i])/(knots[i+1]-knots[i]);f=f*f*(3-2*f)
            return {bones[i]:1-f,bones[i+1]:f}

def compose(name,stage):
    L=[.38,.78,1.28][stage-1];center=.22*L
    bpy.ops.wm.obj_import(filepath=str(FILES/'Fish.obj'));high=bpy.context.object;high.name=name+' source surface'
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    raw=np.array([v.co[:] for v in high.data.vertices]);lo=raw[:,0].min();hi=raw[:,0].max();span=hi-lo
    # Straighten the scanned specimen's lateral bow by local cross sections.
    cuts=np.linspace(lo,hi,121);cy=[];cz=[]
    for x in cuts:
        band=raw[abs(raw[:,0]-x)<span*.018]
        cy.append((np.quantile(band[:,1],.03)+np.quantile(band[:,1],.97))*.5)
        cz.append((np.quantile(band[:,2],.03)+np.quantile(band[:,2],.97))*.5)
    colours=[]
    for v,p in zip(high.data.vertices,raw):
        t=(p[0]-lo)/span;front=np.clip((t-.63)/.25,0,1)
        width=[.80,1.05,1.30][stage-1]*(1+(.16 if stage==1 else 0)*front)
        depth=[.82,1.12,1.37][stage-1]
        x=(p[1]-np.interp(p[0],cuts,cy))/span*L*width
        z=(p[2]-np.interp(p[0],cuts,cz))/span*L*depth
        v.co=(x,(.5-t)*L,center+z)
        dorsal=np.clip(z/(L*.11)+.45,0,1)
        top=np.array([[.53,.77,.94],[.36,.64,.85],[.22,.48,.67]][stage-1])
        tint=np.array([.96,.98,1.])*(1-dorsal)+top*dorsal
        colours.append([*tint,1])
    high.data.uv_layers.active.name='PhotoUV'
    attr=high.data.color_attributes.new(name='StagePigment',type='FLOAT_COLOR',domain='POINT');attr.data.foreach_set('color',np.asarray(colours,np.float32).ravel())
    src=bpy.data.materials.new(name+' source photo projection');src.use_nodes=True;nodes=src.node_tree.nodes;links=src.node_tree.links;nodes.clear()
    out=nodes.new('ShaderNodeOutputMaterial');em=nodes.new('ShaderNodeEmission');tex=nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(FILES/'Fish_0.jpg'),check_existing=False)
    uv=nodes.new('ShaderNodeUVMap');uv.uv_map='PhotoUV';links.new(uv.outputs[0],tex.inputs['Vector'])
    tint=nodes.new('ShaderNodeVertexColor');tint.layer_name='StagePigment';mul=nodes.new('ShaderNodeMixRGB');mul.blend_type='MULTIPLY';mul.inputs[0].default_value=1
    links.new(tex.outputs['Color'],mul.inputs[1]);links.new(tint.outputs['Color'],mul.inputs[2]);links.new(mul.outputs[0],em.inputs['Color']);links.new(em.outputs[0],out.inputs['Surface'])
    high.data.materials.clear();high.data.materials.append(src)
    for p in high.data.polygons:p.material_index=0;p.use_smooth=True
    low=high.copy();low.data=high.data.copy();art.SCENE.collection.objects.link(low);low.name=name+' retessellated body';art.select(low)
    dec=low.modifiers.new('Preserve scanned silhouette with fewer triangles','DECIMATE');dec.ratio=.20;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
    # Continuous cylindrical body UVs, with the seam on the ventral midline.
    for layer in list(low.data.uv_layers):low.data.uv_layers.remove(layer)
    target_uv=low.data.uv_layers.new(name='UVMap')
    coords=[]
    for v in low.data.vertices:
        q=v.co;coords.append((.5+math.atan2(q.x,q.z-center)/math.tau,.5-q.y/L))
    for poly in low.data.polygons:
        values=[coords[low.data.loops[i].vertex_index] for i in poly.loop_indices]
        wrap=max(u for u,v in values)-min(u for u,v in values)>.5
        for i,(u,v) in zip(poly.loop_indices,values):target_uv.data[i].uv=(u+1 if wrap and u<.5 else u,v)
    target=bpy.data.materials.new(name+' bake target');target.use_nodes=True;low.data.materials.clear();low.data.materials.append(target)
    for p in low.data.polygons:p.material_index=0
    target_node=target.node_tree.nodes.new('ShaderNodeTexImage');target.node_tree.nodes.active=target_node
    art.SCENE.cycles.samples=1;art.SCENE.render.bake.use_selected_to_active=True;art.SCENE.render.bake.cage_extrusion=L*.004;art.SCENE.render.bake.max_ray_distance=L*.025;art.SCENE.render.bake.margin=12
    art.select(high);low.select_set(True);bpy.context.view_layer.objects.active=low
    base=bpy.data.images.new(name+' baked photo pigment',width=2048,height=2048);target_node.image=base;bpy.ops.object.bake(type='EMIT');save_bake(base)
    normal=bpy.data.images.new(name+' captured surface normal',width=2048,height=2048);normal.colorspace_settings.name='Non-Color';target_node.image=normal
    bpy.ops.object.bake(type='NORMAL');save_bake(normal)
    art.SCENE.render.bake.use_selected_to_active=False;art.SCENE.cycles.samples=48
    bpy.data.objects.remove(high,do_unlink=True)
    # Wet scales are dielectric. Roughness is authored, not measured from a
    # photograph; the neutral B channel avoids treating silver pigment as metal.
    n=2048;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n
    rough=.28+.055*np.cos(u*math.tau)**2+.025*np.sin(v*51+u*19)**2
    mat=art.pbr(name+' scaled skin',base,normal,art.image(name+' skin MR',np.stack([np.ones_like(v),rough,np.zeros_like(v)],axis=2),True))
    low.data.materials.clear();low.data.materials.append(mat)
    for n in NAMES:low.vertex_groups.new(name=n)
    for vertex in low.data.vertices:
        for b,w in weight(vertex.co,L).items():
            if w>0:low.vertex_groups[b].add([vertex.index],w,'REPLACE')
    art.objects.append(low)
    tree=BVHTree.FromPolygons([v.co for v in low.data.vertices],[p.vertices[:] for p in low.data.polygons])
    def dorsal_root(y,bottom=False):
        start=Vector((0,y,center+(-1 if bottom else 1)*L));direction=Vector((0,0,1 if bottom else -1))
        hit,normal,index,distance=tree.ray_cast(start,direction)
        assert hit is not None,(name,y,bottom)
        return hit+direction*(L*.001)
    n=1024;fv,fu=np.mgrid[0:n,0:n].astype(np.float32)/n
    veins=np.exp(-(np.sin((fu*19+.028*np.sin(fv*11))*math.pi)/.16)**2)
    grain=np.random.default_rng(835+stage).random((n,n)).astype(np.float32)-.5
    col=np.tile([.27,.31,.28],(n,n,1))*(1-veins[:,:,None]*.24+grain[:,:,None]*.035)
    col+=np.stack([fv*.055,fv*.035,fv*.025],axis=2)
    # Thin membranes transmit the background between denser collagen rays.
    # Alpha is coverage/opacity in core glTF metallic-roughness; this is not
    # a measured subsurface/transmission model.
    opacity=np.clip(.32+.24*(1-fv)**1.5+.30*veins+grain*.02,.25,.90)
    finbase=art.image(name+' FinPigment',np.concatenate([col,opacity[:,:,None]],axis=2))
    finmat=art.pbr(name+' fin membranes',finbase,
        art.normal_image(name+' FinRelief',veins*.004+grain*.0003,8),
        art.image(name+' FinMR',np.stack([np.ones_like(fv),.39+veins*.08,np.zeros_like(fv)],axis=2),True))
    finmat.surface_render_method='DITHERED';finmat.use_backface_culling=False
    pigment=next(n for n in finmat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image==finbase)
    finmat.node_tree.links.new(pigment.outputs['Alpha'],finmat.node_tree.nodes.get('Principled BSDF').inputs['Alpha'])
    raymat=art.plain(name+' fin ray collagen',[.31,.33,.26],.42)
    iris=art.plain(name+' mottled gold iris',[.25,.205,.09],.24)
    pupil=art.plain(name+' wet black pupil',[.003,.005,.004],.10)
    for side,letter in [(-1,'L'),(1,'R')]:
        # The scanned head is slightly rolled: identify each photographed eye
        # independently instead of overlaying a mirrored guess on its cheek.
        eye_x,eye_z=(174.7,37.0) if side<0 else (192.4,56.6)
        eye_y=(.5-(eye_x-lo)/span)*L
        eye_height=center+(eye_z-np.interp(eye_x,cuts,cz))/span*L*[.82,1.12,1.37][stage-1]
        start=Vector((side*L,eye_y,eye_height))
        hit,normal,index,distance=tree.ray_cast(start,Vector((-side,0,0)));assert hit is not None
        art.ellipsoid(name+' '+letter+' iris',hit+Vector((side*.0015*L,0,0)),(.005*L,.014*L,.013*L),'Head',iris)
        art.ellipsoid(name+' '+letter+' cornea',hit+Vector((side*.0055*L,-.001*L,0)),(.004*L,.0077*L,.0077*L),'Head',pupil)
    def fin(label,roots,tips,bone):
        roots=list(map(Vector,roots));tips=list(map(Vector,tips));nr=len(roots);steps=12;verts=[];weights=[];faces=[];uvs=[]
        for i,(root,tip) in enumerate(zip(roots,tips)):
            for j in range(steps+1):
                t=j/steps;p=root.lerp(tip,t);p.x+=L*.0008*math.sin(i*math.pi*.75)*math.sin(t*math.pi);verts.append(p)
                ws={b:w*(1-t*.9) for b,w in weight(root,L).items()};ws[bone]=ws.get(bone,0)+t*.9;weights.append(ws)
        for i in range(nr-1):
            for j in range(steps):
                faces.append((i*(steps+1)+j,(i+1)*(steps+1)+j,(i+1)*(steps+1)+j+1,i*(steps+1)+j+1))
                uvs.append([(i/(nr-1),j/steps),((i+1)/(nr-1),j/steps),((i+1)/(nr-1),(j+1)/steps),(i/(nr-1),(j+1)/steps)])
        ob=art.mesh(name+' '+label,verts,faces,finmat,uvs,weights);art.select(ob)
        # One two-sided surface avoids applying alpha twice to nearly coincident
        # front/back shells; the separately modelled rays retain physical volume.
        for i,(root,tip) in enumerate(zip(roots,tips)):
            if i%2:continue
            length=(tip-root).length
            if length<L*.005:continue
            points=[root.lerp(tip,float(t)) for t in np.linspace(.03,.98,12)]
            def ray_weights(p):
                t=min(.9,(p-root).length/length*.9);ws={b:w*(1-t) for b,w in weight(root,L).items()};ws[bone]=ws.get(bone,0)+t;return ws
            art.tube(name+' '+label+' ray',points,[float(L*(.0008*(1-j/11)+.00013)) for j in range(12)],bone,raymat,6,ray_weights)
    roots=[];tips=[]
    for t in np.linspace(0,1,39):
        y=(-.13+.39*t)*L;root=dorsal_root(y);rise=[.085,.145,.215][stage-1]*math.sin(math.pi*t)**.65*(1-.4*t)
        roots.append(root);tips.append(root+Vector((0,.055*L*math.sin(math.pi*t),rise*L)))
    fin('dorsal crest',roots,tips,'FinDorsal')
    roots=[];tips=[]
    for t in np.linspace(0,1,45):
        z=(1-2*t)*.042*L;roots.append(Vector((0,.465*L,center+z)))
        # Broad juvenile lobes become a deeply forked, swept adult tail.
        tips.append(Vector((0,(.57+[.10,.16,.23][stage-1]*abs(1-2*t)**.8)*L,center+(1-2*t)*[.13,.18,.24][stage-1]*L)))
    fin('forked caudal',roots,tips,'FinCaudal')
    pectoral_heads=[]
    for side,letter in [(-1,'L'),(1,'R')]:
        y=(-.14 if side<0 else -.30)*L
        origin=Vector((side*L,y,center+(-.01 if side<0 else .025)*L));hit,normal,index,distance=tree.ray_cast(origin,Vector((-side,0,0)));assert hit is not None
        pectoral_heads.append(tuple(hit))
        roots=[];tips=[]
        for t in np.linspace(0,1,29):
            root=hit+Vector((-side*.003*L,(t-.5)*.026*L,-.012*L));reach=math.sin(math.pi*t)**.65
            roots.append(root);tips.append(root+Vector((side*reach*[.085,.14,.18][stage-1]*L,(.025+.16*t)*reach*L,-.032*reach*L)))
        fin('pectoral '+letter,roots,tips,'FinPec'+letter)
    for label,begin,end,depth in [('anal stabiliser',.16,.33,.065),('pelvic stabiliser',-.04,.045,.045)]:
        roots=[];tips=[]
        for t in np.linspace(0,1,25):
            root=dorsal_root((begin+(end-begin)*t)*L,True);roots.append(root);tips.append(root+Vector((0,.035*L*math.sin(math.pi*t),-depth*L*math.sin(math.pi*t)**.7)))
        fin(label,roots,tips,'Spine00' if begin>0 else 'Spine01')
    heads=[(0,.36*L,center),(0,.11*L,center),(0,-.12*L,center),(0,-.35*L,center),
        tuple(dorsal_root(-.04*L)),*pectoral_heads,(0,.46*L,center)]
    for ob in art.objects:ob.data.validate(verbose=True,clean_customdata=True)
    return NAMES,PARENTS,[Vector(p) for p in heads]

art.compose=compose
requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
for stage,name in enumerate(['Minnet','Shoalfin','Torrentfin'],1):
    if requested and name not in requested:continue
    credit='Fish by Artec 3D / Artec Group inc. https://www.artec3d.com/3d-models/fish ; archive CC BY 3.0 https://creativecommons.org/licenses/by/3.0/ ; adapted body, UVs, materials, fins and skin; no endorsement.'
    provenance={'source':'adapted-artec-shoalfish','recipe':'Tools/Art/create_shoalfish.py','copyright':credit,
        'externalAssets':[{'title':'Fish','author':'Artec 3D / Artec Group inc.','url':'https://www.artec3d.com/3d-models/fish',
        'download':'https://cdn.artec3d.com/content-hub-3dmodels/fish_obj.zip?VersionId=uTKVUs.zbTAsN.U6m10QOiLnBnR2Aq63',
        'sha256':'66fe006cc74962321418b73cb40a271a4d3129d2b6c35119a77b9b1886bea015','archiveLicense':'CC BY 3.0','pageLicense':'CC BY 4.0'}],
        'changes':'Straightened and individually proportioned scanned body; decimation, new UVs, baked surface normals and tinted photo pigment; original modelled fin membranes/rays and eight-joint skin.',
        'finMaterial':'Two-sided single-surface glTF BLEND with textured opacity; solid collagen rays. Alpha is authored, not measured transmission.',
        'limitations':'Photographic lighting is not fully removed; roughness is authored rather than measured; visual and motion acceptance pending.','visualAcceptance':False}
    art.deliver(name,stage,provenance)
    folder=art.OUT/name;shutil.copy2(FILES/'license.txt',folder/'Artec_Fish_LICENSE.txt')
    (folder/'ATTRIBUTION.md').write_text(credit+'\n\nPublisher page also offers CC BY 4.0: https://creativecommons.org/licenses/by/4.0/\n\n'+provenance['changes']+'\n')
