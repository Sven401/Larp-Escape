"""Read-only KiCad placement extraction; run with KiCad bundled Python."""
from pathlib import Path
import json,hashlib,math
import pcbnew as p
BASE=Path(__file__).resolve().parents[1]
def main():
    path=BASE/'naehe-glut-r2b.kicad_pcb';before=path.read_bytes();board=p.LoadBoard(str(path));rows=[]
    for fp in board.GetFootprints():
        shapes=[]
        for g in fp.GraphicalItems():
            if isinstance(g,p.PCB_SHAPE) and g.GetLayer() in [p.F_Fab,p.B_Fab]:
                b=g.GetBoundingBox();shapes.append([p.ToMM(b.GetX()),p.ToMM(b.GetY()),p.ToMM(b.GetRight()),p.ToMM(b.GetBottom())])
        q=fp.GetPosition();row={'ref':fp.GetReference(),'value':fp.GetValue(),'footprint':str(fp.GetFPID().GetLibItemName()),'position_mm':[p.ToMM(q.x),p.ToMM(q.y)],'rotation_deg':fp.GetOrientationDegrees(),'side':'bottom' if fp.GetLayer()==p.B_Cu else 'top'}
        if shapes:row['fab_aabb_mm']=[min(b[0] for b in shapes),min(b[1] for b in shapes),max(b[2] for b in shapes),max(b[3] for b in shapes)]
        row['pads']=[{'number':a.GetNumber(),'position_mm':[p.ToMM(a.GetPosition().x),p.ToMM(a.GetPosition().y)]} for a in fp.Pads()]
        rows.append(row)
    assert path.read_bytes()==before
    out={'pcb_sha256':hashlib.sha256(before).hexdigest(),'read_only':True,'kicad':p.GetBuildVersion(),'footprints':sorted(rows,key=lambda x:x['ref'])}
    (BASE/'mechanik/pcb-placement.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
    print('Extracted',len(rows),'footprints without modifying PCB')
if __name__=='__main__':main()
