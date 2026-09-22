"""Original fire-line Blender sources, continuous voxel sculpt and baked PBR.

Blender --background --factory-startup --disable-autoexec --python-exit-code 1
--python Tools/Art/create_ember_bipeds.py -- [Kindlet Scorchel Pyroclast]
No external meshes/textures. Outputs are review candidates, not accepted art.
"""
from pathlib import Path
import sys, math, json
import bpy, numpy as np
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_lancefish as art
art.OUT=art.ROOT/'Build/artifacts/authored_assets/creature_work/EmberBipeds'
art.OUT.mkdir(parents=True,exist_ok=True)
NAMES=['Spine00','Spine01','Spine02','Spine03','Head','ArmLUp','ArmLLo',
       'ArmRUp','ArmRLo','LegLUp','LegLLo','LegRUp','LegRLo','Crest']
PARENTS=[-1,0,1,2,3,3,5,3,7,0,9,0,11,4]


def sculpt_material(name,stage):
    """Object-space patterns are baked after union, avoiding limb UV seams."""
    m=bpy.data.materials.new(name+' sculpt shader');m.use_nodes=True
    n=m.node_tree.nodes;l=m.node_tree.links;bs=n.get('Principled BSDF')
    tex=n.new('ShaderNodeTexCoord');separate=n.new('ShaderNodeSeparateXYZ');l.new(tex.outputs['Generated'],separate.inputs[0])
    noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=6.5;noise.inputs['Detail'].default_value=5;noise.inputs['Roughness'].default_value=.72
    l.new(tex.outputs['Generated'],noise.inputs['Vector'])
    pigment=n.new('ShaderNodeValToRGB')
    colours=([(.085,.023,.009,1),(.39,.12,.033,1),(.64,.30,.095,1)],
             [(.046,.016,.008,1),(.22,.062,.021,1),(.46,.19,.060,1)],
             [(.024,.020,.017,1),(.11,.047,.023,1),(.31,.11,.035,1)])[stage-1]
    pigment.color_ramp.elements[0].position=.34;pigment.color_ramp.elements[0].color=colours[0]
    pigment.color_ramp.elements[1].position=.65;pigment.color_ramp.elements[1].color=colours[2]
    pigment.color_ramp.elements.new(.49).color=colours[1];l.new(noise.outputs['Fac'],pigment.inputs[0])
    belly=n.new('ShaderNodeMapRange');belly.inputs['From Min'].default_value=.33;belly.inputs['From Max'].default_value=.08
    belly.inputs['To Max'].default_value=.60
    l.new(separate.outputs['Y'],belly.inputs['Value'])
    mix=n.new('ShaderNodeMixRGB');mix.inputs[2].default_value=(.19,.076,.022,1);l.new(belly.outputs['Result'],mix.inputs[0]);l.new(pigment.outputs[0],mix.inputs[1]);l.new(mix.outputs[0],bs.inputs['Base Color'])
    cells=n.new('ShaderNodeTexVoronoi');cells.feature='DISTANCE_TO_EDGE';cells.inputs['Scale'].default_value=105
    l.new(tex.outputs['Generated'],cells.inputs['Vector'])
    ridge=n.new('ShaderNodeValToRGB');ridge.color_ramp.elements[0].position=.018;ridge.color_ramp.elements[0].color=(.05,.05,.05,1)
    ridge.color_ramp.elements[1].position=.12;ridge.color_ramp.elements[1].color=(.8,.8,.8,1);l.new(cells.outputs['Distance'],ridge.inputs[0])
    micro=n.new('ShaderNodeTexNoise');micro.inputs['Scale'].default_value=450;micro.inputs['Detail'].default_value=2
    l.new(tex.outputs['Generated'],micro.inputs['Vector'])
    bump0=n.new('ShaderNodeBump');bump0.inputs['Strength'].default_value=.22;bump0.inputs['Distance'].default_value=.00045
    l.new(micro.outputs['Fac'],bump0.inputs['Height'])
    bump=n.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.62;bump.inputs['Distance'].default_value=.0013
    l.new(ridge.outputs[0],bump.inputs['Height']);l.new(bump0.outputs[0],bump.inputs['Normal']);l.new(bump.outputs[0],bs.inputs['Normal'])
    rough=n.new('ShaderNodeMapRange');rough.inputs['To Min'].default_value=.39;rough.inputs['To Max'].default_value=.68
    l.new(noise.outputs['Fac'],rough.inputs['Value']);l.new(rough.outputs['Result'],bs.inputs['Roughness'])
    return m


def bake_skin(ob,name,stage):
    art.select(ob);ob.data.materials.clear();m=sculpt_material(name,stage);ob.data.materials.append(m)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=1.15,island_margin=.012);bpy.ops.object.mode_set(mode='OBJECT')
    ob.data.uv_layers.active.name='UVMap'
    nodes=m.node_tree.nodes;links=m.node_tree.links;bs=nodes.get('Principled BSDF');output=nodes.get('Material Output')
    emit=nodes.new('ShaderNodeEmission');target=nodes.new('ShaderNodeTexImage');nodes.active=target
    art.SCENE.cycles.samples=8;art.SCENE.render.bake.margin=12;maps={}
    for kind,socket in [('base','Base Color'),('rough','Roughness'),('normal',None)]:
        im=bpy.data.images.new(name+' baked '+kind,width=2048,height=2048,alpha=False)
        im.colorspace_settings.name='sRGB' if kind=='base' else 'Non-Color';target.image=im
        if kind=='normal':
            links.new(bs.outputs[0],output.inputs['Surface']);bpy.ops.object.bake(type='NORMAL',normal_space='TANGENT')
        else:
            links.new(bs.inputs[socket].links[0].from_socket,emit.inputs['Color']);links.new(emit.outputs[0],output.inputs['Surface']);bpy.ops.object.bake(type='EMIT')
        im.filepath_raw=str(art.OUT/(name+'_'+kind+'.png'));im.file_format='PNG';im.save();im.pack();maps[kind]=im
    values=np.empty(2048*2048*4,np.float32);maps['rough'].pixels.foreach_get(values);values=values.reshape(2048,2048,4)
    mr=art.image(name+' packed MR',np.stack([np.ones((2048,2048)),values[:,:,0],np.zeros((2048,2048))],axis=2),True)
    ob.data.materials.clear();ob.data.materials.append(art.pbr(name+' baked reptile scales',maps['base'],maps['normal'],mr));art.SCENE.cycles.samples=48


def compose(name,stage):
    H=[.61,1.04,1.54][stage-1];bulk=[1.,1.02,1.22][stage-1]
    skin=art.plain(name+' temporary sculpt',[.25,.09,.025],.55)
    def P(p):return Vector(p)*H
    def sphere(label,p,s):return art.ellipsoid(name+' '+label,P(p),P(s),'Spine00',skin)
    def sweep(label,points,radii,sides=24):return art.tube(name+' '+label,[P(p) for p in points],[r*H for r in radii],'Spine00',skin,sides)
    # Overlapping volumes are unioned and smoothed into one watertight dermis.
    sphere('pelvis',(0,.035,.42),(.145*bulk,.132,.18))
    sphere('abdomen',(0,.004,.57),(.137*bulk,.125,.20))
    sphere('rib cage',(0,.018,.70),(.151*bulk,.121,.156))
    sphere('neck',(0,-.015,.80),(.096,.097,.13))
    sphere('cranium',(0,-.051,.889),(.117 if stage==1 else .110,.126,.105))
    sphere('snout',(0,-.143,.865),(.102,.126,.051))
    sphere('lower jaw',(0,-.136,.839),(.086,.113,.033))
    sweep('short muscular balancing tail',[(0,.10,.44),(0,.22,.37),(0,.35,.28),(0,.47,.20),(0,.58,.20),(0,.66,.24)],[.092,.078,.055,.035,.018,.003])
    heads=[(0,.035,.42),(0,.012,.53),(0,.014,.64),(0,.018,.73),(0,-.035,.84)]
    segments=[]
    for side,letter in [(-1,'L'),(1,'R')]:
        arm=[(side*.14,.016,.727),(side*.206,-.006,.615),(side*.210,-.09,.542)]
        sweep(letter+' arm',arm,[.061*bulk,.043,.027]);sphere(letter+' shoulder',arm[0],(.067*bulk,.065,.086))
        sphere(letter+' hand',(side*.213,-.11,.531),(.039,.042,.032))
        for finger in range(3):
            x=side*(.19+.022*finger)
            sweep(letter+' finger',[(x,-.125,.534),(x+side*.008,-.168,.510),(x+side*.01,-.179,.493)],[.009,.007,.003],12)
        hip=(side*.115,.038,.43);knee=(side*.165,-.053,.265);ankle=(side*.160,.011,.106)
        sweep(letter+' leg',[hip,(side*.155,-.024,.342),knee,(side*.170,-.01,.18),ankle],[.097*bulk,.085*bulk,.059,.046,.030])
        sphere(letter+' thigh',(side*.132,.023,.375),(.096*bulk,.087,.132))
        sphere(letter+' knee',knee,(.069,.061,.089))
        sphere(letter+' foot',(side*.16,-.033,.068),(.058,.095,.048))
        for toe in range(3):
            x=side*.16+(toe-1)*.041
            sweep(letter+' toe',[(x,-.07,.066),(x+(toe-1)*.008,-.141,.048),(x+(toe-1)*.012,-.187,.039)],[.019,.015,.006],16)
        heads.extend(arm[:2])
        segments.extend([(f'Arm{letter}Up',P(arm[0]),P(arm[1])),(f'Arm{letter}Lo',P(arm[1]),P((side*.213,-.16,.51)))])
    # Canonical order places both arms before both legs.
    for side,letter in [(-1,'L'),(1,'R')]:
        hip=(side*.115,.038,.43);knee=(side*.165,-.053,.265)
        heads.extend([hip,knee]);segments.extend([(f'Leg{letter}Up',P(hip),P(knee)),(f'Leg{letter}Lo',P(knee),P((side*.16,-.04,.075)))])
    heads.append((0,-.022,.964))
    art.select(art.objects[0])
    for ob in art.objects:ob.select_set(True)
    bpy.ops.object.join();body=bpy.context.object;body.name=name+' continuous dermis'
    mod=body.modifiers.new('Union anatomical volumes','REMESH');mod.mode='VOXEL';mod.voxel_size=H*.0048;mod.use_smooth_shade=True;bpy.ops.object.modifier_apply(modifier=mod.name)
    mod=body.modifiers.new('Relax dermis junctions','SMOOTH');mod.factor=.9;mod.iterations=18;bpy.ops.object.modifier_apply(modifier=mod.name)
    mod=body.modifiers.new('Surface delivery density','DECIMATE');mod.ratio=.40;bpy.ops.object.modifier_apply(modifier=mod.name)
    body.vertex_groups.clear();art.objects=[body]
    for bone in NAMES:body.vertex_groups.new(name=bone)
    def weights(p):
        q=p/H;x,y,z=q
        side='L' if x<0 else 'R'
        def ease(t):
            t=float(np.clip(t,0,1));return t*t*(3-2*t)
        if z>.795:return {'Head':float(np.clip((z-.795)/.055,0,1)),'Spine03':float(1-np.clip((z-.795)/.055,0,1))}
        t=float(np.clip((z-.42)/.31,0,1))*3;i=min(2,int(t));trunk={NAMES[i]:1-(t-i),NAMES[i+1]:t-i}
        if z<.48 and abs(x)>.045 and y<.19:
            t=ease((.31-z)/.105);attach=ease((.48-z)/.16)*ease((abs(x)-.045)/.06)*ease((.19-y)/.07)
            return {**{b:w*(1-attach) for b,w in trunk.items()},f'Leg{side}Up':(1-t)*attach,f'Leg{side}Lo':t*attach}
        if abs(x)>.155 and z>.46:
            t=ease((.657-z)/.10);attach=ease((abs(x)-.155)/.05)
            return {**{b:w*(1-attach) for b,w in trunk.items()},f'Arm{side}Up':(1-t)*attach,f'Arm{side}Lo':t*attach}
        return trunk
    for v in body.data.vertices:
        for b,w in weights(v.co).items():
            if w>1e-7:body.vertex_groups[b].add([v.index],w,'REPLACE')
    bake_skin(body,name,stage)
    dark=art.plain(name+' natural orifices',[.014,.009,.005],.42)
    keratin=art.plain(name+' worn claw keratin',[.16,.115,.058],.43)
    eyelid=art.plain(name+' eye rim',[.24,.085,.025],.48)
    # Amber iris has fine radial striations and a narrow vertical pupil.
    n=512;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n
    grain=(.5+.5*np.sin(u*math.tau*119+np.sin(v*31)*1.5))**5
    colour=np.tile([.46,.25,.055],(n,n,1))*(.66+.34*grain[:,:,None]);eye=art.pbr(name+' iris striations',art.image(name+' iris base',colour),art.flat,art.image(name+' iris MR',[1,.24,0],True))
    pupil=art.plain(name+' moist pupil',[.003,.004,.003],.09)
    eye_scale=1-.15*(stage-1)
    for side,letter in [(-1,'L'),(1,'R')]:
        c=(side*.096,-.118,.899)
        art.ellipsoid(name+' recessed eye '+letter,P(c),P((.018,.029*eye_scale,.025*eye_scale)),'Head',eye)
        art.ellipsoid(name+' vertical pupil '+letter,P((side*.108,-.127,.900)),P((.008,.009*eye_scale,.021*eye_scale)),'Head',pupil)
        points=[P((side*(.094+.014*math.sin(a)),-.118+.030*eye_scale*math.cos(a),.899+.026*eye_scale*math.sin(a))) for a in np.linspace(0,math.tau,65)]
        art.tube(name+' fleshy orbital rim '+letter,points,[H*.0035]*len(points),'Head',eyelid,8)
        art.ellipsoid(name+' nostril '+letter,P((side*.055,-.246,.879)),P((.013,.004,.005)),'Head',dark)
        points=[P((side*.101*math.sin(t*1.4),-.143-.120*math.cos(t*1.4),.847-.005*math.sin(t*math.pi))) for t in np.linspace(0,1,40)]
        art.tube(name+' labial seam '+letter,points,[H*.0018]*40,'Head',dark,8)
        for toe in range(3):
            x=side*.16+(toe-1)*.053
            art.tube(name+' toe claw '+letter,[P((x,-.174,.041)),P((x,-.201,.033)),P((x,-.218,.035))],[H*.008,H*.006,H*.0006],f'Leg{letter}Lo',keratin,12)
        for finger in range(3):
            x=side*(.20+.022*finger)
            art.tube(name+' hand claw '+letter,[P((x,-.173,.502)),P((x,-.190,.486)),P((x,-.183,.477))],[H*.004,H*.003,H*.0005],f'Arm{letter}Lo',keratin,10)
    # Irregular mineral ridges are separate rigid osteoderms; skin remains continuous.
    n=1024;v,u=np.mgrid[0:n,0:n].astype(np.float32)/n;rng=np.random.default_rng(4700+stage)
    noise=rng.random((n,n)).astype(np.float32);vein=(.5+.5*np.sin(u*34+np.sin(v*21)*3))**18
    rockcolour=np.tile([.093,.074,.055] if stage==3 else [.26,.105,.031],(n,n,1))*(.78+.4*noise[:,:,None])
    rock=art.pbr(name+' mineralised crest',art.image(name+' plate base',rockcolour),art.normal_image(name+' plate normal',noise*.008+vein*.016,12),art.image(name+' plate MR',np.stack([np.ones_like(v),.65+.19*noise,np.zeros_like(v)],axis=2),True))
    for k in range(5+stage*2):
        t=k/(4+stage*2);z=.95-.51*t;y=.04+.118*math.sin(t*math.pi/2)
        height=(.052+stage*.021)*(1-.40*t);bone='Crest' if k<2 else 'Spine03' if t<.42 else 'Spine02' if t<.68 else 'Spine00'
        points=[P((0,y,z)),P((0,y+.025,z+height*.54)),P((0,y+.042,z+height))]
        art.tube(name+' broken dorsal crest',points,[H*(.024+stage*.006)*(1-.3*t),H*.019,H*.0013],bone,rock,9)
    if stage>1:
        for side,letter in [(-1,'L'),(1,'R')]:
            for k in range(6 if stage==2 else 14):
                row=k//3;j=k%3;z=.73-row*.058;x=side*(.065+j*.043);y=.018+.114*math.sqrt(max(.05,1-(x/(.15*bulk))**2))
                bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=P((x,y,z)))
                ob=bpy.context.object;ob.name=name+' interlocking mineral osteoderm';ob.scale=P((.036,.031,.042));ob.rotation_euler[1]=side*(.1+j*.15)
                bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);ob.data.materials.append(rock)
                ob.vertex_groups.new(name='Spine03' if row<2 else 'Spine02').add(list(range(len(ob.data.vertices))),1,'REPLACE')
                # Icosphere UVs are supplied explicitly for portable glTF maps.
                uv=ob.data.uv_layers.new(name='UVMap')
                for poly in ob.data.polygons:
                    poly.use_smooth=True
                    for li in poly.loop_indices:
                        p=ob.data.vertices[ob.data.loops[li].vertex_index].co/H-Vector((x,y,z))
                        uv.data[li].uv=(.5+math.atan2(p.y,p.x)/math.tau,.5+p.z/.09)
                art.objects.append(ob)
    def mature(p):
        p=p.copy();z=p.z/H;t=float(np.clip((z-.81)/.05,0,1))
        if z>.81:p.z=H*(.81+(z-.81)*(1-.10*(stage-1)))
        p.y-=H*.032*(stage-1)*t;p.x*=1-.035*(stage-1)*t
        return p
    for ob in art.objects:
        for vertex in ob.data.vertices:vertex.co=mature(vertex.co)
    return NAMES,PARENTS,[mature(P(p)) for p in heads]


art.compose=compose
if __name__=='__main__':
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for stage,name in enumerate(['Kindlet','Scorchel','Pyroclast'],1):
        if requested and name not in requested:continue
        art.deliver(name,stage,{'source':'original-ember-biped-authoring','recipe':'Tools/Art/create_ember_bipeds.py','externalSources':[],
            'features':'Unified voxel sculpt; articulated biped anatomy; embedded 2048px baked dermis base, normal, MR; claws, iris, nostrils and mineral crest.',
            'visualAcceptance':False,'limitations':'Requires in-engine anatomy, animation and photorealistic finishing review. Procedural material optical values are artistic estimates.'})
