"""Conservative manufacturer-dimension envelopes for the PCB STEP assembly.

These are mechanical clearance envelopes, not detailed vendor models.
Run with FreeCAD's bundled Python. No GUI required.
"""
from pathlib import Path
import FreeCAD
import Part

destination=Path(__file__).resolve().parents[1]/'pcb/models'
destination.mkdir(exist_ok=True)
# KEMET C320 body maximum 5.08 x 3.18; seated maximum 7.36 incl standoff.
cap=Part.makeBox(5.08,3.18,7.36,FreeCAD.Vector(-1.27,-1.59,0))
cap.exportStep(str(destination/'C_Kemet_C320_envelope.step'))
# Littelfuse372 cylinder D8.5, body8; 1mm seating/lead allowance.
fuse=Part.makeCylinder(4.25,9,FreeCAD.Vector(2.54,0,0))
fuse.exportStep(str(destination/'Fuse_Littelfuse_372_envelope.step'))
print('Two valid envelopes:',cap.isValid(),fuse.isValid())
