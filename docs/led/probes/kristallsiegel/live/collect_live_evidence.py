"""Hash-checked qualitative camera excerpts, not optical acceptance or calibration.

Usage: python collect_live_evidence.py --raw PRIVATE_RUN --output EVIDENCE_DIR
Needs the existing LED camera Python environment (OpenCV 4.13, NumPy).
All frame times are host receive times. PNG pixels are cropped/resized only;
the WebP preview is lossy and resampled. Original images remain private.
"""
import argparse,hashlib,json
from pathlib import Path
import cv2,numpy as np

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--raw',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    data={}; evidence={'schema':1,'scope':'Qualitative camera excerpts, not optical acceptance',
        'source_commit':'dc8c5d2358ffc4d5753b21c8806261046b4788d3',
        'collector_sha256':sha(Path(__file__)),'runs':{},'selected_frames':[]}
    for name in ('v1','v1b','v2','v3'):
        path=args.raw/name
        c=json.loads((path/'capture.json').read_text());k=json.loads((path/'camera.json').read_text())
        data[name]=(path,c,k)
        evidence['runs'][name]={'capture_sha256':sha(path/'capture.json'),
            'camera_sha256':sha(path/'camera.json'),'session_sha256':sha(path/'session.jsonl'),
            'status':c['status'],'error':c.get('error'),'firmware_sha256':c['firmware_sha256'],
            'host_source_sha256':c['host_source_sha256'],'camera_timing':k['timing'],
            'camera_settings':k['settings'],'frames':len(k['frames']),
            'transfer_events':len(c['events']),'writer_drops':k['dropped_for_writer_backlog'],
            'preview_warning_count':k['preview_warning_count'],
            'final_black_confirmed':c['final_black_confirmed']}
    def frame(name,req,dt):
        path,c,k=data[name]
        command=next(x for x in c['commands'] if x['req']==req)
        f=min(k['frames'],key=lambda f:abs(f['t_ms']-command['sent_ms']-dt))
        assert sha(path/f['file'])==f['sha256']
        evidence['selected_frames'].append({'run':name,'req':req,'requested_offset_ms':dt,
            'actual_offset_ms':f['t_ms']-command['sent_ms'],'file':f['file'],'sha256':f['sha256']})
        return cv2.imread(str(path/f['file'])),f,command
    def label(im,text,x,y,size=.55):
        cv2.putText(im,text,(x,y),cv2.FONT_HERSHEY_SIMPLEX,size,(235,235,235),1,cv2.LINE_AA)
    progress=np.full((380,1080,3),18,np.uint8)
    for i,(req,title) in enumerate(((2,'1 / 3'),(3,'2 / 3'),(4,'3 / 3 - nach dem Puls'))):
        im,f,c=frame('v1b',req,1800)
        progress[40:310,i*360:i*360+360]=cv2.resize(im[110:600,240:920],(360,270),interpolation=cv2.INTER_AREA)
        label(progress,title,i*360+10,27)
        label(progress,f"{f['file']} / +{round(f['t_ms']-c['sent_ms'])} ms",i*360+8,337,.40)
    label(progress,'Echte Kameraausschnitte / RGB 0,8,12 / Belichtung -10 / keine Farbkorrektur',10,368,.48)
    cv2.imwrite(str(args.output/'progress-states.png'),progress)
    mapping=json.loads((args.raw/'index-map.json').read_text())
    def tile_row(name,req,dt):
        im,f,c=frame(name,req,dt)
        assert im.shape[:2]==(400,400)
        row=np.zeros((36,23*36,3),np.uint8)
        for led in mapping['leds']:
            x,y=led['x'],led['y'];crop=im[y-12:y+12,x-12:x+12]
            row[:,led['index']*36:(led['index']+1)*36]=cv2.resize(crop,(36,36),interpolation=cv2.INTER_NEAREST)
        return row,round(f['t_ms']-c['sent_ms'])
    comparison=np.full((610,1020,3),18,np.uint8)
    label(comparison,'Erster Abschnitt: echte LED-Bildausschnitte in Indexreihenfolge',12,25)
    for i in range(23):label(comparison,str(i),180+i*36,51,.35)
    for group,(name,title) in enumerate((('v3','600 / 150 ms'),('v2','900 / 250 ms'))):
        label(comparison,title,12,85+group*255,.5)
        for row,dt in enumerate((100,300,500,700,1000)):
            strip,actual=tile_row(name,2,dt);y=63+group*255+row*45
            comparison[y:y+36,180:1008]=strip
            label(comparison,f'+{actual} ms',40,y+34,.43)
    label(comparison,'Zeit seit Hostbefehl, kein Belichtungszeitpunkt; keine Helligkeits-/Farbkalibrierung.',12,600,.45)
    cv2.imwrite(str(args.output/'speed-comparison.png'),comparison)
    mix=np.full((425,1020,3),18,np.uint8)
    label(mix,'Gezielter Stop: echte LED-Ausschnitte / parallele Beitraege bleiben sichtbar',12,25,.53)
    for row,(req,title) in enumerate(((7,'Siegel + Hintergrund + 2 Beitraege'),(8,'Siegel aus: RGB 7,2,3'),(9,'Warm aus: RGB 1,0,3'),(10,'Blau aus: RGB 1,0,0'),(11,'Hintergrund aus: RGB 0,0,0'))):
        strip,actual=tile_row('v3',req,1000);y=65+row*68
        label(mix,title+' / +'+str(actual)+' ms',12,y-10,.45)
        mix[y:y+36,180:1008]=strip
    label(mix,'RGB-Angaben sind gesendete Kanalcodes; die Bilder zeigen die Kameraantwort.',12,417,.45)
    cv2.imwrite(str(args.output/'mix-and-stop.png'),mix)
    path,c,k=data['v1b'];first=next(x for x in c['commands'] if x['req']==2)['sent_ms']-300
    last=next(x for x in c['commands'] if x['req']==4)['sent_ms']+2200
    preview=[]
    for t in np.arange(first,last,100):
        f=min(k['frames'],key=lambda f:abs(f['t_ms']-t));p=path/f['file'];assert sha(p)==f['sha256']
        im=cv2.resize(cv2.imread(str(p))[110:600,240:920],(680,490),interpolation=cv2.INTER_AREA)
        canvas=np.full((540,680,3),18,np.uint8);canvas[:490]=im
        label(canvas,'Kameraauszug: 600 ms Aufbau / reale unregelmaessige Anordnung',10,511,.46)
        label(canvas,'100-ms-Vorschau; Aufnahme mit Bildluecken, kein Zeitmessbeleg',10,532,.43)
        preview.append(canvas)
    animation=cv2.Animation();animation.frames=preview;animation.durations=[100]*len(preview);animation.loop_count=0
    assert cv2.imwriteanimation(str(args.output/'camera-preview.webp'),animation,[cv2.IMWRITE_WEBP_QUALITY,85])
    ok,check=cv2.imreadanimation(str(args.output/'camera-preview.webp'))
    assert ok and check.frames and sum(check.durations)==100*len(preview)
    assert all(f.shape[:2]==(540,680) for f in check.frames)
    evidence['preview']={'run':'v1b','first_host_ms':first,'last_host_ms':last,'interval_ms':100,
        'warning':'Nearest frames reused across gaps; qualitative appearance only'}
    evidence['index_map']={'mapping_status':mapping['mapping_status'],'radiometry_status':mapping['radiometry_status'],
        'map_sha256':sha(args.raw/'index-map.json'),'camera_map_sha256':sha(args.raw/'index-map-camera.json'),
        'calibration_capture_sha256':mapping['capture_sha256'],'positions':len(mapping['leds'])}
    (args.output/'evidence.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
    print(json.dumps({n:{'status':d['status'],'frames':d['frames'],'timing':d['camera_timing']} for n,d in evidence['runs'].items()},indent=2))

if __name__=='__main__':main()
