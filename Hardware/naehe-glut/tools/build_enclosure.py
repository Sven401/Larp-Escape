"""Generate the NG-R1 enclosure with FreeCAD 1.0.2's bundled Python.

From the repository root:
  & 'C:/Program Files/FreeCAD 1.0/bin/python.exe' Hardware/naehe-glut/tools/build_enclosure.py
All coordinates and exported meshes are millimetres. This is a digital design,
not evidence of a physical fit, water resistance, or RF qualification.
"""
import json
import hashlib
import math
from pathlib import Path

import FreeCAD as App
import Part
import Mesh
import MeshPart

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Hardware/naehe-glut/mechanik'
V = App.Vector
P = dict(width=168.0, depth=148.0, wall=3.0, base_height=87.0,
         lid_thickness=3.0, corner_radius=6.0, tray_z=38.0,
         tray_thickness=3.0, pcb_bottom_z=46.0, pcb_thickness=1.6,
         ring_x=84.0, ring_y=52.0, ring_od=65.5, ring_id=52.3,
         ring_thickness=3.2, ring_holder_z=74.6,
         screw_clearance=3.4, nut_af=5.8, nut_depth=2.8)
CORNERS = [(9,9),(159,9),(9,139),(159,139)]
TRAY_MOUNTS = [(25,9),(151,20),(25,117),(151,117)]
PCB_MOUNTS = [(125,132),(50,132),(125,62),(50,62)]
RING_MOUNTS = [(84+40*math.cos(math.radians(a)),52+40*math.sin(math.radians(a)))
               for a in (30,150,270)]


def box(x,y,z,dx,dy,dz):
    return Part.makeBox(dx,dy,dz,V(x,y,z))


def cyl(x,y,z,r,h):
    return Part.makeCylinder(r,h,V(x,y,z))


def rounded(x,y,z,dx,dy,dz,r):
    shape=box(x+r,y,z,dx-2*r,dy,dz).fuse(box(x,y+r,z,dx,dy-2*r,dz))
    for cx,cy in [(x+r,y+r),(x+dx-r,y+r),(x+r,y+dy-r),(x+dx-r,y+dy-r)]:
        shape=shape.fuse(cyl(cx,cy,z,r,dz))
    return shape.removeSplitter()


def hexprism(x,y,z,af,h):
    rr=af/math.sqrt(3)
    vs=[V(x+rr*math.cos(math.radians(30+i*60)),y+rr*math.sin(math.radians(30+i*60)),z)
        for i in range(6)]
    return Part.Face(Part.makePolygon(vs+[vs[0]])).extrude(V(0,0,h))


def hole(shape,x,y,z,h):
    return shape.cut(cyl(x,y,z,P['screw_clearance']/2,h))


def build():
    w,d,h=P['width'],P['depth'],P['base_height']
    base=rounded(0,0,0,w,d,h,6).cut(rounded(3,3,3,w-6,d-6,h,3))
    # Lid bosses intersect walls, rather than only touching tangentially.
    for x,y in CORNERS:
        post=cyl(x,y,0,7,h)
        post=hole(post,x,y,h-16,17)
        post=post.cut(hexprism(x,y,h-6.2,P['nut_af'],P['nut_depth']))
        # Nut enters from the chamber side; 5.8 mm bridge in printing.
        direction=1 if y<d/2 else -1
        post=post.cut(box(x-2.9,y if direction>0 else y-8,h-6.2,5.8,8,2.8))
        base=base.fuse(post)
    # Tray supports have open-top captive nut pockets, installed before tray.
    for x,y in TRAY_MOUNTS:
        post=cyl(x,y,0,5,P['tray_z'])
        post=hole(post,x,y,P['tray_z']-12,13)
        post=post.cut(hexprism(x,y,P['tray_z']-2.8,5.8,2.9))
        base=base.fuse(post)
    # Low, open-ended battery rails: 122 x 86 mm clear area; ports face +X.
    for bx,by,dx,dy in [(8,13,2,86),(8,11,124,2),(8,99,124,2)]:
        base=base.fuse(box(bx,by,3,dx,dy,7))
    for sy in (7,103):
        base=base.cut(box(59,sy,-1,22,3,5))
    # Deliberate ventilation; enclosure is NOT weatherproof.
    for y in (35,45,55,65,75):
        base=base.cut(box(-1,y,12,5,5,15))
    base=base.removeSplitter()

    tray=rounded(18,4,38,132,136,3,3)
    tray=tray.cut(box(28,14,37,112,116,5))
    for y in (62,132):
        tray=tray.fuse(box(18,y-5,38,132,10,3))
    for x,y in CORNERS:
        tray=tray.cut(cyl(x,y,37,8,5))
    for x,y in TRAY_MOUNTS:
        tray=tray.fuse(cyl(x,y,38,5,3))
        tray=hole(tray,x,y,37,5)
    for x,y in PCB_MOUNTS:
        tray=tray.fuse(cyl(x,y,38,5,8))
        tray=hole(tray,x,y,37,10)
        tray=tray.cut(hexprism(x,y,37.9,5.8,2.9))
    tray=tray.removeSplitter()

    lid=rounded(0,0,h,w,d,3,6)
    # Shallow locating rim sits inside base; boss cutouts avoid collision.
    rim=rounded(3.4,3.4,h-1.5,w-6.8,d-6.8,1.5,3)
    rim=rim.cut(rounded(5.4,5.4,h-2,w-10.8,d-10.8,3,2))
    for x,y in CORNERS:
        rim=rim.cut(cyl(x,y,h-2,7.5,4))
    lid=lid.fuse(rim)
    lid=lid.cut(cyl(P['ring_x'],P['ring_y'],h-2,33.5,6))
    for x,y in CORNERS+RING_MOUNTS:
        lid=hole(lid,x,y,h-3,7)
    lid=lid.removeSplitter()

    # Diffuser is a removable cap; flange rests underneath lid. No glue needed.
    diffuser=Part.makeCone(36.5,33.25,3.4,V(84,52,h-3.4)).fuse(cyl(84,52,h,33.25,3.0)).removeSplitter()
    # Uniform outer 1.2 mm optical membrane, with a shallow cavity below it.
    diffuser=diffuser.cut(cyl(84,52,h-3.5,31.8,5.3)).removeSplitter()

    rz=P['ring_holder_z']
    holder=cyl(84,52,rz,34.5,5.4).cut(cyl(84,52,rz-1,25.7,7.4))
    holder=holder.cut(cyl(84,52,rz+1.8,33.15,4.0))
    # Three arms/posts support the holder and retain the diffuser flange.
    for x,y in RING_MOUNTS:
        dx,dy=x-84,y-52
        arm=box(84,49.5,rz,40,5,1.8)
        arm.rotate(V(84,52,0),V(0,0,1),math.degrees(math.atan2(dy,dx)))
        holder=holder.fuse(arm).fuse(cyl(x,y,rz,5,h-rz))
        holder=hole(holder,x,y,rz-1,h-rz+2)
        holder=holder.cut(hexprism(x,y,rz-0.1,5.8,2.9))
    # Ring cable exits via inner opening; clips prevent it falling out on handling.
    # Ring is held with three small reusable cable ties through these pairs.
    for a in (0,120,240):
        for rad in (26.5,33.8):
            x=84+rad*math.cos(math.radians(a));y=52+rad*math.sin(math.radians(a))
            holder=holder.cut(cyl(x,y,rz-1,1.6,8))
    holder=holder.cut(cyl(84,52,h-3.4,36.7,3.5)).removeSplitter()

    # Coupon: nut pocket, clearance hole and ring/locating-fit segment.
    coupon=box(0,0,0,50,24,4)
    coupon=coupon.fuse(cyl(10,12,0,7,10))
    coupon=hole(coupon,10,12,-1,12).cut(hexprism(10,12,-0.1,5.8,2.9))
    coupon=coupon.fuse(box(25,3,4,20,2,3))
    coupon=coupon.fuse(box(25,9,4,20,2,3))
    coupon=coupon.removeSplitter()
    parts=dict(base=base,tray=tray,lid=lid,diffuser=diffuser,ring_holder=holder,coupon=coupon)
    # Explicit reference envelopes. Detailed electrical modules are verified separately.
    battery=box(12,16,3,118,82,26) # alternate 26mm battery revision also admitted
    pcb=box(45,57,46,85,80,1.6)
    for x,y in PCB_MOUNTS: pcb=pcb.cut(cyl(x,y,45,1.6,4))
    pcb=pcb.cut(box(60.5,122,45,20,16,4))
    pcb_step=ROOT/'Hardware/naehe-glut/pcb/exports/naehe-glut-r1.step'
    if pcb_step.exists():
        pcb=Part.Shape();pcb.read(str(pcb_step));pcb.translate(V(45,137,46))
    esp=box(57.73,80.26,58,25.4,62.74,1.6)
    antenna=box(57.73,127.5,59.6,25.4,15.5,3.5)
    ring=cyl(84,52,rz+1.8,32.75,3.2).cut(cyl(84,52,rz+1.7,26.15,3.4))
    usb_c=box(102,32,47.6,16,25,17)
    esp_usb=box(60,55,57,20,25,10)
    battery_plugs=box(130,29,6,32,20,20).fuse(box(130,69,6,32,20,20))
    refs=dict(battery=battery,pcb=pcb,esp=esp,antenna=antenna,led_ring=ring,
              usb_c_plug_clearance=usb_c,esp_usb_plug_clearance=esp_usb,
              battery_plug_clearance=battery_plugs)
    def fasteners(points,top,length,nut_bottom):
        shapes=[]
        for x,y in points:
            shapes.extend([cyl(x,y,top-length,1.5,length),cyl(x,y,top,2.75,3),
                           hexprism(x,y,nut_bottom,5.5,2.4)])
        return Part.makeCompound(shapes)
    refs['metal_lid_fasteners']=fasteners(CORNERS,90,12,80.8)
    refs['metal_tray_fasteners']=fasteners(TRAY_MOUNTS,41,10,35.2)
    refs['nylon_pcb_fasteners']=fasteners(PCB_MOUNTS,47.6,10,38)
    refs['metal_ring_fasteners']=fasteners(RING_MOUNTS,90,16,rz)
    # Bounds supplement missing STEP models / optional IC socket / capacitor tolerance.
    refs['BOUND_F1']=cyl(120.54,77,47.6,4.25,8)
    refs['BOUND_U1_socket']=box(95.8,89.8,47.6,8.1,19.2,12)
    refs['BOUND_C1']=cyl(115.5,89,47.6,5,22)
    return parts,refs


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    parts,refs=build()
    doc=App.newDocument('NG_R1_Enclosure')
    params=doc.addObject('App::FeaturePython','Parameters')
    for key,value in P.items():
        params.addProperty('App::PropertyLength',key,'Dimensions')
        setattr(params,key,value)
    report={'revision':'NG-R1-mechanical-draft','freecad_version':'.'.join(App.Version()[:3]),
            'units':'mm','parameters':P,'parts':{},'collisions_mm3':{},
            'notes':['Digital checks do not establish physical fit, RF performance or water resistance.',
                     'Cable envelopes are reserved design spaces, not certified dimensions of a selected cable.']}
    pcb_step=ROOT/'Hardware/naehe-glut/pcb/exports/naehe-glut-r1.step'
    report['pcb_step_sha256']=hashlib.sha256(pcb_step.read_bytes()).hexdigest() if pcb_step.exists() else None
    report['pcb_step_transform']='KiCad STEP + (45,137,46); KiCad layout (u,v) -> (45+u,137-v)'
    for name,shape in parts.items():
        obj=doc.addObject('Part::Feature',name);obj.Shape=shape
        # Preserve assembly coordinates in FCStd/STEP; move each STL to bed at z=0.
        printed=shape.copy()
        if name in ('lid','diffuser'):
            # Outside face lies on the bed; inner locating rim prints upwards.
            printed.rotate(V(0,0,0),V(1,0,0),180)
        b=printed.BoundBox
        printed.translate(V(-b.XMin,-b.YMin,-b.ZMin))
        mesh=MeshPart.meshFromShape(Shape=printed,LinearDeflection=0.08,
                                  AngularDeflection=0.15,Relative=False)
        mesh.write(str(OUT/(name+'.stl')))
        Part.export([obj],str(OUT/(name+'.step')))
        reread=Mesh.Mesh(str(OUT/(name+'.stl')))
        reread_step=Part.Shape();reread_step.read(str(OUT/(name+'.step')))
        bounds=printed.BoundBox
        item={'valid_brep':shape.isValid(),'solids':len(shape.Solids),
              'volume_mm3':shape.Volume,'stl_closed':reread.isSolid(),
              'stl_facets':reread.CountFacets,'stl_volume_mm3':reread.Volume,
              'stl_non_manifold':reread.hasNonManifolds(),
              'stl_self_intersections':reread.hasSelfIntersections(),
              'stl_non_uniform_orientation':reread.hasNonUniformOrientedFacets(),
              'step_valid':reread_step.isValid(),'step_solids':len(reread_step.Solids),
              'print_bounds_mm':[bounds.XLength,bounds.YLength,bounds.ZLength]}
        report['parts'][name]=item
    for name,shape in refs.items():
        obj=doc.addObject('Part::Feature','REF_'+name);obj.Shape=shape
    # Physical modules must not intersect manufactured solids. Contact is allowed.
    for rn,rs in refs.items():
        for pn,ps in parts.items():
            if pn=='coupon':continue
            volume=rs.common(ps).Volume
            if volume>0.01:report['collisions_mm3'][rn+' / '+pn]=round(volume,4)
    report['module_collisions_mm3']={}
    rn=list(refs)
    for i,a in enumerate(rn):
        for b in rn[i+1:]:
            if (a=='pcb' and b.startswith('BOUND_')) or (b=='pcb' and a.startswith('BOUND_')):
                continue # Deliberate conservative enclosure of a component already in STEP.
            vol=refs[a].common(refs[b]).Volume
            if vol>0.01:report['module_collisions_mm3'][a+' / '+b]=round(vol,4)
    # At z=83.6 the diffuser cone rests on three uncut portions of the posts.
    support_probe=cyl(84,52,83.5,36.5,0.1)
    report['diffuser_support_area_mm2']=round(parts['ring_holder'].common(support_probe).Volume/0.1,4)
    # Pairwise collision check of assembled printed parts.
    names=[n for n in parts if n!='coupon']
    for i,a in enumerate(names):
        for b in names[i+1:]:
            vol=parts[a].common(parts[b]).Volume
            if vol>0.01:report['collisions_mm3'][a+' / '+b]=round(vol,4)
    antenna_clearance=box(42.73,112.5,44.6,55.4,45.5,33.5)
    report['external_metal_antenna_clearance_mm']=15
    report['antenna_clearance_intersections_mm3']={}
    for name in ('battery','metal_lid_fasteners','metal_tray_fasteners','metal_ring_fasteners'):
        vol=refs[name].common(antenna_clearance).Volume
        if vol>0.01:report['antenna_clearance_intersections_mm3'][name]=round(vol,4)
    report['passed']=all(x['valid_brep'] and x['solids']==1 and x['stl_closed']
                         and x['step_valid'] and x['step_solids']==1
                         and not x['stl_non_manifold'] and not x['stl_self_intersections']
                         and not x['stl_non_uniform_orientation']
                         and max(x['print_bounds_mm'])<=170.01
                         for x in report['parts'].values()) and not report['collisions_mm3'] \
                     and not report['antenna_clearance_intersections_mm3']
    report['passed']=report['passed'] and not report['module_collisions_mm3'] \
                     and report['diffuser_support_area_mm2']>10 \
                     and report['pcb_step_sha256'] is not None
    doc.recompute();doc.saveAs(str(OUT/'NG-R1.FCStd'))
    Part.export([o for o in doc.Objects if hasattr(o,'Shape')],str(OUT/'assembly.step'))
    render_assets=ROOT/'.tmp/naehe-glut-render'
    render_assets.mkdir(parents=True,exist_ok=True)
    for name,shape in {**parts,**refs}.items():
        MeshPart.meshFromShape(Shape=shape,LinearDeflection=0.12,
                              AngularDeflection=0.2,Relative=False).write(str(render_assets/(name+'.stl')))
    (OUT/'geometry-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    App.closeDocument(doc.Name)
    if not report['passed']:raise SystemExit('Mechanical verification failed; inspect geometry-report.json')


if __name__=='__main__':main()
