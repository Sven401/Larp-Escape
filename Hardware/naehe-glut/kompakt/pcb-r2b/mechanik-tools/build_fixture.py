"""R2b service-fixture engineering draft. Run with FreeCAD 1.0 bundled Python.
All dimensions mm. Parameters describe an agreed placement draft, NOT qualified fit.
"""
from pathlib import Path
import json, hashlib, math
import FreeCAD as A
import Part, Mesh, MeshPart
BASE=Path(__file__).resolve().parents[1]
OUT=BASE/'mechanik'
V=A.Vector
# Explicitly bounded parametrization: only P drives freely editable dimensions.
# Layout coordinates C are a frozen placement contract, not adjustable parameters.
P={'support_radius':1.25,'screw_clearance':2.3,'nut_af':4.2,'nut_depth':1.8}
C={'pcb_origin_x':25.,'pcb_origin_y':20.,'pcb_bottom_z':12.,'pcb_thickness':1.,
   'pcb_bounds_local':[0.,5.2,30.,43.], 'supports_local':[[1.5,22.],[28.5,22.],[15.,41.]],
   'key_notch_local':[0.,28.,1.5,30.], 'base_footprint_mm':[80.,120.], 'base_plate_thickness_mm':13., 'table_z_mm':-10., 'integrated_foot_height_mm':10., 'base_nut_pocket_depth_mm':4.3, 'side_clearance_mm':0.3,
   'cable_clamp_y':108., 'usb_axis_z':14.73, 'jtag_ribbon_seat_z':20.,
   'status':'fixed engineering placement draft; changing layout requires rebuilding coordinated geometry'}
def box(x,y,z,dx,dy,dz): return Part.makeBox(dx,dy,dz,V(x,y,z))
def cyl(x,y,z,r,h): return Part.makeCylinder(r,h,V(x,y,z))
def hexhole(x,y,z,af,h):
    r=af/math.sqrt(3)
    vs=[V(x+r*math.cos(math.pi/6+i*math.pi/3),y+r*math.sin(math.pi/6+i*math.pi/3),z) for i in range(6)]
    return Part.Face(Part.makePolygon(vs+[vs[0]])).extrude(V(0,0,h))
def bolt(s,x,y,z,h): return s.cut(cyl(x,y,z,P['screw_clearance']/2,h))
def build():
    # Open-ended central aperture: no material below antenna or underside probes.
    base=box(0,0,-10,80,120,13).cut(box(19,0,-11,42,64,15))
    base=base.cut(box(-1,39.8,-11,31,3,8)) # bottom-open battery lead exit,2mm bridge
    for x in (21.5,58.5):
        base=bolt(base,x,68,-11,15).cut(hexhole(x,68,-10.1,P['nut_af'],C['base_nut_pocket_depth_mm']+0.1))
    # Separate USB and ribbon strain-relief tie slots; cable remains accessible.
    for x in (29,36,43.5,53.5): base=base.cut(box(x-1.2,105,-11,2.4,7,15))
    # Raised cable seats keep USB cable straight rather than forcing a tight bend.
    usb_seat=box(30.2,104,3,4.6,9,10.5)
    usb_seat=usb_seat.cut(Part.makeCylinder(2.15,11,V(32.5,103,14.73),V(0,1,0)))
    base=base.fuse(usb_seat).fuse(box(44.7,104,3,7.6,9,17))
    insert=box(20,38,3,7.75,33,3).fuse(box(52.25,38,3,7.75,33,3))
    insert=insert.fuse(box(20,63,3,40,8,3)).fuse(box(38.75,60,3,2.5,7,3))
    for x,y in C['supports_local']:
        insert=insert.fuse(cyl(x+C['pcb_origin_x'],y+C['pcb_origin_y'],6,P['support_radius'],6))
    for x in (21.5,58.5):
        insert=insert.fuse(cyl(x,43,3,3,3)).fuse(cyl(x,43,6,2.5,7))
        insert=bolt(insert,x,43,2,12).cut(hexhole(x,43,2.9,P['nut_af'],P['nut_depth']+0.1))
        insert=insert.fuse(cyl(x,68,3,3,3))
        insert=bolt(insert,x,68,2,5)
    # Short side fences locate board; +Y stop near third support avoids port zones.
    for x in (23.4,55.3): insert=insert.fuse(box(x,39,6,1.3,6,7))
    insert=insert.fuse(box(39.5,63.3,6,1.,1.3,7))
    # Asymmetric matching PCB edge notch, 0.3 mm clearance on closed sides.
    insert=insert.fuse(box(25.3,48.3,6,0.9,1.4,7))
    left=box(19,39,13,5.7,6,3).fuse(box(24.7,40.5,13,3.05,3,3))
    left=bolt(left,21.5,43,12,5)
    right=box(55.3,39,13,5.7,6,3).fuse(box(52.25,40.5,13,3.05,3,3))
    right=bolt(right,58.5,43,12,5)
    # Print two open clamp bars, tied separately to USB and JTAG slots.
    usb=box(27,104,16.88,12,9,3)
    for x in (29,36): usb=usb.cut(box(x-1.2,105,15.88,2.4,7,5))
    jtag=box(41.5,104,20.74,14,9,3)
    for x in (43.5,53.5): jtag=jtag.cut(box(x-1.2,105,19.74,2.4,7,5))
    return {k:v.removeSplitter() for k,v in {'base':base,'insert_r2b':insert,'clamp_left':left,'clamp_right':right,'usb_tie_bar':usb,'jtag_tie_bar':jtag}.items()}
def main():
    OUT.mkdir(exist_ok=True)
    assert 1.0 <= P['support_radius'] <= 1.25
    assert 2.1 <= P['screw_clearance'] <= 2.5
    assert 4.0 <= P['nut_af'] <= 4.4 and 1.5 <= P['nut_depth'] <= 2.0
    doc=A.newDocument('R2bServiceDraft')
    parts=build(); report={'status':'engineering_draft_not_fit_released','editable_parameters':P,'fixed_layout_contract':C,'parts':{},'printed_part_collisions':{},'physical_fit_test':'not_performed','final_component_collision_test':'frozen_PCB_screened_exact_MPN_tolerance_qualification_incomplete'}
    for name,s in parts.items():
        o=doc.addObject('Part::Feature',name);o.Shape=s
        (OUT/'render-assets').mkdir(exist_ok=True)
        MeshPart.meshFromShape(Shape=s,LinearDeflection=0.05,AngularDeflection=0.15,Relative=False).write(str(OUT/'render-assets'/(name+'.stl')))
        printed=s.copy()
        # Clamp top goes on bed; stepped PCB contact surface faces upward.
        if name.startswith('clamp_'): printed.rotate(V(0,0,0),V(1,0,0),180)
        b=printed.BoundBox;printed.translate(V(-b.XMin,-b.YMin,-b.ZMin))
        m=MeshPart.meshFromShape(Shape=printed,LinearDeflection=0.03,AngularDeflection=0.12,Relative=False)
        m.write(str(OUT/(name+'.stl')));Part.export([o],str(OUT/(name+'.step')))
        r=Mesh.Mesh(str(OUT/(name+'.stl')));step=Part.Shape();step.read(str(OUT/(name+'.step')))
        report['parts'][name]={'quantity':1,'brep_valid':s.isValid(),'solids':len(s.Solids),'step_valid':step.isValid(),'step_solids':len(step.Solids),'stl_closed':r.isSolid(),'non_manifold':r.hasNonManifolds(),'self_intersections':r.hasSelfIntersections(),'orientation_error':r.hasNonUniformOrientedFacets(),'degenerate_facets':sum(1 for f in r.Facets if f.Area<1e-10),'bounds_mm':[printed.BoundBox.XLength,printed.BoundBox.YLength,printed.BoundBox.ZLength],'volume_mm3':s.Volume,'stl_volume_mm3':r.Volume}
    for i,a in enumerate(parts):
        for b in list(parts)[i+1:]:
            v=parts[a].common(parts[b]).Volume
            if v>1e-6: report['printed_part_collisions'][a+'/'+b]=v
    pcb=box(25,25.2,12,30,37.8,1).cut(box(24.9,48,11,1.6,2,3))
    report['nominal_pcb_collisions']={n:s.common(pcb).Volume for n,s in parts.items() if s.common(pcb).Volume>1e-6}
    wrong=pcb.copy();wrong.rotate(V(40,44.1,12),V(0,0,1),180)
    report['reversed_180_nominal_interference_mm3']=parts['insert_r2b'].common(wrong).Volume
    o=doc.addObject('Part::Feature','REFERENCE_nominal_PCB');o.Shape=pcb
    MeshPart.meshFromShape(Shape=pcb,LinearDeflection=0.05,AngularDeflection=0.15,Relative=False).write(str(OUT/'render-assets'/'pcb.stl'))
    o.addProperty('App::PropertyString','Status');o.Status='Nominal substrate only, no component collision evidence'
    for k,val in {**P,**C}.items():
        o.addProperty('App::PropertyString','P_'+k);setattr(o,'P_'+k,json.dumps(val))
    from component_envelopes import build as envelope_build
    refs,sweeps,metal,envelope_note=envelope_build()
    report['connector_and_mate_collisions']={}
    for n,shape in {**refs,**sweeps}.items():
        for pn,ps in parts.items():
            v=shape.common(ps).Volume
            if v>1e-5: report['connector_and_mate_collisions'][n+'/'+pn]=v
    antenna=box(10,5,-10,60,35.2,70)
    report['metal_in_15mm_antenna_zone']={n:shape.common(antenna).Volume for n,shape in metal.items() if shape.common(antenna).Volume>1e-5}
    for n,shape in {**refs,**metal}.items():
        obj=doc.addObject('Part::Feature','REF_'+n);obj.Shape=shape
        MeshPart.meshFromShape(Shape=shape,LinearDeflection=0.05,AngularDeflection=0.15,Relative=False).write(str(OUT/'render-assets'/(n+'.stl')))
    from pcba_envelopes import build as pcba_build
    pcba,access,pcba_note=pcba_build()
    pcba_note['fixture_collisions']={};pcba_note['mate_collisions']={};pcba_note['access_collisions']={}
    for n,shape in pcba.items():
        for pn,ps in parts.items():
            vol=shape.common(ps).Volume
            if vol>1e-5:pcba_note['fixture_collisions'][n+'/'+pn]=vol
        for rn,rs in refs.items():
            if rn in ['USB4105_bound','FTSH_K_bound']:continue
            vol=shape.common(rs).Volume
            if vol>1e-5:pcba_note['mate_collisions'][n+'/'+rn]=vol
        obj=doc.addObject('Part::Feature',n);obj.Shape=shape
        MeshPart.meshFromShape(Shape=shape,LinearDeflection=0.05,AngularDeflection=0.15,Relative=False).write(str(OUT/'render-assets'/(n+'.stl')))
    for n,shape in access.items():
        for rn,rs in {**parts,**refs}.items():
            vol=shape.common(rs).Volume
            if vol>1e-5:pcba_note['access_collisions'][n+'/'+rn]=vol
    pcba_note['battery_wire_antenna_collisions']={n:sh.common(antenna).Volume for n,sh in pcba.items() if n.startswith('PCBA_J1_wire') and sh.common(antenna).Volume>1e-5}
    pcba_note['battery_wire_table_clearance_mm']={n:sh.BoundBox.ZMin-C['table_z_mm'] for n,sh in pcba.items() if n.startswith('PCBA_J1_wire')}
    report['pcba_screening_collisions']={k:pcba_note[k] for k in ['fixture_collisions','mate_collisions','access_collisions']}
    (OUT/'pcba-envelope-report.json').write_text(json.dumps(pcba_note,indent=2)+'\n',encoding='utf-8')
    envelope_note['collision_results']=report['connector_and_mate_collisions']
    (OUT/'component-envelope-report.json').write_text(json.dumps(envelope_note,indent=2)+'\n',encoding='utf-8')
    doc.recompute();doc.saveAs(str(OUT/'R2b-service-draft.FCStd'));Part.export([o for o in doc.Objects if hasattr(o,'Shape')],str(OUT/'assembly-draft.step'))
    report['mesh_and_nominal_geometry_pass']=all(v['brep_valid'] and v['step_valid'] and v['solids']==v['step_solids']==1 and v['stl_closed'] and not any(v[k] for k in ['non_manifold','self_intersections','orientation_error','degenerate_facets']) for v in report['parts'].values()) and not report['printed_part_collisions'] and not report['nominal_pcb_collisions'] and report['reversed_180_nominal_interference_mm3']>1.0 and not report['connector_and_mate_collisions'] and not report['metal_in_15mm_antenna_zone']
    (OUT/'geometry-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    A.closeDocument(doc.Name);print(json.dumps(report,indent=2))
    if not report['mesh_and_nominal_geometry_pass']: raise SystemExit('Geometry check failed')
if __name__=='__main__':main()


