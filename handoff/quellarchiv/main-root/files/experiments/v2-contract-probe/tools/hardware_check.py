"""Local lab evidence; never record Bluetooth addresses or whole phone logs."""
import argparse, pathlib, subprocess, time, re, sys, xml.etree.ElementTree as ET
sys.stdout.reconfigure(encoding='utf-8')
ADB = r'C:/Program Files (x86)/Android/android-sdk/platform-tools/adb.exe'
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence'
OUT.mkdir(exist_ok=True)
def adb(*args):
    return subprocess.run([ADB,*args], check=True, capture_output=True).stdout
p=argparse.ArgumentParser();p.add_argument('action',choices=['serial','ui','tap','logs','screen']);p.add_argument('value',nargs='?',default='');p.add_argument('--file',default='esp-v2.txt');a=p.parse_args()
if pathlib.Path(a.file).name!=a.file:raise RuntimeError('Evidence filename must be local')
if a.action=='serial':
    import serial
    port=serial.Serial(port=None,baudrate=115200,timeout=.2);port.dtr=False;port.rts=False;port.port='COM3'
    captured=bytearray()
    try:
        port.open();end=time.monotonic()+float(a.value or 35)
        while time.monotonic()<end:
            captured.extend(port.read(min(4096,port.in_waiting or 1)))
            if len(captured)>131072:raise RuntimeError('Serial capture limit')
    finally:
        port.close();text=captured.decode('utf-8','replace');text=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[address omitted]',text)
        (OUT/a.file).write_text(text,encoding='utf-8');print(text)
elif a.action in ('ui','tap','screen'):
    name=f'/sdcard/larp-contract-{time.time_ns()}.xml'
    dumped=adb('shell','uiautomator','dump',name).decode('utf-8','replace')
    if 'dumped to' not in dumped:raise RuntimeError('Fresh UI dump failed')
    try:xml=adb('shell','cat',name)
    finally:adb('shell','rm',name)
    root=ET.fromstring(xml);nodes=list(root.iter('node'))
    if not any(n.get('package')=='de.larpescape.hardwarediagnostic' for n in nodes):raise RuntimeError('Lab app not foreground')
    if a.action=='screen':(OUT/'handy-v2.png').write_bytes(adb('exec-out','screencap','-p'))
    elif a.action=='tap':
        found=[n for n in nodes if a.value in n.get('text','') and n.get('clickable')=='true']
        if len(found)!=1:raise RuntimeError(f'Expected one button, found {len(found)}')
        coords=list(map(int,re.findall(r'\d+',found[0].get('bounds'))));adb('shell','input','tap',str((coords[0]+coords[2])//2),str((coords[1]+coords[3])//2))
        print('Tapped:',found[0].get('text'))
    else:
        text='\n'.join(n.get('text','') for n in nodes if n.get('text'));(OUT/'handy-ui.txt').write_text(text,encoding='utf-8');print(text)
elif a.action=='logs':
    text=adb('logcat','-d','-v','brief','LarpEspDiagnostic:I','*:S').decode('utf-8','replace');text=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[address omitted]',text)
    (OUT/'handy-v2.txt').write_text(text,encoding='utf-8');print(text)
