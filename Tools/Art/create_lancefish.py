"""Original aquatic-line authoring with Blender's Python API.

Finlet, Marlance and Tidesabre retain the eight aquatic joint names/hierarchy,
but use horizontal anatomical rest positions. All metallic/roughness PBR maps
are embedded. Outputs remain review candidates until explicitly installed.
"""
from pathlib import Path
import bpy, numpy as np, math, json, sys
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Build/artifacts/authored_assets/creature_work/Lancefish'
OUT.mkdir(parents=True,exist_ok=True)
SCENE=bpy.data.scenes.new('Lancefish anatomical sources');bpy.context.window.scene=SCENE
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

flat=image('Lancefish flat normal',[.5,.5,1],True)
def plain(name,colour,rough,metal=0):
    return pbr(name,image(name+' base',colour),flat,image(name+' MR',[1,rough,metal],True))

def normal_image(name,height,strength):
    dy,dx=np.gradient(height);n=np.stack([-dx*strength,-dy*strength,np.ones_like(height)],axis=2)
    n/=np.linalg.norm(n,axis=2)[:,:,None];return image(name,n*.5+.5,True)

def skin_material(name,stage):
    n=2048;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n;rng=np.random.default_rng(600+stage)
    rows=72 if stage==1 else 106;cols=40 if stage==1 else 56
    row=np.floor(v*rows).astype(int);col=np.floor(u*cols+(row%2)*.5).astype(int)%cols
    a=(u*cols+(row%2)*.5)%1-.5;b=(v*rows)%1-.48
    scale=np.maximum(0,1-(a/.56)**2-(b/.62)**2)**.7
    pigment=rng.uniform(.985,1.015,(rows+1,cols))[row,col]
    grain=rng.random((n,n)).astype(np.float32)-.5
    z=np.sin(u*math.tau);back=np.clip((z-.1)/.70,0,1);belly=np.clip((-z-.05)/.65,0,1)
    side=np.array([.42,.61,.66] if stage==1 else [.33,.49,.56] if stage==2 else [.40,.49,.52])
    dorsal=np.array([.018,.095,.13] if stage<3 else [.025,.065,.085]);ventral=np.array([.73,.74,.65])
    colour=np.tile(side,(n,n,1));colour=colour*(1-back[:,:,None])+dorsal*back[:,:,None]
    colour=colour*(1-belly[:,:,None])+ventral*belly[:,:,None]
    colour*=((.97+.025*scale)*pigment+grain*.015)[:,:,None]
    lateral=np.exp(-(z/.045)**2)*np.clip((v-.20)/.15,0,1)*np.clip((.96-v)/.1,0,1)
    colour*=1-lateral[:,:,None]*.18
    bars=(.5+.5*np.sin(v*math.tau*(9 if stage==1 else 14)))**6
    colour*=1-(bars*np.clip(z,0,1)*.09)[:,:,None]
    head=np.clip((v-.14)/.13,0,1)
    height=(scale*.013+grain*.002)*head
    rough=np.clip(.32+back*.13+(1-scale)*.08+grain*.06,.23,.55)
    return pbr(name+' wet cycloid scales',image(name+' ScaleBase',np.clip(colour,0,1)),
        normal_image(name+' ScaleNormal',height,18),image(name+' ScaleMR',np.stack([np.ones_like(v),rough,np.zeros_like(v)],axis=2),True))

def fin_material(name,stage):
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n;rng=np.random.default_rng(stage+809)
    grain=rng.random((n,n)).astype(np.float32)-.5;rays=(.5+.5*np.cos(u*math.tau*40))**8
    colour=np.tile([.11,.23,.27] if stage<3 else [.11,.16,.19],(n,n,1))
    colour*= (.65+.40*v+.10*rays+grain*.025)[:,:,None]
    colour=colour*(1-v[:,:,None]**5*.4)+np.array([.38,.48,.45])*v[:,:,None]**5*.4
    rough=np.clip(.60+grain*.07+(1-v)*.12,.45,.82)
    return pbr(name+' fin membranes',image(name+' FinBase',colour),normal_image(name+' FinNormal',rays*.014+grain*.001,16),
        image(name+' FinMR',np.stack([np.ones_like(v),rough,np.zeros_like(v)],axis=2),True))

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
            q=p+radii[i]*(a*math.cos(j*math.tau/sides)+b*math.sin(j*math.tau/sides));verts.append(q);weights.append(weight_fn(q) if weight_fn else {bone:1})
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

def compose(name,stage):
    length=[.50,.91,1.28][stage-1];centre=.27*length
    material=skin_material(name,stage);finmat=fin_material(name,stage)
    raymat=plain(name+' segmented fin rays',[.25,.36,.35],.41)
    dark=plain(name+' mouth and gill creases',[.012,.019,.018],.42)
    iris=plain(name+' iris',[.22,.26,.17],.31)
    cornea=plain(name+' wet pupil',[.003,.005,.005],.11)
    bone_names=['Spine00','Spine01','Spine02','Head','FinDorsal','FinPecL','FinPecR','FinCaudal']
    parents=[-1,0,1,2,2,1,1,0]
    heads=[(0,.36*length,centre),(0,.12*length,centre),(0,-.12*length,centre),(0,-.35*length,centre),
        (0,-.07*length,centre+.13*length),(-.11*length,-.14*length,centre),(.11*length,-.14*length,centre),(0,.49*length,centre)]
    def body_weights(p):
        t=p[1]/length+.5
        zones=[(.26,.43,'Head','Spine02'),(.43,.65,'Spine02','Spine01'),(.65,.89,'Spine01','Spine00')]
        if t<.26:return {'Head':1}
        for a,b,left,right in zones:
            if t<=b:
                blend=(t-a)/(b-a);return {left:1-blend,right:blend}
        return {'Spine00':1}
    def radii(t):
        ts=[0,.07,.20,.38,.59,.81,1]
        wide=[.012,.067,.124,.128,.10,.048,.016] if stage==1 else [.013,.061,.10,.112,.086,.038,.014]
        tall=[.020,.076,.145,.160,.119,.049,.020] if stage==1 else [.024,.069,.113,.130,.102,.043,.021]
        # Monotone cubic slopes keep the profile smooth without stopping at
        # each control ring, which would create artificial bumps on the head.
        def interpolate(values,i,u):
            slopes=np.diff(values)/np.diff(ts)
            def slope(k):
                if k==0:return slopes[0]
                if k==len(values)-1:return slopes[-1]
                a,b=slopes[k-1],slopes[k]
                return 0 if a*b<=0 else 2*a*b/(a+b)
            h=ts[i+1]-ts[i]
            return (2*u**3-3*u*u+1)*values[i]+(u**3-2*u*u+u)*h*slope(i)+(-2*u**3+3*u*u)*values[i+1]+(u**3-u*u)*h*slope(i+1)
        for i in range(len(ts)-1):
            if t<=ts[i+1]:
                u=(t-ts[i])/(ts[i+1]-ts[i])
                return length*interpolate(wide,i,u),length*interpolate(tall,i,u)
        return length*wide[-1],length*tall[-1]
    verts=[];weights=[];faces=[];uvs=[];rings=121;sides=96
    for i in range(rings):
        t=i/(rings-1);rx,rz=radii(t)
        for j in range(sides):
            theta=j*math.tau/sides;p=(rx*math.cos(theta),length*(t-.5),centre+rz*math.sin(theta))
            verts.append(p);weights.append(body_weights(p))
    for i in range(rings-1):
        for j in range(sides):
            faces.append((i*sides+j,(i+1)*sides+j,(i+1)*sides+(j+1)%sides,i*sides+(j+1)%sides))
            uvs.append([(j/sides,i/(rings-1)),(j/sides,(i+1)/(rings-1)),((j+1)/sides,(i+1)/(rings-1)),((j+1)/sides,i/(rings-1))])
    faces.extend([tuple(range(sides)),tuple(reversed(range((rings-1)*sides,rings*sides)))]);uvs.extend([[ (.5,.5)]*sides]*2)
    mesh(name+' continuous fish anatomy',verts,faces,material,uvs,weights)
    # Anatomical operculum seams follow the flank; the curved lips leave an
    # actual dark mouth opening, and the pupil sits within its gold iris.
    for side,letter in [(-1,'L'),(1,'R')]:
        rx,rz=radii(.16);eye_y=-.34*length;eye_z=centre+.039*length;eye_x=side*rx*.93
        eye_size=(.026 if stage==1 else .022)*length
        ellipsoid(name+' iris '+letter,(eye_x,eye_y,eye_z),(.010*length,eye_size,eye_size),'Head',iris)
        ellipsoid(name+' pupil '+letter,(eye_x+side*.007*length,eye_y-.001*length,eye_z),(.004*length,eye_size*.68,eye_size*.68),'Head',cornea)
        points=[]
        for k in range(40):
            angle=-1.12+2.30*k/39;t=.265+.026*math.cos(angle);rx,rz=radii(t)
            points.append((side*(rx+.001*length)*math.cos(angle),length*(t-.5),centre+rz*math.sin(angle)))
        tube(name+' opercular crease '+letter,points,[.0017*length]*40,'Head',dark,8,body_weights)
    ellipsoid(name+' mouth opening',(0,-.503*length,centre-.007*length),(.015*length,.005*length,.010*length),'Head',dark)
    for upper in [False,True]:
        points=[(math.cos(a)*.017*length,-.505*length,centre-.007*length+math.sin(a)*.010*length) for a in np.linspace(0 if upper else math.pi,math.pi if upper else math.tau,30)]
        tube(name+' fleshy lip',points,[.0018*length]*30,'Head',raymat,8)

    def fin(label,root,edge,bone):
        root=Vector(root);edge=[Vector(p) for p in edge];verts=[root];weights=[body_weights(root)];faces=[];uvs=[];radial=15
        for k in range(1,radial+1):
            t=k/radial
            for i,p in enumerate(edge):
                q=root.lerp(p,t);q.x+=length*.0025*math.sin(i*math.pi)*math.sin(t*math.pi)
                verts.append(q);body=body_weights(root);ws={b:w*(1-t) for b,w in body.items()};ws[bone]=ws.get(bone,0)+t;weights.append(ws)
        count=len(edge)
        for i in range(count-1):
            faces.append((0,1+i,2+i));uvs.append([((i+.5)/(count-1),0),(i/(count-1),1/radial),((i+1)/(count-1),1/radial)])
        for k in range(radial-1):
            for i in range(count-1):
                faces.append((1+k*count+i,1+(k+1)*count+i,2+(k+1)*count+i,2+k*count+i))
                uvs.append([(i/(count-1),(k+1)/radial),(i/(count-1),(k+2)/radial),((i+1)/(count-1),(k+2)/radial),((i+1)/(count-1),(k+1)/radial)])
        ob=mesh(name+' '+label+' membrane',verts,faces,finmat,uvs,weights);select(ob)
        solid=ob.modifiers.new('Physical fin thickness','SOLIDIFY');solid.thickness=.0012*length;bpy.ops.object.modifier_apply(modifier=solid.name)
        for i,p in enumerate(edge):
            if i%2:continue
            points=[root.lerp(p,t) for t in np.linspace(.03,.97,15)]
            distance=(p-root).length
            def ray_weights(q):
                t=min(1,(q-root).length/max(distance,1e-8));ws={b:w*(1-t) for b,w in body_weights(root).items()};ws[bone]=ws.get(bone,0)+t;return ws
            tube(name+' '+label+' ray',points,[length*(.0011*(1-k/14)+.00025) for k in range(15)],bone,raymat,6,ray_weights)
    # A forked tail, a species-specific dorsal silhouette, and paired swept
    # pectorals share anatomy but are composed independently per stage.
    tail=[]
    for t in np.linspace(0,1,45):
        z=(1-2*t)*(.20 if stage==1 else .26)*length
        y=(.61+(.16 if stage==1 else .23)*abs(1-2*t)**.8)*length
        tail.append((0,y,centre+z))
    fin('forked caudal',(0,.48*length,centre),tail,'FinCaudal')
    dorsal=[]
    for t in np.linspace(0,1,43):
        y=(-.27+.66*t)*length
        rise=(.13 if stage==1 else .30 if stage==2 else .23)*math.sin(math.pi*t)**.7*(1-.45*t)
        if stage==3:rise=float(np.interp(t,[0,.12,.26,.45,.75,1],[0,.16,.28,.13,.035,0]))
        dorsal.append((0,y,centre+(radii(y/length+.5)[1]/length+rise)*length))
    fin('dorsal sail',(0,-.045*length,centre+.095*length),dorsal,'FinDorsal')
    for side,letter in [(-1,'L'),(1,'R')]:
        edge=[]
        for t in np.linspace(0,1,31):
            reach=(.22 if stage==1 else .28)*math.sin(t*math.pi)**(.75 if stage==1 else 2.4)
            edge.append((side*(.095+reach)*length,(-.14+(.42 if stage==1 else .60)*t)*length,centre-(.025+.105*math.sin(t*math.pi))*length))
        fin('pectoral '+letter,(side*.10*length,-.16*length,centre-.01*length),edge,'FinPec'+letter)
    anal=[]
    for t in np.linspace(0,1,29):
        y=(.08+.32*t)*length;drop=(.08 if stage==1 else .12)*math.sin(math.pi*t)**.9
        anal.append((0,y,centre-radii(y/length+.5)[1]-drop*length))
    fin('anal stabiliser',(0,.22*length,centre-.040*length),anal,'Spine00')
    # The juvenile's short rostrum grows into a lance; the final stage carries
    # a broad keratin/iron blade and armoured scutes rather than a scaled copy.
    bill=.055 if stage==1 else .30 if stage==2 else .39
    billmat=plain(name+' rostrum',[.25,.38,.41] if stage<3 else [.38,.43,.45],.31,.0 if stage<3 else .72)
    verts=[];faces=[];uvs=[]
    for i in range(40):
        t=i/39;w=(.012 if stage<3 else .043)*length*(1-t)**.65+.00035*length;h=(.009 if stage<3 else .006)*length*(1-t)+.00025*length
        for j in range(12):
            a=j*math.tau/12;verts.append((w*math.cos(a),(-.48-bill*t)*length,centre+.006*length+h*math.sin(a)))
    for i in range(39):
        for j in range(12):
            faces.append((i*12+j,i*12+(j+1)%12,(i+1)*12+(j+1)%12,(i+1)*12+j));uvs.append([(j/12,i/39),((j+1)/12,i/39),((j+1)/12,(i+1)/39),(j/12,(i+1)/39)])
    mesh(name+' tapered rostrum',verts,faces,billmat,uvs,'Head')
    if stage==3:
        for side in [-1,1]:
            for i in range(14):
                t=.29+i*.044;rx,rz=radii(t);y=(t-.5)*length;z=centre+.026*length
                p=Vector((side*rx,y,z));size=.030*length
                verts=[p+Vector((side*.003*length,0,0)),p+Vector((0,-size,0)),p+Vector((0,0,size*.45)),p+Vector((0,size,0)),p+Vector((0,0,-size*.45))]
                faces=[(0,1,2),(0,2,3),(0,3,4),(0,4,1)]
                if side>0:faces=[tuple(reversed(f)) for f in faces]
                mesh('Tidesabre overlapping iron scute',verts,faces,billmat,[[(.5,.5),(0,0),(1,1)]]*4,[body_weights(p)]*5)
    return bone_names,parents,[Vector(p) for p in heads]

def deliver(name,stage,provenance=None):
    global objects
    for ob in list(SCENE.objects):bpy.data.objects.remove(ob,do_unlink=True)
    objects=[];names,parents,heads=compose(name,stage)
    select(objects[0])
    for ob in objects:ob.select_set(True)
    bpy.ops.object.join();body=bpy.context.object;body.name=name
    floor=min(v.co.z for v in body.data.vertices)
    for v in body.data.vertices:v.co.z-=floor
    heads=[p-Vector((0,0,floor)) for p in heads]
    data=bpy.data.armatures.new(name+' aquatic anatomy');rig=bpy.data.objects.new(name+' aquatic anatomy',data);SCENE.collection.objects.link(rig);select(rig);bpy.ops.object.mode_set(mode='EDIT')
    for i,n in enumerate(names):
        b=data.edit_bones.new(n);b.head=heads[i];b.tail=b.head+Vector((0,0,.04))
        if parents[i]>=0:b.parent=data.edit_bones[names[parents[i]]]
    bpy.ops.object.mode_set(mode='OBJECT');body.parent=rig;select(body)
    tri=body.modifiers.new('Delivery triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    for v in body.data.vertices:
        active=[g for g in v.groups if g.weight>1e-8]
        if not active or len(active)>4 or abs(sum(g.weight for g in active)-1)>.001:raise ValueError('Invalid aquatic skin')
    mod=body.modifiers.new('Aquatic skin','ARMATURE');mod.object=rig;rig.select_set(True)
    folder=OUT/name;folder.mkdir(exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(folder/(name+'.glb')),export_format='GLB',use_selection=True,use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True,export_skins=True,
        export_copyright=provenance.get('copyright','') if provenance else '')
    bpy.data.libraries.write(str(folder/(name+'.blend')),{SCENE},fake_user=True)
    (folder/'provenance.json').write_text(json.dumps(provenance or {'source':'original-lancefish-authoring','recipe':'Tools/Art/create_lancefish.py','externalSources':[],
        'features':'Original continuous fish anatomy, scale and fin PBR maps, modelled gills, eyes, fins/rays, rostrum and adult iron scutes; eight-joint anatomical skin.',
        'triangles':len(body.data.polygons),'visualAcceptance':False},indent=2))
    print('LANCEFISH_SOURCE',name,len(body.data.vertices),len(body.data.polygons),flush=True)
    floor_mat=plain(name+' preview floor',[.10,.12,.13],.9)
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.06));bpy.context.object.data.materials.append(floor_mat)
    extent=max(max(v.co[k] for v in body.data.vertices)-min(v.co[k] for v in body.data.vertices) for k in range(3))
    target=Vector(tuple((min(v.co[k] for v in body.data.vertices)+max(v.co[k] for v in body.data.vertices))*.5 for k in range(3)))
    for offset,power,size in [((1,-1.3,2),360,2),((-1,-.5,1.1),180,2),((0,1,1.8),260,1.5)]:
        bpy.ops.object.light_add(type='AREA',location=target+Vector(offset)*extent);lamp=bpy.context.object;lamp.data.energy=power*extent**2;lamp.data.size=size*extent;lamp.rotation_euler=(target-lamp.location).to_track_quat('-Z','Y').to_euler()
    SCENE.world=bpy.data.worlds.new(name+' aquatic daylight');SCENE.world.use_nodes=True;SCENE.world.node_tree.nodes.get('Background').inputs[0].default_value=(.20,.24,.29,1);SCENE.world.node_tree.nodes.get('Background').inputs[1].default_value=.35
    bpy.ops.object.camera_add(location=target+Vector((1.75,-1.20,.60))*extent);cam=bpy.context.object;SCENE.camera=cam;cam.data.lens=58;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    SCENE.render.resolution_x=1200;SCENE.render.resolution_y=1000;SCENE.render.resolution_percentage=100;SCENE.render.filepath=str(folder/(name+'_offline.png'));bpy.ops.render.render(write_still=True)

if __name__=='__main__':
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for stage,name in enumerate(['Finlet','Marlance','Tidesabre'],1):
        if not requested or name in requested:deliver(name,stage)
