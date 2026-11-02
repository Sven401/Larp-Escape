"""Render actual assembly-coordinate meshes, not print-bed STL positions."""
from pathlib import Path
import bpy
from mathutils import Vector
OUT=Path(__file__).resolve().parents[1]/'mechanik'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
s=bpy.context.scene;s.render.engine='BLENDER_WORKBENCH';s.display.shading.light='STUDIO';s.display.shading.color_type='MATERIAL';s.display.shading.show_cavity=True;s.display.shading.cavity_type='BOTH';s.display.shading.background_type='WORLD';s.world.color=(0.94,0.95,0.96)
s.render.resolution_x=1200;s.render.resolution_y=1000;s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG';s.view_settings.view_transform='Standard'
colors={'base':(.25,.40,.50),'insert_r2b':(.40,.65,.73),'clamp_left':(.90,.60,.20),'clamp_right':(.90,.60,.20),'usb_tie_bar':(.50,.65,.70),'jtag_tie_bar':(.50,.65,.70),'pcb':(.12,.42,.20)}
colors.update(USB4105_bound=(.65,.66,.68),FTSH_K_bound=(.18,.18,.20),FFSD_mated_bound=(.30,.30,.32),USB_overmold_bound=(.20,.22,.24),USB_cable_max=(.35,.35,.38),JTAG_ribbon_max=(.65,.50,.35))
colors.update({p.stem:(.35,.36,.39) for p in (OUT/'render-assets').glob('PCBA_*.stl')})
objects={}
for n,c in colors.items():
    bpy.ops.wm.stl_import(filepath=str(OUT/'render-assets'/(n+'.stl')));o=bpy.context.object;o.name=n
    m=bpy.data.materials.new(n);m.diffuse_color=(*c,1);o.data.materials.append(m);objects[n]=o
bpy.ops.object.camera_add(location=(180,220,170));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=165;cam.rotation_euler=(Vector((40,60,10))-cam.location).to_track_quat('-Z','Y').to_euler();s.camera=cam
s.render.filepath=str(OUT/'assembled-draft.png');bpy.ops.render.render(write_still=True)
cam.location=(170,210,-155);cam.rotation_euler=(Vector((40,60,10))-cam.location).to_track_quat('-Z','Y').to_euler()
s.render.filepath=str(OUT/'underside-draft.png');bpy.ops.render.render(write_still=True)
for n in ['USB4105_bound','FTSH_K_bound','FFSD_mated_bound','USB_overmold_bound','USB_cable_max','JTAG_ribbon_max']:objects[n].hide_render=True
for n in objects:
    if n.startswith('PCBA_'):objects[n].hide_render=True
objects['insert_r2b'].location.z+=15;objects['pcb'].location.z+=28
for n in ['clamp_left','clamp_right']:objects[n].location.z+=40
for n in ['usb_tie_bar','jtag_tie_bar']:objects[n].location.z+=10
cam.location=(185,230,185);cam.data.ortho_scale=180;cam.rotation_euler=(Vector((40,60,25))-cam.location).to_track_quat('-Z','Y').to_euler()
s.render.filepath=str(OUT/'exploded-draft.png');bpy.ops.render.render(write_still=True)
