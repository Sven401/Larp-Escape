"""Blender 4.0 illustration of fit-prototype envelopes; no optical simulation."""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[3]
ASSETS=ROOT/'.tmp/compact-mechanics-render'
OUT=Path(__file__).resolve().parent/'mechanik'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.cavity_type='BOTH'
scene.display.shading.background_type='WORLD'
scene.world.color=(0.85,0.88,0.91)
scene.render.resolution_x=1400;scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard'

def material(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1)
    return m

colors={'base':(.13,.16,.19),'tray':(.32,.36,.40),'lid':(.13,.16,.19),
 'diffuser':(.94,.81,.48),'contact_cap_negative':(.4,.42,.44),
 'contact_cap_positive':(.4,.42,.44),'CELL_MAX_ENVELOPE':(.15,.25,.52),
 'POGO_NEGATIVE':(.82,.66,.19),'POGO_POSITIVE':(.82,.66,.19),
 'POWERBOOST_ALLOWANCE':(.12,.37,.24),'QTPY_ALLOWANCE':(.14,.23,.36),
 'BFF_ALLOWANCE':(.92,.71,.22),'SWITCH_BODY':(.12,.12,.12)}
objects={}
for name,c in colors.items():
    bpy.ops.wm.stl_import(filepath=str(ASSETS/(name+'.stl')))
    obj=bpy.context.object;obj.name=name;obj.data.materials.append(material(name,c))
    for poly in obj.data.polygons: poly.use_smooth=False
    objects[name]=obj
bpy.ops.object.camera_add(location=(115,-135,120))
camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=105
camera.rotation_euler=(Vector((33,17,18))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera=camera
scene.render.filepath=str(OUT/'assembled.png');bpy.ops.render.render(write_still=True)
for name in ['tray','POWERBOOST_ALLOWANCE','QTPY_ALLOWANCE','BFF_ALLOWANCE']:
    objects[name].location.z+=31
objects['lid'].location.z+=66;objects['diffuser'].location.z+=66
objects['contact_cap_negative'].location.z+=12
objects['contact_cap_positive'].location.z+=12
camera.location=(128,-152,150)
camera.rotation_euler=(Vector((33,17,58))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.ortho_scale=195
scene.render.filepath=str(OUT/'exploded.png');bpy.ops.render.render(write_still=True)
# Illustration-only front cutaway. Never exported as printable CAD geometry.
bpy.ops.mesh.primitive_cube_add(size=1,location=(33,3.0,22))
cut=bpy.context.object;cut.name='DISPLAY_ONLY_CUT';cut.dimensions=(72,20,36)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
mod=objects['base'].modifiers.new('Illustration cutaway','BOOLEAN')
mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cut
cut.hide_render=True
scene.render.filepath=str(OUT/'cutaway-exploded.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ASSETS/'compact-preview.blend'))
