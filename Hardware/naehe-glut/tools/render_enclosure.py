"""Blender 4.0 presentation renderer; run after prepare_render in FreeCAD Python.

This file is also imported by build_enclosure.py's external preparation helper.
Rendered cutaways are illustration only, never exported as printable geometry.
"""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

root=Path(__file__).resolve().parents[3]
assets=root/'.tmp/naehe-glut-render'
out=root/'Hardware/naehe-glut/mechanik'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=False
scene.display.shading.show_cavity=True
scene.display.shading.cavity_type='BOTH'
scene.display.shading.background_type='WORLD'
scene.world.color=(0.85,0.88,0.90)
scene.eevee.use_gtao=True
scene.eevee.gtao_distance=12
scene.eevee.gtao_factor=1.2
scene.render.resolution_x=1400
scene.render.resolution_y=1100
scene.render.resolution_percentage=100
scene.world.color=(0.95,0.95,0.95)
scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=0.8
scene.render.image_settings.file_format='PNG'

def material(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1)
    m.use_nodes=True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*color,1)
    m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=0.58
    return m

colors={'base':(0.32,0.52,0.60),'tray':(0.50,0.65,0.70),'lid':(0.32,0.52,0.60),
        'diffuser':(0.93,0.85,0.64),'ring_holder':(0.50,0.65,0.70),
        'battery':(0.3,0.3,0.32),'pcb':(0.05,0.36,0.18),'esp':(0.08,0.1,0.1),
        'antenna':(0.12,0.12,0.12),'led_ring':(0.85,0.6,0.08)}
objects={}
for name,color in colors.items():
    bpy.ops.wm.stl_import(filepath=str(assets/(name+'.stl')))
    obj=bpy.context.object;obj.name=name;obj.data.materials.append(material(name,color))
    objects[name]=obj
    for polygon in obj.data.polygons: polygon.use_smooth=False

def text(name,content,pos,size):
    curve=bpy.data.curves.new(name,'FONT');curve.body=content;curve.size=size
    curve.align_x='CENTER';curve.extrude=0
    obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj)
    obj.location=pos;obj.data.materials.append(material(name,(0.9,0.9,0.9)))

text('lid_label','NAEHE GLUT  /  R1',(84,112,90.1),5)
text('battery_label','V50  /  48 Wh',(71,59,29.1),6)

bpy.ops.mesh.primitive_plane_add(size=2000,location=(84,74,-1))
floor=bpy.context.object;floor.data.materials.append(material('Background',(0.9,0.92,0.93)))
for loc,power,size in [((80,-120,320),350000,220),((-180,140,180),250000,180),((250,200,240),300000,160)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    light=bpy.context.object;light.data.energy=power;light.data.shape='DISK';light.data.size=size
    light.rotation_euler=(Vector((84,74,50))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(305,-285,260))
camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=265
camera.rotation_euler=(Vector((84,74,45))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera=camera
scene.render.filepath=str(out/'assembled.png');bpy.ops.render.render(write_still=True)

# Exploded assembly: display clearances instead of hiding modules behind walls.
for name in ('tray','pcb','esp','antenna'):
    objects[name].location.z+=65
objects['ring_holder'].location.z+=100;objects['led_ring'].location.z+=100
objects['lid'].location.z+=145;objects['diffuser'].location.z+=145
bpy.data.objects['lid_label'].location.z+=145
camera.location=(350,-360,310)
camera.rotation_euler=(Vector((84,74,120))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.ortho_scale=415
scene.render.filepath=str(out/'exploded.png');bpy.ops.render.render(write_still=True)

# Direct top view before tray installation makes the battery bay inspectable.
for name,obj in objects.items():
    obj.hide_render=name not in ('base','battery')
bpy.data.objects['lid_label'].hide_render=True
camera.location=(84,74,350)
camera.rotation_euler=(Vector((84,74,0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.ortho_scale=220
scene.render.filepath=str(out/'battery-bay.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(assets/'enclosure-preview.blend'))
