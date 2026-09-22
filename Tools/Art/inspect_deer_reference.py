"""Inspect the CC0 CDmir deer with Blender --disable-autoexec.

Does not save or modify the downloaded .blend. This is a source assessment,
not a delivered Zenithmon creature.
"""
from pathlib import Path
import bpy, json
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Build/artifacts/authored_assets/reference_sources/CDmir_Deer'
result = {'objects': [], 'images': [], 'materials': []}
for ob in bpy.data.objects:
    item = {'name': ob.name, 'type': ob.type, 'dimensions': list(ob.dimensions), 'location': list(ob.location)}
    if ob.type == 'MESH':
        item.update(vertices=len(ob.data.vertices), faces=len(ob.data.polygons),
                    materials=[m.name if m else None for m in ob.data.materials],
                    groups=[g.name for g in ob.vertex_groups])
    if ob.type == 'ARMATURE':
        item['bones'] = [{'name': b.name, 'head': list(b.head_local), 'tail': list(b.tail_local)} for b in ob.data.bones]
    result['objects'].append(item)
for image_index, im in enumerate(bpy.data.images):
    result['images'].append({'name': im.name, 'size': list(im.size), 'filepath': im.filepath, 'packed': bool(im.packed_file)})
    if im.size[0] and im.packed_file:
        im.filepath_raw = str(OUT / (f'image_{image_index:02d}_' + Path(im.name).stem + '.png')); im.file_format = 'PNG'; im.save()
for m in bpy.data.materials:
    result['materials'].append({'name': m.name, 'nodes': [n.name for n in m.node_tree.nodes] if m.node_tree else []})
(OUT / 'inspection.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

# Modern preview wiring for the legacy Blender-Internal materials. Roughness
# below is an assessment default, not a recovered artist-authored PBR channel.
for name, colour, normal in [('Body','body','deer-body-normal.png'),('Head','head','deer-head-normal.png'),('Antlers','deer-antlers.png','deer-antlers-normal.png')]:
    mat=bpy.data.materials[name];mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;n.clear()
    out=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');l.new(bs.outputs[0],out.inputs['Surface'])
    tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images[colour];tex.image.colorspace_settings.name='sRGB';l.new(tex.outputs['Color'],bs.inputs['Base Color'])
    tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images[normal];tex.image.colorspace_settings.name='Non-Color'
    nm=n.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.45;l.new(tex.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal']);bs.inputs['Roughness'].default_value=.8
mat=bpy.data.materials['Eye'];mat.use_nodes=True;n=mat.node_tree.nodes;n.clear();out=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');bs.inputs['Base Color'].default_value=(.009,.006,.003,1);bs.inputs['Roughness'].default_value=.18;mat.node_tree.links.new(bs.outputs[0],out.inputs[0])
for ob in bpy.data.objects:
    ob.hide_render = ob.type!='ARMATURE' and ob.name not in ['Body','Head','Eyes','Horns']
    if ob.type=='ARMATURE':ob.data.pose_position='REST'
scene=bpy.context.scene;scene.frame_set(1);bpy.context.view_layer.update()
targets=[bpy.data.objects[n] for n in ['Body','Head','Eyes','Horns']]
points=[ob.matrix_world@Vector(p) for ob in targets for p in ob.bound_box]
minimum=Vector(tuple(min(p[k] for p in points) for k in range(3)));maximum=Vector(tuple(max(p[k] for p in points) for k in range(3)))
center=(minimum+maximum)*.5;height=maximum.z-minimum.z
bpy.ops.mesh.primitive_plane_add(size=100,location=(0,0,minimum.z-.01));floor=bpy.context.object
mat=bpy.data.materials.new('Assessment floor');mat.diffuse_color=(.12,.13,.11,1);floor.data.materials.append(mat)
for offset,energy,size in [((1.3,-1.5,2),2200,5),((-1,.3,1.5),1300,4)]:
    bpy.ops.object.light_add(type='AREA',location=center+Vector(offset)*height);ob=bpy.context.object;ob.data.energy=energy;ob.data.shape='DISK';ob.data.size=size;ob.rotation_euler=(center-ob.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=center+Vector((1.5,-2.1,.6))*height);camera=bpy.context.object;camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.lens=58;scene.camera=camera
scene.world=bpy.data.worlds.new('Assessment world');scene.world.use_nodes=True;scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.22,.23,.25,1);scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.4
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
scene.render.filepath=str(OUT/'source_assessment.png');bpy.ops.render.render(write_still=True)
