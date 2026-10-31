"""Cross-check exported schematic nets against PCB pads and copper resistance.

The resistance calculation is a conservative single-path DC calculation at
20 C, NOT a thermal qualification or a measurement of assembled hardware.
"""
import heapq
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET
import pcbnew as pcb

BASE=Path(__file__).resolve().parents[1]
board=pcb.LoadBoard(str(BASE/'pcb/naehe-glut-r1.kicad_pcb'))
xml=ET.parse(BASE/'pcb/exports/netlist.xml')
expected={}
for net in xml.findall('./nets/net'):
    for n in net.findall('node'):
        if not n.attrib['ref'].startswith('#'):
            expected[(n.attrib['ref'],n.attrib['pin'])]=net.attrib['name']
actual={};pads={}
for fp in board.GetFootprints():
    if fp.GetReference().startswith('H'):continue
    for pad in fp.Pads():
        key=(fp.GetReference(),pad.GetNumber())
        actual[key]=pad.GetNetname()
        pads.setdefault(key,[]).append(pad)
assert actual==expected,{'missing':list(expected.keys()-actual.keys()),'extra':list(actual.keys()-expected.keys()),'different':[(k,expected[k],actual.get(k)) for k in expected if expected[k]!=actual.get(k)]}
assert all(actual[('J1',n)].startswith('unconnected-') for n in ['1','2','21'])
assert actual[('J1','8')]=='/GPIO15' and actual[('J1','9')]=='/GPIO16'

def point(p,layer):return (round(p.x/100)*100,round(p.y/100)*100,layer)
def distance(a,b):return math.hypot(a[0]-b[0],a[1]-b[1])/1e6

def path_resistance(net,start,end):
    graph={}
    def edge(a,b,r):
        graph.setdefault(a,[]).append((b,r));graph.setdefault(b,[]).append((a,r))
    def padnodes(key):
        out=[]
        for pad in pads[key]:
            layers=[pcb.F_Cu] if pad.GetAttribute()==pcb.PAD_ATTRIB_SMD else [pcb.F_Cu,pcb.B_Cu]
            out.extend(point(pad.GetPosition(),l) for l in layers)
        return out
    for tr in board.GetTracks():
        if tr.GetNetname()!=net:continue
        if isinstance(tr,pcb.PCB_VIA):
            a=point(tr.GetPosition(),pcb.F_Cu);b=point(tr.GetPosition(),pcb.B_Cu)
            # Minimum specified plated wall 20um, nominal1.6mm board.
            area=math.pi*pcb.ToMM(tr.GetDrillValue())*.020
            edge(a,b,.0175*.0016/area)
        else:
            a=point(tr.GetStart(),tr.GetLayer());b=point(tr.GetEnd(),tr.GetLayer())
            edge(a,b,.0175*(distance(a,b)/1000)/(pcb.ToMM(tr.GetWidth())*.035))
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetname()!=net:continue
            centers=padnodes((fp.GetReference(),pad.GetNumber()))
            for a in centers:
                for b in centers:
                    if a!=b:edge(a,b,.0012)
                # Electrically continuous copper inside each pad. Small pad
                # spreading resistance deliberately added rather than omitted.
                for b in list(graph):
                    if a[2]==b[2] and pad.HitTest(pcb.VECTOR2I(b[0],b[1])):
                        edge(a,b,.0002 if a!=b else 0)
    queue=[];seen={}
    targets=set(padnodes(end))
    for node in padnodes(start):heapq.heappush(queue,(0,node));seen[node]=0
    while queue:
        r,node=heapq.heappop(queue)
        if r!=seen[node]:continue
        if node in targets:return r
        for nxt,delta in graph.get(node,[]):
            nr=r+delta
            if nr<seen.get(nxt,1e9):seen[nxt]=nr;heapq.heappush(queue,(nr,nxt))
    raise AssertionError('No copper path '+net)

resistances={
 'usb_to_fuse':path_resistance('/USB5V',('J2','B9'),('F1','1')),
 'fuse_to_led':path_resistance('/LED5V',('F1','2'),('J4','1')),
 'led_ground_to_usb':path_resistance('/GND',('J4','3'),('J2','B12')),
}
total=sum(resistances.values())
report={'netlist_pad_pairs':len(expected),'native_netlist_matches_pcb':True,
        'esp_power_pins_isolated':True,'copper_path_resistance_ohm_at_20C':resistances,
        'single_path_total_ohm':total,'copper_drop_V_at_1_5A':1.5*total,
        'copper_drop_V_at_2A':2*total,'copper_loss_W_at_2A':4*total,
        'assumptions':'35um copper,20um via wall,1.6mm board; single paths carrying full current; excludes fuse,connector,cable; no thermal/device proof'}
(BASE/'pcb/pruefung/netlist-and-power.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
