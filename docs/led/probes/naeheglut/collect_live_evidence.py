"""Reproduce selected camera evidence; no device access and no effect implementation.

python collect_live_evidence.py --raw PRIVATE_SESSION --output NEW_EVIDENCE_DIR
Needs the pinned camera packages. Raw PNGs remain private. Crops have no colour
correction; ramp details use nearest-neighbour enlargement. Times are host receipt
times, never exposure timestamps. The 23 regions are not firmware-index calibrated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import cv2
import numpy as np

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def stats(values):
    return dict(min=min(values), median=statistics.median(values), max=max(values)) if values else None

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--raw', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    trials={}
    evidence={'schema':1,'scope':'Visual feedback and digital trace; no calibrated optical acceptance',
              'analysis_source_sha256':sha(Path(__file__)),'runs':{},'selected_frames':[]}
    for name in ('v1','v2','v3'):
        folder=args.raw/name
        capture=json.loads((folder/'capture.json').read_text())
        camera=json.loads((folder/'camera.json').read_text())
        trials[name]=(folder,capture,camera)
        all_frames=capture['events']
        assert all(len(e['pixels'])==23 and all(p==e['pixels'][0] for p in e['pixels']) for e in all_frames)
        assert [e['sequence'] for e in all_frames]==list(range(len(all_frames)))
        commands=capture['commands']
        evidence['runs'][name]={
            'status':capture['status'],'error':capture.get('error'),
            'source_sha256':{f:sha(folder/f) for f in ('capture.json','camera.json','session.jsonl')},
            'firmware_sha256':capture['firmware_sha256'],'host_source_sha256':capture['host_source_sha256'],
            'frame_count':len(camera['frames']),'digital_frame_count':len(all_frames),
            'all_23_pixels_uniform':True,'trace_sequence_contiguous':True,
            'channel_max':max(max(e['pixels'][0]) for e in all_frames),
            'camera_settings':camera['settings'],'camera_timing':camera.get('timing'),
            'writer_drops':camera['dropped_for_writer_backlog'],
            'preview_warnings':camera.get('preview_warning_count'),
            'final_black_confirmed':capture.get('final_black_confirmed'),
            'stop_to_done_ms':capture['stop_done_host_ms']-capture['stop_sent_ms'],
            'stop_to_done_receive_ms':next(s['host_ms'] for s in capture['serial'] if s['host_ms']>=capture['stop_sent_ms'] and s['line']==f"DONE {capture['token']}")-capture['stop_sent_ms'],
            'latency_ms':{k:stats([c[k] for c in commands if k in c]) for k in
                ('host_to_ack_ms','applied_to_submitted_ms','applied_to_completed_ms','host_to_tx_event_ms')},
            'commands':[{k:v for k,v in c.items() if k not in ('wire',)} for c in commands]}

    def frame(name,req,offset=1800,absolute=None):
        folder,capture,camera=trials[name]
        command=next((c for c in capture['commands'] if c['req']==req),None)
        aim=absolute if absolute is not None else command['sent_ms']+offset
        entry=min(camera['frames'],key=lambda f:abs(f['t_ms']-aim))
        path=folder/entry['file']
        assert sha(path)==entry['sha256']
        evidence['selected_frames'].append(dict(run=name,request=req,file=entry['file'],
              t_ms=entry['t_ms'],sha256=entry['sha256']))
        return cv2.imread(str(path)),entry,command

    def text(image,value,x,y,size=.52):
        cv2.putText(image,value,(x,y),cv2.FONT_HERSHEY_SIMPLEX,size,(235,235,235),1,cv2.LINE_AA)

    colour=np.full((770,1020,3),22,np.uint8)
    variants=[('v1',1,'A: RGB 24,12,2 / exposure -7'),('v2',1,'B: same RGB / exposure -10'),
              ('v2',2,'C: RGB 24,8,1 / exposure -10'),('v3',2,'D: RGB 24,8,0 / exposure -10')]
    for i,(name,req,label) in enumerate(variants):
        im,e,_=frame(name,req)
        x=(i%2)*510;y=(i//2)*385
        text(colour,label,x+10,y+24)
        colour[y+35:y+355,x+10:x+500]=cv2.resize(im[140:620,250:920],(490,320),interpolation=cv2.INTER_AREA)
        text(colour,f"{name} / {e['file']} / host {e['t_ms']/1000:.3f}s",x+10,y+375,.42)
    cv2.imwrite(str(args.output/'colour-iterations.png'),colour)

    ramps=np.full((650,1120,3),22,np.uint8)
    for row,(name,req,label) in enumerate((('v2',4,'Linear / RGB 24,8,1'),('v2',6,'Smoothstep / RGB 24,8,1'),('v3',4,'Smoothstep / RGB 24,8,0'))):
        text(ramps,label+' / 1000 ms',10,row*210+22,.6)
        for col,dt in enumerate((0,200,400,600,800,1000,1200)):
            im,e,c=frame(name,req,dt)
            ramps[row*210+35:row*210+195,col*160:(col+1)*160]=cv2.resize(im[161:201,565:605],(160,160),interpolation=cv2.INTER_NEAREST)
            text(ramps,str(round(e['t_ms']-c['sent_ms']))+' ms',col*160+5,row*210+207,.42)
    text(ramps,'One fixed light area; time since host command, not calibrated optical latency.',10,646,.43)
    cv2.imwrite(str(args.output/'ramp-comparison.png'),ramps)

    phases=np.full((610,1020,3),22,np.uint8)
    cases=[(4,1500,'Gold at full level'),(5,1500,'Invalid: dark'),(7,1500,'Valid again: half level'),
           (9,500,'Gold + blue + background'),(10,1600,'Gold stopped: blue + background'),(None,0,'Global stop: dark')]
    for i,(req,offset,label) in enumerate(cases):
        end=trials['v3'][1]['stop_sent_ms']+350 if req is None else None
        im,e,_=frame('v3',req,offset,absolute=end)
        x=(i%3)*340;y=(i//3)*305
        text(phases,label,x+7,y+22,.45)
        phases[y+35:y+275,x+5:x+335]=cv2.resize(im[140:620,250:920],(330,240),interpolation=cv2.INTER_AREA)
        text(phases,f"{e['file']} / {e['t_ms']/1000:.3f}s",x+7,y+296,.37)
    cv2.imwrite(str(args.output/'lifecycle.png'),phases)
    for f in ('backup.json','install.json','restore.json','source-manifest.json'):
        path=args.raw/f
        if path.exists(): evidence[f]=json.loads(path.read_text(encoding='utf-8-sig'))
    evidence['output_sha256']={p.name:sha(p) for p in args.output.glob('*.png')}
    (args.output/'summary.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'runs':{k:v['status'] for k,v in evidence['runs'].items()},'selected_frames':len(evidence['selected_frames'])}))

if __name__=='__main__': main()
