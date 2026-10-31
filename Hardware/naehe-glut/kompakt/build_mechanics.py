"""Compact module-case fit prototype; run with FreeCAD 1.0 bundled Python.

All geometry is editable through the parameters/functions below. This creates
fit-prototype parts, not evidence of RF, contact, thermal or battery safety.
"""
import hashlib
import json
import math
from pathlib import Path

import FreeCAD as App
import Mesh
import MeshPart
import Part

OUT = Path(__file__).resolve().parent / 'mechanik'
V = App.Vector
P = dict(length=66.0, width=34.0, height=38.4, wall=1.6,
         base_height=36.4, cell_diameter_max=18.75,
         cell_length_nominal=38.7, cell_length_min=38.4,
         cell_length_max=39.0, cell_axis_y=17.0, cell_axis_z=11.5,
         contact_free_left=10.0, contact_free_right=47.4,
         contact_height=5.0038, contact_tail=1.7526,
         contact_full_stroke_nominal=1.397, contact_full_stroke_min=1.27,
         contact_length_tolerance_each=0.1524, pcb_bottom=23.3,
         qtpy_x=42.0, qtpy_y=2.0, led_bottom=30.2)
POSTS = [(3.5, 3.5), (3.5, 30.5), (62.5, 30.5)]


def box(x,y,z,dx,dy,dz):
    return Part.makeBox(dx,dy,dz,V(x,y,z))


def cylinder(x,y,z,r,h,axis=V(0,0,1)):
    return Part.makeCylinder(r,h,V(x,y,z),axis)


def rounded(x,y,z,dx,dy,dz,r):
    s = box(x+r,y,z,dx-2*r,dy,dz).fuse(box(x,y+r,z,dx,dy-2*r,dz))
    for a,b in [(x+r,y+r),(x+dx-r,y+r),(x+r,y+dy-r),(x+dx-r,y+dy-r)]:
        s = s.fuse(cylinder(a,b,z,r,dz))
    return s.removeSplitter()


def hexhole(x,y,z,af,h):
    r = af/math.sqrt(3)
    p = [V(x+r*math.cos(math.pi/6+i*math.pi/3),
           y+r*math.sin(math.pi/6+i*math.pi/3),z) for i in range(6)]
    return Part.Face(Part.makePolygon(p+[p[0]])).extrude(V(0,0,h))


def pogo(free_x, direction, compression=0):
    # 0906-2 drawing dimensions in inches converted to mm; flange is at root.
    root = free_x - direction*P['contact_height']
    axis = V(direction,0,0)
    z = P['cell_axis_z']; y = P['cell_axis_y']
    flange = cylinder(root,y,z,0.9144,0.7112,axis)
    barrel = cylinder(root,y,z,0.8128,2.6924,axis)
    plunger = cylinder(root+direction*2.3,y,z,0.5334,
                       P['contact_height']-2.3-compression,axis)
    tail = cylinder(root-direction*P['contact_tail'],y,z,0.2159,
                    P['contact_tail'],axis)
    return flange.fuse(barrel).fuse(plunger).fuse(tail).removeSplitter()


def contact_cut(shape, free_x, direction):
    # Broad assembly cavity, not a press fit; flange is trapped by split clamp.
    root = free_x-direction*P['contact_height']
    z = P['cell_axis_z']; y = P['cell_axis_y']
    cut = cylinder(root-direction*2.2,y,z,0.65,2.5,V(direction,0,0))
    cut = cut.fuse(cylinder(root-direction*0.18,y,z,1.10,1.1,V(direction,0,0)))
    cut = cut.fuse(cylinder(root+direction*0.7,y,z,0.98,4.0,V(direction,0,0)))
    return shape.cut(cut)


def build():
    w,d,h = P['length'],P['width'],P['base_height']; wall=P['wall']
    base = rounded(0,0,0,w,d,h,3).cut(rounded(wall,wall,wall,
                    w-2*wall,d-2*wall,h,1.4))
    # Three lid screws, away from the designated antenna-end side wall.
    for x,y in POSTS:
        post = cylinder(x,y,0,3.0,h)
        post = post.cut(cylinder(x,y,h-8,1.1,9))
        post = post.cut(hexhole(x,y,h-5,4.3,6))
        base = base.fuse(post)
    # Low cell cradles leave the cell lift path open after the electronics tray
    # is removed. No screw or spring-metal sharp edge enters the cell sleeve.
    for x in [16.0,38.0]:
        rib = box(x,5.0,wall,2.0,24.0,5.0)
        rib = rib.cut(cylinder(x-1,17.0,11.5,9.675,4,V(1,0,0)))
        base = base.fuse(rib)
    # 4-mm insulated textile lift ribbon lies below the cell; no metal pry tool.
    base = base.cut(box(27.0,3.0,1.2,4.0,28.0,1.5))
    caps = []
    for free_x, direction in [(P['contact_free_left'],1),
                              (P['contact_free_right'],-1)]:
        root = free_x-direction*P['contact_height']
        x0 = root-1.6
        block = box(x0,10.2,wall,5.0,13.6,11.5-wall)
        cap = box(x0,10.2,11.5,5.0,13.6,2.4)
        block = contact_cut(block,free_x,direction)
        cap = contact_cut(cap,free_x,direction)
        for y in [12.0,22.0]:
            block = block.cut(cylinder(root+0.7,y,3.0,1.1,12))
            block = block.cut(hexhole(root+0.7,y,5.0,4.3,1.8))
            # Nut insertion is from outside the contact block, not cell side.
            block = block.cut(box(x0-0.1,y-2.2,5.0,2.0,4.4,1.8))
            cap = cap.cut(cylinder(root+0.7,y,11.3,1.1,3))
        base = base.fuse(block)
        caps.append(cap)
    # Wiring channels to the rear wall are below the battery contact height.
    base = base.cut(box(2.0,23.0,7.5,55.0,4.0,2.0))
    # The EN switch is low-current only. Its pins point inward, not through cell.
    # Body 11.6x4x5.4, pins 4.7, actuator 2mm. Actuator is in a finger recess.
    base = base.fuse(box(11.5,25.8,28.1,13.0,7.0,0.8))
    for x in [11.1,23.7]:
        base = base.fuse(box(x,26.0,28.1,0.8,6.8,5.2))
    base = base.cut(box(12.0,31.3,28.7,12.0,4.0,5.0))
    # USB-C service mouth is at the front, with a 12x8mm opening for real plug.
    base = base.cut(box(44.8,-1.0,22.3,12.0,5.0,8.0))
    # Front/rear ledges for a removable insulating electronics tray.
    for y in [1.4,29.1]:
        base = base.fuse(box(7.0,y,20.9,52.0,3.5,1.3))
    # Four small ventilation slots: no waterproof/IP claim.
    for x in [8,14,20,26]:
        base = base.cut(box(x,-1,25.7,2.5,4,1.5))
    base = base.removeSplitter()

    # Removable tray is lifted to service the cell. All wires stay flexible.
    tray = box(7.0,4.5,22.2,53.0,25.0,0.8)
    # Keep USB service corridor and low-power EN switch lead chamber clear.
    tray = tray.cut(box(40.5,4.0,22.0,19.5,3.0,1.5))
    # Pockets do not clamp component bodies or load the cell.
    for x,y,dx,dy in [(7,5.5,29,23),(42,2.0,17.8,20.7)]:
        for bx,by,bw,bd in [(x-0.5,y-0.5,dx+1,0.4),
                           (x-0.5,y+dy+0.1,dx+1,0.4),
                           (x-0.5,y,0.4,dy)]:
            # The QT Py front wall is deliberately omitted at USB.
            if x==42 and by < 4.5: continue
            rail=box(bx,by,22.2,bw,bd,2.4)
            # Connect rails to the tray even beside the USB notch.
            if rail.common(tray).Volume>0.001:
                tray=tray.fuse(rail)
    # Upper LED-board shelf sits above complete QT Py envelope, not on its ICs.
    led_shelf = box(40.8,3.3,29.2,20.0,21.7,0.8)
    # Optical centre remains open. Two full-height edge ribs join lower tray.
    led_shelf = led_shelf.cut(box(43.5,5.0,29.0,14.5,18.0,2.0))
    for x in [40.8,60.0]:
        led_shelf=led_shelf.fuse(box(x,24.0,22.2,0.8,1.0,7.8))
    tray = tray.fuse(led_shelf)
    for x,y in POSTS:
        tray=tray.cut(cylinder(x,y,20,3.3,12))
    tray=tray.removeSplitter()

    lid = rounded(0,0,h,w,d,2.0,3)
    for x,y in POSTS:
        lid=lid.cut(cylinder(x,y,h-0.1,1.1,2.3))
    # Window clears the actual ~13mm LED-grid footprint. Diffuser is removable.
    lid=lid.cut(rounded(42.0,4.0,h-0.1,17.6,18.0,2.3,1))
    # Underside counterbore leaves 0.6mm positive seating lip.
    lid=lid.cut(rounded(40.8,2.8,h-0.1,20.0,20.4,1.5,1))
    diffuser=rounded(41.05,3.05,h-0.0,19.5,19.9,0.8,0.8)
    # Two small lid-attached retaining shelves support plate from below.
    # They engage its edge, outside the illuminated aperture.
    for x in [39.9,59.9]:
        lid=lid.fuse(box(x,3.0,h-0.7,1.8,20.0,1.0))
    # Leave assembled clearance below diffuser; it sits on its lid seat lip.
    diffuser.translate(V(0,0,0.35))

    refs={
        'CELL_MAX_ENVELOPE':cylinder(9.2,17.0,11.5,9.375,39.0,V(1,0,0)),
        'POGO_NEGATIVE':pogo(10.0,1,0.8),
        'POGO_POSITIVE':pogo(47.4,-1,0.8),
        # 4.5mm allowance includes SMD and wire/solder, vs advertised2mm PCB.
        'POWERBOOST_ALLOWANCE':box(7.0,5.5,23.3,29.0,23.0,4.5),
        'QTPY_ALLOWANCE':box(42.0,2.0,23.3,17.8,20.7,5.7).fuse(
            box(46.4,1.0,23.3,9.0,7.5,3.2)),
        'BFF_ALLOWANCE':box(42.0,2.4,30.2,17.9,20.9,2.8),
        'SWITCH_BODY':box(12.0,26.2,29.0,11.6,5.4,4.0).fuse(
            box(15.0,31.6,30.0,4.5,2.0,2.0)),
        'SWITCH_PIN_ROOM':box(14.0,21.5,30.0,6.5,4.7,1.0),
        'USB_PLUG_ROOM':box(44.8,-10.0,22.3,12.0,11.0,8.0),
    }
    parts={'base':base,'tray':tray,'lid':lid.removeSplitter(),
           'diffuser':diffuser,'contact_cap_negative':caps[0],
           'contact_cap_positive':caps[1]}
    return parts,refs


def main():
    OUT.mkdir(exist_ok=True)
    parts,refs=build()
    doc=App.newDocument('NaeheGlutCompactFit')
    params=doc.addObject('Spreadsheet::Sheet','Parameters')
    params.set('A1','Fit prototype, no powered assembly release')
    for i,(k,v) in enumerate(P.items(),3):
        params.set('A'+str(i),k);params.set('B'+str(i),str(v))
    report={'parameters':P,'source':'483c3320 + confirmed compact-module brief',
            'parts':{},'collisions_mm3':{},'module_collisions_mm3':{},
            'fit_prototype_only':True,
            'open_physical_items':['positive-button guard geometry unverified; no reverse-polarity protection claimed',
                'contact flange seating/stroke/cell extraction and FDM tolerance coupon',
                'actual board revisions, solder, wire routing and USB plug shape',
                'RF in enclosure and carry position; no generic15mm module rule applied to chip antenna',
                'temperature, optical diffusion and4h runtime'],
            'powered_assembly_released':False}
    for name,s in parts.items():
        o=doc.addObject('Part::Feature',name);o.Label=name;o.Shape=s
        o.addProperty('App::PropertyString','Status');o.Status='Unpowered fit prototype'
        printed=s.copy()
        b=printed.BoundBox;printed.translate(V(-b.XMin,-b.YMin,-b.ZMin))
        mesh=MeshPart.meshFromShape(Shape=printed,LinearDeflection=0.06,
                                  AngularDeflection=0.15,Relative=False)
        mesh.write(str(OUT/(name+'.stl')))
        Part.export([o],str(OUT/(name+'.step')))
        reread=Mesh.Mesh(str(OUT/(name+'.stl')))
        read_step=Part.Shape();read_step.read(str(OUT/(name+'.step')))
        report['parts'][name]={'valid':s.isValid(),'solids':len(s.Solids),
             'volume_mm3':round(s.Volume,4),'stl_closed':reread.isSolid(),
             'non_manifold':reread.hasNonManifolds(),
             'self_intersections':reread.hasSelfIntersections(),
             'step_valid':read_step.isValid(),'step_solids':len(read_step.Solids),
             'bounds_mm':[round(printed.BoundBox.XLength,4),
                          round(printed.BoundBox.YLength,4),
                          round(printed.BoundBox.ZLength,4)]}
    for name,s in refs.items():
        o=doc.addObject('Part::Feature','REF_'+name);o.Shape=s
    # Explicitly reserved service space must clear print, like physical modules.
    for rn,rs in refs.items():
        for pn,ps in parts.items():
            v=rs.common(ps).Volume
            if v>0.01:report['collisions_mm3'][rn+'/'+pn]=round(v,4)
    names=list(parts)
    for i,a in enumerate(names):
        for b in names[i+1:]:
            v=parts[a].common(parts[b]).Volume
            if v>0.01:report['collisions_mm3'][a+'/'+b]=round(v,4)
    # USB room joins its socket by design; switch pin room touches switch body.
    mods=[n for n in refs if n not in ['USB_PLUG_ROOM','SWITCH_PIN_ROOM']]
    for i,a in enumerate(mods):
        for b in mods[i+1:]:
            v=refs[a].common(refs[b]).Volume
            if v>0.01:report['module_collisions_mm3'][a+'/'+b]=round(v,4)
    report['nominal_contact_compression_each_mm']=(38.7-37.4)/2
    # Includes cell +/-0.3, assumed combined printed seat +/-0.4 and two
    # contact-length tolerances +/-0.1524 each. Must be checked after assembly.
    span=(0.3+0.4+2*P['contact_length_tolerance_each'])/2
    report['assumed_tolerance_contact_compression_range_mm']=[round(0.65-span,4),round(0.65+span,4)]
    report['geometric_passed']=all(v['valid'] and v['solids']==1 and v['stl_closed']
                and not v['non_manifold'] and not v['self_intersections']
                and v['step_valid'] and v['step_solids']==1
                for v in report['parts'].values()) and not report['collisions_mm3'] \
                and not report['module_collisions_mm3']
    doc.recompute();doc.saveAs(str(OUT/'NG-Compact-Fit.FCStd'))
    Part.export([o for o in doc.Objects if hasattr(o,'Shape')],str(OUT/'assembly.step'))
    render_dir=Path(__file__).resolve().parents[3]/'.tmp/compact-mechanics-render'
    render_dir.mkdir(exist_ok=True)
    for name,s in {**parts,**refs}.items():
        MeshPart.meshFromShape(Shape=s,LinearDeflection=0.12,
                              AngularDeflection=0.2,Relative=False).write(str(render_dir/(name+'.stl')))
    (OUT/'geometry-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    App.closeDocument(doc.Name)
    if not report['geometric_passed']:
        raise SystemExit('Fit prototype has open geometric issues; see report')


if __name__=='__main__':
    main()
