"""Original mineral-beetle authoring with Blender's Python API.

Prismite, Geoborer and Crystallisk retain the nineteen insectoid joints,
but use horizontal anatomical rest positions. All metallic/roughness PBR maps
are embedded. Outputs remain review candidates until explicitly installed.
"""
from pathlib import Path
import bpy, numpy as np, math, json, sys
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Build/artifacts/authored_assets/creature_work/MineralBeetles'
OUT.mkdir(parents=True,exist_ok=True)
SCENE=bpy.data.scenes.new('Mineral beetle anatomy');bpy.context.window.scene=SCENE
SCENE.unit_settings.system='METRIC';SCENE.render.engine='CYCLES'
SCENE.cycles.samples=48;SCENE.cycles.use_denoising=True;SCENE.view_settings.view_transform='AgX'
objects=[]

def select(ob):
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob

def image(name,rgb,data=False):
    rgb=np.asarray(rgb,np.float32)
    if rgb.ndim==1:rgb=np.tile(rgb,(8,8,1))
    h,w=rgb.shape[:2]
    if rgb.shape[2]==3:rgb=np.concatenate([rgb,np.ones((h,w,1),np.float32)],axis=2)
    im=bpy.data.images.new(name,width=w,height=h,alpha=True);im.colorspace_settings.name='Non-Color' if data else 'sRGB'
    im.pixels.foreach_set(rgb.ravel());im.update();im.filepath_raw=str(OUT/(name+'.png'));im.file_format='PNG';im.save();im.pack();return im

def pbr(name,base,normal,rm):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;bs=n.get('Principled BSDF')
    def tex(im):
        t=n.new('ShaderNodeTexImage');t.image=im;return t
    l.new(tex(base).outputs['Color'],bs.inputs['Base Color'])
    nm=n.new('ShaderNodeNormalMap');l.new(tex(normal).outputs['Color'],nm.inputs['Color']);l.new(nm.outputs[0],bs.inputs['Normal'])
    sep=n.new('ShaderNodeSeparateColor');l.new(tex(rm).outputs['Color'],sep.inputs[0]);l.new(sep.outputs['Green'],bs.inputs['Roughness']);l.new(sep.outputs['Blue'],bs.inputs['Metallic'])
    return mat

flat=image('Mineral beetle flat normal',[.5,.5,1],True)
def plain(name,colour,rough,metal=0):
    return pbr(name,image(name+' base',colour),flat,image(name+' MR',[1,rough,metal],True))

def normal_image(name,height,strength):
    dy,dx=np.gradient(height);n=np.stack([-dx*strength,-dy*strength,np.ones_like(height)],axis=2)
    n/=np.linalg.norm(n,axis=2)[:,:,None];return image(name,n*.5+.5,True)

def mesh(name,verts,faces,mat,uvs,weights):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update();ob=bpy.data.objects.new(name,data);SCENE.collection.objects.link(ob)
    data.materials.append(mat);uv=data.uv_layers.new(name='UVMap')
    for poly,coords in zip(data.polygons,uvs):
        poly.use_smooth=True
        for loop,coord in zip(poly.loop_indices,coords):uv.data[loop].uv=coord
    if isinstance(weights,str):weights=[{weights:1}]*len(verts)
    for b in sorted({b for ws in weights for b in ws}):ob.vertex_groups.new(name=b)
    for i,ws in enumerate(weights):
        total=sum(ws.values())
        for b,w in ws.items():
            if w>0:ob.vertex_groups[b].add([i],w/total,'REPLACE')
    objects.append(ob);return ob

def tube(name,points,radii,bone,mat,sides=10,weight_fn=None):
    points=[Vector(p) for p in points];verts=[];faces=[];uvs=[];weights=[]
    for i,p in enumerate(points):
        t=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized();ref=Vector((0,0,1)) if abs(t.z)<.9 else Vector((1,0,0))
        a=t.cross(ref).normalized();b=t.cross(a)
        for j in range(sides):
            q=p+float(radii[i])*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides));verts.append(q);weights.append(weight_fn(q) if weight_fn else {bone:1})
    for i in range(len(points)-1):
        for j in range(sides):
            faces.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
            uvs.append([(j/sides,i/(len(points)-1)),((j+1)/sides,i/(len(points)-1)),((j+1)/sides,(i+1)/(len(points)-1)),(j/sides,(i+1)/(len(points)-1))])
    faces.extend([tuple(reversed(range(sides))),tuple(range((len(points)-1)*sides,len(points)*sides))]);uvs.extend([[ (.5,.5)]*sides]*2)
    return mesh(name,verts,faces,mat,uvs,weights)

def ellipsoid(name,center,size,bone,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=40,ring_count=24,location=center);ob=bpy.context.object;ob.name=name;ob.scale=size
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);ob.data.materials.append(mat)
    ob.vertex_groups.new(name=bone).add(list(range(len(ob.data.vertices))),1,'REPLACE')
    for poly in ob.data.polygons:poly.use_smooth=True
    ob.data.uv_layers.active.name='UVMap';objects.append(ob);return ob

def cuticle(name,stage,elytra=False):
    n=2048 if elytra else 1024
    v,u=np.mgrid[0:n,0:n].astype(np.float32)/n;rng=np.random.default_rng(734+stage+int(elytra)*97)
    noise=np.zeros((n,n),np.float32)
    for f,a in [(2,.28),(5,.16),(13,.09),(37,.05),(103,.02)]:
        for k in range(3):
            angle=rng.uniform(0,math.tau);phase=rng.uniform(0,math.tau)
            noise+=a*np.sin((u*math.cos(angle)+v*math.sin(angle))*math.tau*f+phase)
    grain=rng.random((n,n)).astype(np.float32)-.5
    # Irregular punctures sit in gently wandering striae, with a rougher waxy
    # cuticle between them. No baked illumination is included in base colour.
    row=np.floor(v*90).astype(int);fu=(u*26+.09*np.sin(v*24)+(row%2)*.13+noise*.12)%1-.5
    fv=(v*90+noise*.26)%1-.5;pits=np.exp(-((fu/.15)**2+(fv/.20)**2)*2)
    striae=np.exp(-(np.sin((u*13+.018*np.sin(v*30))*math.pi)/.10)**2) if elytra else 0
    height=noise*.015+grain*.0008-pits*.036-striae*.010
    palettes=[([.055,.095,.070],[.075,.085,.059]),([.075,.090,.040],[.12,.085,.034]),([.035,.065,.071],[.060,.050,.082])]
    a,b=map(np.array,palettes[stage-1]);mix=np.clip(.5+noise,.05,.95)
    col=a[None,None,:]*(1-mix[:,:,None])+b[None,None,:]*mix[:,:,None]
    col*= (1-pits*.30+grain*.035)[:,:,None]
    if elytra:
        # Thin worn edges, without a broad painted outline.
        rim=np.exp(-((u-.98)/.014)**2)*.32
        col=col*(1-rim[:,:,None])+np.array([.28,.23,.11])*rim[:,:,None]
    rough=np.clip(.36+noise*.12+pits*.22+grain*.09,.25,.69)
    if elytra:
        deposits=np.zeros_like(u);count=[3,7,12][stage-1]
        for j in range(count):
            t=.15+.7*(j+.3)/count;phi=.24+.47*((j*.618)%1)
            d=((u-phi/(math.pi*.52))/.050)**2+((v-t)/.035)**2
            deposits=np.maximum(deposits,np.clip((1.2-d+noise*.60)*.80,0,1))
        col=col*(1-deposits[:,:,None]*.65)+np.array([.22,.18,.105])*deposits[:,:,None]*.65
        rough=rough*(1-deposits)+.76*deposits
        height+=deposits*(grain*.016+noise*.014)
    return pbr(name,image(name+' colour',col),normal_image(name+' normal',height,10),
        image(name+' MR',np.stack([np.ones_like(u),rough,np.zeros_like(u)],axis=2),True))

def eye_material(name):
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n
    row=np.floor(v*65);a=(u*100+(row%2)*.5)%1-.5;b=(v*65)%1-.5
    lenses=np.maximum(0,1-(a/.52)**2-(b/.56)**2)
    colour=np.tile([.022,.015,.007],(n,n,1))*(.85+lenses[:,:,None]*.15)
    return pbr(name,image(name+' colour',colour),normal_image(name+' normal',lenses*.014,9),
        image(name+' MR',np.stack([np.ones_like(u),.22+(1-lenses)*.12,np.zeros_like(u)],axis=2),True))

def mineral(name):
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n;rng=np.random.default_rng(166)
    cloud=.45+.16*np.sin(u*17+np.sin(v*13))+.10*np.sin(v*42+u*8)
    grain=rng.random((n,n)).astype(np.float32)-.5
    seams=np.exp(-(np.sin((v+u*.24)*math.pi*36)/.10)**2)
    colour=np.stack([.64+cloud*.15,.58+cloud*.16,.40+cloud*.20],axis=2)*(1-seams[:,:,None]*.16)
    rough=np.clip(.29+grain*.09+seams*.15,.22,.61)
    return pbr(name,image(name+' colour',colour),normal_image(name+' normal',seams*.008+grain*.001,9),
        image(name+' MR',np.stack([np.ones_like(u),rough,np.zeros_like(u)],axis=2),True))

def crystal(name,base,radius,height,tilt,bone,mat,rng):
    axis=Vector(tilt).normalized();base=Vector(base);a=axis.cross(Vector((0,1,0))).normalized();b=axis.cross(a)
    verts=[];faces=[];uvs=[];sides=6;phase=rng.uniform(0,math.tau)
    for z,scale in [(0,1),(height*.72,.88),(height,0.12)]:
        for k in range(sides):
            angle=phase+k*math.tau/sides;verts.append(base+axis*z+(a*math.cos(angle)+b*math.sin(angle))*radius*scale)
    for ring in range(2):
        for k in range(sides):
            faces.append((ring*sides+k,ring*sides+(k+1)%sides,(ring+1)*sides+(k+1)%sides,(ring+1)*sides+k))
            uvs.append([(k/sides,ring/2),((k+1)/sides,ring/2),((k+1)/sides,(ring+1)/2),(k/sides,(ring+1)/2)])
    faces+=[tuple(reversed(range(sides))),tuple(range(12,18))];uvs+=[[(.5+.45*math.cos(k*math.tau/sides),.5+.45*math.sin(k*math.tau/sides)) for k in range(sides)]]*2
    ob=mesh(name,verts,faces,mat,uvs,bone)
    for poly in ob.data.polygons:poly.use_smooth=False
    return ob

def compose(name,stage):
    L=[.36,.76,1.13][stage-1];rng=np.random.default_rng(914+stage)
    shell=cuticle(name+' elytra',stage,True);chitin=cuticle(name+' sclerites',stage)
    dark=plain(name+' flexible joint membrane',[.034,.026,.019],.58)
    eye=eye_material(name+' compound eye')
    horn=plain(name+' worn claws and mandibles',[.13,.095,.044],.29)
    quartz=mineral(name+' mineral inclusions');seta=plain(name+' sensory setae',[.28,.22,.105],.56)
    names=['Spine00','Spine01','Spine02','Spine03','Head'];parents=[-1,0,1,2,3]
    heads=[Vector(p)*L for p in [(0,.34,.21),(0,.10,.23),(0,-.09,.23),(0,-.24,.23),(0,-.39,.21)]]
    # A low horizontal body with separate ventral plates and closed wing cases.
    ellipsoid(name+' abdomen',(0,.19*L,.21*L),(.208*L,.29*L,.139*L),'Spine01',dark)
    for j in range(6):
        y=(.015+j*.077)*L;rad=.198*math.sqrt(max(.12,1-((y/L-.19)/.32)**2))*L
        ellipsoid(name+' ventrite '+str(j),(0,y,.148*L),(rad,.046*L,.068*L),'Spine01',chitin)
    ellipsoid(name+' mesothorax',(0,-.092*L,.208*L),(.166*L,.13*L,.104*L),'Spine02',dark)
    pronotum=ellipsoid(name+' pronotum',(0,-.232*L,.244*L),(.191*L,.159*L,.115*L),'Spine03',chitin)
    for vertex in pronotum.data.vertices:
        q=vertex.co/L-Vector((0,-.232,.244))
        q.x=math.copysign(abs(q.x/.191)**.80*.191,q.x)
        q.y=math.copysign(abs(q.y/.159)**.76*.159,q.y)
        if q.z>0:q.z=math.sqrt(q.z/.115)*.115
        vertex.co=(q+Vector((0,-.232,.244)))*L
    head=ellipsoid(name+' head capsule',(0,-.405*L,.225*L),(.128*L,.112*L,.078*L),'Head',chitin)
    for vertex in head.data.vertices:
        q=vertex.co/L-Vector((0,-.405,.225));q.x=math.copysign(abs(q.x/.128)**.78*.128,q.x)
        if q.z>0:q.z=math.sqrt(q.z/.078)*.078
        vertex.co=(q+Vector((0,-.405,.225)))*L
    ellipsoid(name+' clypeus',(0,-.486*L,.215*L),(.099*L,.055*L,.046*L),'Head',chitin)

    def wing_point(t,phi,side):
        curve=max(.002,math.sin(.13+t*(math.pi-.15)))**.48
        width=.232*L*curve;dome=.164*L*curve
        # Subtle ridges displace the actual silhouette without a faceted shell.
        ripple=(.0007*L)*math.sin(phi*20+math.sin(t*7))*(math.sin(phi)**.5)
        return Vector((side*(.0025*L+width*math.sin(phi)),(-.107+.607*t)*L,.215*L+dome*math.cos(phi)+ripple))
    for side in [-1,1]:
        verts=[];faces=[];uvs=[];nu=90;nv=48
        for i in range(nu+1):
            for j in range(nv+1):verts.append(wing_point(i/nu,j/nv*math.pi*.52,side))
        for i in range(nu):
            for j in range(nv):
                f=(i*(nv+1)+j,(i+1)*(nv+1)+j,(i+1)*(nv+1)+j+1,i*(nv+1)+j+1)
                uv=[(j/nv,i/nu),(j/nv,(i+1)/nu),((j+1)/nv,(i+1)/nu),((j+1)/nv,i/nu)]
                if side>0:f=tuple(reversed(f));uv=list(reversed(uv))
                faces.append(f);uvs.append(uv)
        ob=mesh(name+' left elytron' if side<0 else name+' right elytron',verts,faces,shell,uvs,'Spine01');select(ob)
        mod=ob.modifiers.new('Wing case thickness','SOLIDIFY');mod.thickness=.003*L;bpy.ops.object.modifier_apply(modifier=mod.name)
        # Mineral growth follows each case, leaving its centre seam free.
        count=[3,7,12][stage-1]
        for j in range(count):
            t=.15+.7*(j+.3)/count;phi=.24+.47*((j*0.618)%1);p=wing_point(t,phi,side)
            r=L*rng.uniform(.010,.018)*(1+stage*.2);height=L*rng.uniform(.020,.045)*stage
            crystal(name+' elytral crystal',p-Vector((0,0,.006*L)),r,height,(side*.22,.15,1),'Spine01',quartz,rng)
            for k in range(2):
                offset=Vector((rng.uniform(-1,1),rng.uniform(-1,1),-.04))*r
                crystal(name+' basal crystal',p+offset,r*.55,height*.42,(side*.5,.25,1),'Spine01',quartz,rng)

    # Six articulated limbs. Front tibiae broaden into digging paddles in the
    # later stages; all tarsi have separate cuticular segments and paired claws.
    for side,letter in [(-1,'L'),(1,'R')]:
        for row in range(3):
            up=f'Leg{letter}{row}Up';lo=f'Leg{letter}{row}Lo';parent=[3,2,1][row]
            hip=Vector((side*.13,[-.26,-.09,.055][row],.205))*L
            knee=Vector((side*[.31,.36,.33][row],[-.355,-.005,.26][row],.185))*L
            ankle=Vector((side*[.38,.435,.395][row],[-.50,-.00,.46][row],.040))*L
            toe=ankle+Vector((side*.050,[-.10,.025,.115][row],-.027))*L
            names.extend([up,lo]);parents.extend([parent,len(names)-2]);heads.extend([hip,knee])
            ellipsoid(name+' '+up+' coxa',hip,(.052*L,.055*L,.05*L),up,dark)
            pts=[hip.lerp(knee,t) for t in np.linspace(0,1,16)]
            tube(name+' '+up+' femur',pts,[L*(.014+.024*math.sin(math.pi*t)**.65) for t in np.linspace(0,1,16)],up,chitin,16)
            ellipsoid(name+' '+lo+' knee',knee,(.022*L,.022*L,.023*L),lo,dark)
            pts=[knee.lerp(ankle,t) for t in np.linspace(0,1,18)]
            radii=[L*(.024*(1-t)+.011*t)*(1+(stage-1)*.25*(math.sin(t*math.pi)**2) if row==0 else 1) for t in np.linspace(0,1,18)]
            tube(name+' '+lo+' tibia',pts,radii,lo,chitin,14)
            # Serrated tibial spurs emerge along the outer face.
            for k in range(5 if row==0 else 3):
                t=.25+.12*k;base=knee.lerp(ankle,t)+Vector((side*.017*L,0,0))
                tip=base+Vector((side*(.017+.006*stage),-.012 if row==0 else .010,.007))*L
                tube(name+' tibial spur',[base,base.lerp(tip,.6),tip],[.009*L,.006*L,.0008*L],lo,horn,8)
            for j in range(5):
                p=ankle.lerp(toe,j/5);q=ankle.lerp(toe,(j+.90)/5)
                tube(name+' tarsomere',[p,p.lerp(q,.5),q],[.010*L,.013*L,.009*L],lo,chitin,10)
            for s in [-1,1]:
                base=toe+Vector((s*.008*L,0,0));tip=base+Vector((s*.013,-.024 if row<2 else .024,-.006))*L
                tube(name+' hooked pretarsal claw',[base,base+Vector((s*.011*L,0,.010*L)),tip],[.004*L,.003*L,.0005*L],lo,horn,8)
            for k in range(18):
                t=rng.uniform(.12,.96);p=knee.lerp(ankle,t)+Vector((side*.014*L,0,.007*L))
                tip=p+Vector((side*rng.uniform(.008,.019),rng.uniform(-.009,.009),rng.uniform(.009,.024)))*L
                tube(name+' leg tactile seta',[p,p.lerp(tip,.6),tip],[.0007*L,.0004*L,.00006*L],lo,seta,4)

    for side,letter in [(-1,'L'),(1,'R')]:
        # Small lateral compound eyes sit flush against the head capsule.
        ellipsoid(name+' '+letter+' compound eye',(side*.119*L,-.433*L,.242*L),(.016*L,.038*L,.026*L),'Head',eye)
        base=Vector((side*.084,-.470,.244))*L;elbow=Vector((side*.165,-.550,.262))*L;tip=Vector((side*.245,-.621,.292))*L
        bone='Antenna'+letter;names.append(bone);parents.append(4);heads.append(base)
        pts=[base.lerp(elbow,t) for t in np.linspace(0,1,8)]
        tube(name+' antennal scape',pts,[L*(.009-.003*t) for t in np.linspace(0,1,8)],bone,chitin,10)
        for j in range(7):
            p=elbow.lerp(tip,j/7);q=elbow.lerp(tip,(j+.86)/7)
            tube(name+' antennomere',[p,p.lerp(q,.5),q],[.006*L,.009*L,.006*L],bone,chitin,10)
        for j in range(3):
            p=tip+Vector((side*.005*j,-.007*j,0))*L
            ob=ellipsoid(name+' antennal lamella',p,(.008*L,.025*L,.014*L),bone,chitin)
            angle=side*(.40+j*.21)
            for vertex in ob.data.vertices:
                q=vertex.co-p;vertex.co=p+Vector((q.x*math.cos(angle)-q.y*math.sin(angle),q.x*math.sin(angle)+q.y*math.cos(angle),q.z))
        for j in range(2):
            base=Vector((side*(.04+j*.045),-.505,.198))*L
            tip=Vector((side*(.012+j*.094),-.57-j*.017,.19))*L
            mid=base.lerp(tip,.45)+Vector((side*.026*L,0,.008*L))
            tube(name+' mandible' if j==0 else name+' maxillary palp',[base,mid,tip],[L*(.022 if j==0 else .007),L*(.020 if j==0 else .009),.0015*L],'Head',horn if j==0 else chitin,12)
    if stage>1:
        for side in [-1,1]:
            base=Vector((side*.08,-.25,.34))*L
            crystal(name+' pronotal mineral',base,.022*L,(.07 if stage==2 else .14)*L,(side*.2,-.22,1),'Spine03',quartz,rng)
    return names,parents,heads

def deliver(name,stage,provenance=None):
    global objects
    for ob in list(SCENE.objects):bpy.data.objects.remove(ob,do_unlink=True)
    objects=[];names,parents,heads=compose(name,stage)
    assert len(names)==19 and len(parents)==19
    select(objects[0])
    for ob in objects:ob.select_set(True)
    bpy.ops.object.join();body=bpy.context.object;body.name=name
    floor=min(v.co.z for v in body.data.vertices)
    for v in body.data.vertices:v.co.z-=floor
    heads=[p-Vector((0,0,floor)) for p in heads]
    data=bpy.data.armatures.new(name+' insect anatomy');rig=bpy.data.objects.new(name+' insect anatomy',data);SCENE.collection.objects.link(rig);select(rig);bpy.ops.object.mode_set(mode='EDIT')
    for i,n in enumerate(names):
        b=data.edit_bones.new(n);b.head=heads[i];b.tail=b.head+Vector((0,0,.03))
        if parents[i]>=0:b.parent=data.edit_bones[names[parents[i]]]
    bpy.ops.object.mode_set(mode='OBJECT');body.parent=rig;select(body)
    tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    for v in body.data.vertices:
        active=[g for g in v.groups if g.weight>1e-8]
        assert active and len(active)<=4 and abs(sum(g.weight for g in active)-1)<.001
    mod=body.modifiers.new('Anatomical skin','ARMATURE');mod.object=rig;rig.select_set(True)
    folder=OUT/name;folder.mkdir(exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(folder/(name+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True,export_skins=True,
        export_copyright=provenance.get('copyright','') if provenance else '')
    bpy.data.libraries.write(str(folder/(name+'.blend')),{SCENE},fake_user=True)
    refs=['https://australian.museum/learn/animals/insects/christmas-beetles/christmas-beetles-glossary/']
    (folder/'provenance.json').write_text(json.dumps(provenance or {'source':'original-mineral-beetle-authoring','recipe':'Tools/Art/create_mineral_beetles.py','externalAssets':[],
        'anatomyReferences':refs,'features':'Original paired elytra, punctate cuticle, ventrites, six articulated legs with tarsi/claws/setae, segmented lamellate antennae, mandibles and mineral growth.',
        'triangles':len(body.data.polygons),'visualAcceptance':False},indent=2))
    print('MINERAL_BEETLE_SOURCE',name,len(body.data.vertices),len(body.data.polygons),flush=True)
    floor_mat=plain(name+' preview floor',[.07,.085,.083],.92)
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.002));bpy.context.object.data.materials.append(floor_mat)
    extent=max(max(v.co[k] for v in body.data.vertices)-min(v.co[k] for v in body.data.vertices) for k in range(3))
    target=Vector(tuple((min(v.co[k] for v in body.data.vertices)+max(v.co[k] for v in body.data.vertices))*.5 for k in range(3)))
    for offset,power,size in [((1,-1.3,2),300,1.2),((-1,-.5,1.1),150,2),((0,1,1.8),320,1.1)]:
        bpy.ops.object.light_add(type='AREA',location=target+Vector(offset)*extent);lamp=bpy.context.object;lamp.data.energy=power*extent**2;lamp.data.size=size*extent;lamp.rotation_euler=(target-lamp.location).to_track_quat('-Z','Y').to_euler()
    SCENE.world=bpy.data.worlds.new(name+' daylight');SCENE.world.use_nodes=True;SCENE.world.node_tree.nodes.get('Background').inputs[0].default_value=(.20,.24,.29,1);SCENE.world.node_tree.nodes.get('Background').inputs[1].default_value=.3
    bpy.ops.object.camera_add(location=target+Vector((1.35,-1.8,1.25))*extent);cam=bpy.context.object;SCENE.camera=cam;cam.data.lens=58;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    SCENE.render.resolution_x=1400;SCENE.render.resolution_y=1100;SCENE.render.resolution_percentage=100;SCENE.render.filepath=str(folder/(name+'_offline.png'));bpy.ops.render.render(write_still=True)

if __name__=='__main__':
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for stage,name in enumerate(['Prismite','Geoborer','Crystallisk'],1):
        if not requested or name in requested:deliver(name,stage)
