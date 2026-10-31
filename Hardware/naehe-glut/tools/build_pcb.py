"""Generate the editable two-layer carrier from circuit.json using KiCad pcbnew.

This script does NOT declare a manufacturing release. KiCad DRC, the schematic
netlist comparison, CAM inspection and independent review are separate gates.
Use the Python bundled with the documented KiCad installation.
"""

import argparse
import heapq
import json
import math
from pathlib import Path
import shutil
import uuid
import xml.etree.ElementTree as ET

import pcbnew as pcb

BASE = Path(__file__).resolve().parents[1]
GRID = 0.2
CLEARANCE = 0.25


def mm(n):
    return pcb.FromMM(float(n))


def pos(x,y):
    return pcb.VECTOR2I(mm(x),mm(y))


def xy(p):
    return pcb.ToMM(p.x), pcb.ToMM(p.y)


def line(board, a,b,layer,width=0.15):
    item=pcb.PCB_SHAPE()
    item.SetShape(pcb.SHAPE_T_SEGMENT)
    item.SetStart(pos(*a));item.SetEnd(pos(*b))
    item.SetLayer(layer);item.SetWidth(mm(width));board.Add(item)


class Router:
    """Conservative two-layer grid router; final authority is KiCad's DRC."""
    def __init__(self,board,outline,pads):
        self.board=board
        self.width,self.height=outline
        self.nx=round(self.width/GRID)+1;self.ny=round(self.height/GRID)+1
        self.pads=pads
        self.copper=[]
        self.routes=[]
        self.vias=set()

    def point(self, x,y):
        return round(x/GRID),round(y/GRID)

    def block_disk(self,mask,x,y,r):
        gx,gy=self.point(x,y); gr=math.ceil(r/GRID)+1
        for yy in range(max(0,gy-gr),min(self.ny,gy+gr+1)):
            for xx in range(max(0,gx-gr),min(self.nx,gx+gr+1)):
                if (xx*GRID-x)**2+(yy*GRID-y)**2 <= r*r:
                    mask[yy*self.nx+xx]=1

    def block_rect(self,mask,x0,y0,x1,y1,expand):
        for yy in range(max(0,math.floor((y0-expand)/GRID)),min(self.ny,math.ceil((y1+expand)/GRID)+1)):
            for xx in range(max(0,math.floor((x0-expand)/GRID)),min(self.nx,math.ceil((x1+expand)/GRID)+1)):
                dx=max(x0-xx*GRID,0,xx*GRID-x1)
                dy=max(y0-yy*GRID,0,yy*GRID-y1)
                if dx*dx+dy*dy<=expand*expand:
                    mask[yy*self.nx+xx]=1

    def masks(self,net,width,via=False):
        radius=0.4 if via else width/2
        masks=[bytearray(self.nx*self.ny),bytearray(self.nx*self.ny)]
        for mask in masks:
            # Copper edge clearance 0.5 mm and milled antenna notch.
            edge=radius+0.5
            self.block_rect(mask,-10,-10,self.width+10,0,edge)
            self.block_rect(mask,-10,self.height,self.width+10,self.height+10,edge)
            self.block_rect(mask,-10,0,0,self.height,edge)
            self.block_rect(mask,self.width,0,self.width+10,self.height,edge)
            self.block_rect(mask,15.5,-10,35.5,15,edge)
            for x,y in [[5,5],[80,5],[5,75],[80,75]]:
                self.block_disk(mask,x,y,1.6+radius+0.5)
            # Conservative RF keepout. Connector GND pins are outside it.
            self.block_rect(mask,16,-10,35,16,radius+0.25)
        for pad in self.pads:
            if pad['net']==net:
                continue
            x,y=pad['xy'];sx,sy=pad['size']
            for layer in pad.get('layers',[0,1]):
                mask=masks[layer]
                self.block_rect(mask,x-sx/2,y-sy/2,x+sx/2,y+sy/2,radius+CLEARANCE+0.03)
        for other,layer,points,other_width in self.copper:
            if other==net:
                continue
            for x,y in points:
                self.block_disk(masks[layer],x,y,radius+other_width/2+CLEARANCE+0.03)
        return masks

    def route(self,net,padlist,width):
        # Start from the nearest pair and grow a single connected tree.
        padlist=list(padlist)
        first=padlist.pop(0)
        tree={(*self.point(*first['xy']),l) for l in first.get('layers',[0,1])}
        anchors=[first['xy']]
        while padlist:
            target=min(padlist,key=lambda p:min(math.dist(p['xy'],a) for a in anchors))
            padlist.remove(target)
            startx,starty=self.point(*target['xy'])
            masks=self.masks(net,width);via_masks=self.masks(net,width,True)
            goals=list(tree)
            xmin=min(g[0] for g in goals);xmax=max(g[0] for g in goals)
            ymin=min(g[1] for g in goals);ymax=max(g[1] for g in goals)
            # Multigoal heuristic toward the existing routed tree.
            def heuristic(x,y):
                return max(xmin-x,0,x-xmax)+max(ymin-y,0,y-ymax)
            queue=[];dist={};prev={}
            for layer in target.get('layers',[0,1]):
                s=(startx,starty,layer)
                dist[s]=0;heapq.heappush(queue,(heuristic(startx,starty),0,s))
            end=None
            while queue:
                _,g,state=heapq.heappop(queue)
                if dist.get(state)!=g:continue
                if state in tree:end=state;break
                x,y,l=state
                for dx,dy,cost in [(1,0,1),(-1,0,1),(0,1,1),(0,-1,1)]:
                    xx,yy=x+dx,y+dy
                    if not(0<=xx<self.nx and 0<=yy<self.ny):continue
                    if masks[l][yy*self.nx+xx]:continue
                    s=(xx,yy,l);ng=g+cost
                    if ng<dist.get(s,1e99):
                        dist[s]=ng;prev[s]=state;heapq.heappush(queue,(ng+heuristic(xx,yy),ng,s))
                i=y*self.nx+x
                if not via_masks[0][i] and not via_masks[1][i]:
                    s=(x,y,1-l);ng=g+12
                    if ng<dist.get(s,1e99):
                        dist[s]=ng;prev[s]=state;heapq.heappush(queue,(ng+heuristic(x,y),ng,s))
            if end is None:raise RuntimeError(f'No safe route: {net} to {target}')
            path=[end]
            while path[-1] in prev:path.append(prev[path[-1]])
            path.reverse()
            # Pad centers can lie between grid points: the stub stays in pad.
            self.track(net,target['xy'],(startx*GRID,starty*GRID),path[0][2],width)
            run=[];current_layer=path[0][2]
            for i,state in enumerate(path):
                x,y,l=state
                if l!=current_layer:
                    self.copper.append((net,current_layer,run,width));run=[]
                    if (x,y) not in self.vias:
                        v=pcb.PCB_VIA(self.board);v.SetPosition(pos(x*GRID,y*GRID))
                        v.SetWidth(mm(0.8));v.SetDrill(mm(0.4));v.SetViaType(pcb.VIATYPE_THROUGH)
                        v.SetLayerPair(pcb.F_Cu,pcb.B_Cu);v.SetNetCode(net);self.board.Add(v)
                        self.vias.add((x,y))
                    for vl in [0,1]:self.copper.append((net,vl,[(x*GRID,y*GRID)],0.8))
                    current_layer=l
                run.append((x*GRID,y*GRID))
                if i and path[i-1][2]==l:
                    ax,ay,_=path[i-1]
                    self.track(net,(ax*GRID,ay*GRID),(x*GRID,y*GRID),l,width)
            self.copper.append((net,current_layer,run,width))
            tree.update(path)
            anchors.append(target['xy'])
            # The initial source pad also needs its exact-center stub.
            for a in anchors:
                gx,gy=self.point(*a)
                for l in [0,1]:
                    if (gx,gy,l) in tree:self.track(net,a,(gx*GRID,gy*GRID),l,width)
            self.routes.append({'net_code':net,'target':target['ref'],'grid_steps':len(path)})

    def track(self,net,a,b,layer,width):
        if math.dist(a,b)<1e-8:return
        t=pcb.PCB_TRACK(self.board);t.SetStart(pos(*a));t.SetEnd(pos(*b))
        t.SetWidth(mm(width));t.SetLayer(pcb.F_Cu if layer==0 else pcb.B_Cu)
        t.SetNetCode(net);self.board.Add(t)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--footprints',required=True,type=Path)
    args=parser.parse_args()
    data=json.loads((BASE/'pcb/circuit.json').read_text(encoding='utf-8'))
    board=pcb.BOARD();board.SetCopperLayerCount(2)
    board.GetDesignSettings().SetBoardThickness(mm(1.6))
    netnames=sorted({net for part in data['parts'] for net in part['pins'].values()})
    nets={}
    for name in netnames:
        net=pcb.NETINFO_ITEM(board,'/'+name);board.Add(net);nets[name]=net
    nc_nets={}
    exported=BASE/'pcb/exports/netlist.xml'
    if exported.exists():
        for element in ET.parse(exported).findall('./nets/net'):
            if element.attrib['name'].startswith('unconnected-'):
                net=pcb.NETINFO_ITEM(board,element.attrib['name']);board.Add(net)
                for node in element.findall('node'):nc_nets[(node.attrib['ref'],node.attrib['pin'])]=net
    pads=[]
    ownlib=BASE/'pcb/NearGlow.pretty';ownlib.mkdir(exist_ok=True)
    for part in data['parts']:
        lib,name=part['footprint'].split(':')
        source=(ownlib if lib=='NearGlow' else args.footprints/(lib+'.pretty'))/(name+'.kicad_mod')
        if not source.exists():raise FileNotFoundError(source)
        if source.resolve()!=(ownlib/source.name).resolve():shutil.copyfile(source,ownlib/source.name)
        if part['ref']=='F1':
            file=ownlib/source.name
            text=file.read_text(encoding='utf-8').replace('${KICAD10_3DMODEL_DIR}/Fuse.3dshapes/Fuse_Littelfuse_372_D8.50mm.step','${KIPRJMOD}/models/Fuse_Littelfuse_372_envelope.step')
            file.write_text(text,encoding='utf-8')
        footprint=pcb.FootprintLoad(str(ownlib),name)
        footprint.SetFPID(pcb.LIB_ID(lib,name))
        if part['ref'] in ['J1','J3']:
            for item in list(footprint.GraphicalItems()):
                if item.GetLayer()==pcb.F_SilkS:item.SetLayer(pcb.F_Fab)
            pcb.FootprintSave(str(ownlib),footprint)
        footprint.SetReference(part['ref']);footprint.SetValue(part['value'])
        footprint.SetPosition(pos(*part['xy']))
        footprint.SetOrientationDegrees(part.get('angle',0))
        from build_schematic import uid
        path=pcb.KIID_PATH();path.push_back(pcb.KIID(uid('root')));path.push_back(pcb.KIID(uid(part['ref'])))
        footprint.SetPath(path)
        footprint.SetSheetfile('naehe-glut-r1.kicad_sch');footprint.SetSheetname('')
        footprint.Reference().SetVisible(False)
        footprint.Value().SetVisible(False)
        board.Add(footprint)
        for pad in footprint.Pads():
            number=pad.GetNumber();netname=part['pins'].get(number)
            if netname:pad.SetNet(nets[netname])
            elif (part['ref'],number) in nc_nets:pad.SetNet(nc_nets[(part['ref'],number)])
            pads.append({'ref':part['ref']+'.'+number,'net':nets[netname].GetNetCode() if netname else 0,'xy':xy(pad.GetPosition()),'size':xy(pad.GetSize()),'layers':[0] if pad.GetAttribute()==pcb.PAD_ATTRIB_SMD else [0,1]})
    outline=[(0,0),(15.5,0),(15.5,15),(35.5,15),(35.5,0),(85,0),(85,80),(0,80),(0,0)]
    for a,b in zip(outline,outline[1:]):line(board,a,b,pcb.Edge_Cuts,0.05)
    for i,(x,y) in enumerate(data['mounts_mm']):
        fp=pcb.FOOTPRINT(board);fp.SetReference('H'+str(i+1));fp.SetValue('M3 NPTH')
        fp.SetAttributes(pcb.FP_BOARD_ONLY|pcb.FP_EXCLUDE_FROM_BOM|pcb.FP_EXCLUDE_FROM_POS_FILES)
        fp.SetPosition(pos(x,y));board.Add(fp)
        pad=pcb.PAD(fp);pad.SetAttribute(pcb.PAD_ATTRIB_NPTH);pad.SetShape(pcb.PAD_SHAPE_CIRCLE)
        pad.SetSize(pos(3.2,3.2));pad.SetDrillSize(pos(3.2,3.2));pad.SetPosition(pos(x,y));pad.SetLayerSet(pcb.LSET.AllCuMask());fp.Add(pad)
    for text,x,y,size in [('NAEHE GLUT R1',49,5,1.3),('ESP USB POWER ONLY',25,65,1.0),('NO ESP 5V LINK',25,69,1.0),('+5V DATA GND',69,17,0.9),('LED POWER',65,78,1.0),('ANTENNA',25.5,18,0.8)]:
        t=pcb.PCB_TEXT(board);t.SetText(text);t.SetPosition(pos(x,y));t.SetTextSize(pos(size,size));t.SetTextThickness(mm(.15));t.SetLayer(pcb.F_SilkS);board.Add(t)
    router=Router(board,data['board_mm'],pads)
    routed_pads=list(pads)
    # USB-C fine-pitch fanout: short 0.4 mm necks, then 1.2 mm power rails.
    fanouts={'A5':(63,65),'B5':(65.4,64),'A9':(66.6,69),'B9':(63.4,69),'A12':(67.8,71),'B12':(62.2,71)}
    for number,end in fanouts.items():
        pad=next(p for p in pads if p['ref']=='J2.'+number)
        a=pad['xy'];b=(a[0],72.0);c=(end[0],71.8)
        points=[a,b,c,end]
        if number=='A5':points=[a,b,(64.6,71.8),(64.6,67),(63,67),end]
        for start,finish in zip(points,points[1:]):
            router.track(pad['net'],start,finish,0,.4)
            count=max(1,math.ceil(math.dist(start,finish)/.1))
            sampled=[(start[0]+(finish[0]-start[0])*i/count,start[1]+(finish[1]-start[1])*i/count) for i in range(count+1)]
            router.copper.append((pad['net'],0,sampled,.4))
        v=pcb.PCB_VIA(board);v.SetPosition(pos(*end));v.SetWidth(mm(.8));v.SetDrill(mm(.4));v.SetViaType(pcb.VIATYPE_THROUGH);v.SetLayerPair(pcb.F_Cu,pcb.B_Cu);v.SetNetCode(pad['net']);board.Add(v)
        router.vias.add(router.point(*end))
        for layer in [0,1]:router.copper.append((pad['net'],layer,[end],.8))
        routed_pads.remove(pad)
        routed_pads.append({'ref':pad['ref']+' fanout','net':pad['net'],'xy':end,'size':(.8,.8),'layers':[0,1]})
    # Route high current rails first, then short/low-pin-count signals.
    order=['USB5V','LED5V','GND']+[n for n in netnames if n not in ['USB5V','LED5V','GND']]
    for name in order:
        net=nets[name];members=[p for p in routed_pads if p['net']==net.GetNetCode()]
        if len(members)>1:
            print('Routing',name,len(members),flush=True)
            router.route(net.GetNetCode(),members,1.2 if name in ['USB5V','LED5V','GND'] else .3)
    pcb.SaveBoard(str(BASE/'pcb/naehe-glut-r1.kicad_pcb'),board)
    project={"meta":{"filename":"naehe-glut-r1.kicad_pro","version":3},"board":{"design_settings":{"rules":{"min_clearance":0.25,"min_track_width":0.25,"min_via_diameter":0.6,"min_through_hole_diameter":0.3,"min_copper_edge_clearance":0.5,"min_hole_clearance":0.25,"min_hole_to_hole":0.25}}}}
    (BASE/'pcb/naehe-glut-r1.kicad_pro').write_text(json.dumps(project,indent=2)+'\n',encoding='utf-8')
    (BASE/'pcb/routing-report.json').write_text(json.dumps({'grid_mm':GRID,'clearance_mm':CLEARANCE,'routes':router.routes},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
