"""Render documented RSSI laboratory evidence; fixed 0.5 s bins, no time fitting."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

def compact(raw):
    out={'schema':1,'clock':raw['clock'],'limits':raw['limits'],
         'display_reduction':'TX and camera arithmetic means in fixed 0.5 s host-time bins; no time shift', 'runs':{}}
    for name in ('walk02','outage01'):
        source=raw['runs'][name]
        target={k:v for k,v in source.items() if k not in ('tx','camera')}
        for kind in ('tx','camera'):
            bins={}
            for row in source[kind]:bins.setdefault(math.floor(row['time_s']*2),[]).append(row)
            reduced=[]
            for bucket,rows in sorted(bins.items()):
                item={'time_s':bucket/2+.25,'count':len(rows)}
                if kind=='tx':
                    item['rgb']=[statistics.mean(r['rgb'][ch] for r in rows) for ch in range(3)]
                else:
                    values=[r['mean_delta_RGB'] for r in rows]
                    item.update(mean=statistics.mean(values),minimum=min(values),maximum=max(values))
                reduced.append(item)
            target[kind]=reduced
        out['runs'][name]=target
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--raw',type=Path)
    p.add_argument('--data',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--deps',type=Path)
    a=p.parse_args()
    if a.raw:
        data=compact(json.loads(a.raw.read_text(encoding='utf-8')))
        data['source_plot_data_sha256']=hashlib.sha256(a.raw.read_bytes()).hexdigest()
        with a.data.open('x',encoding='utf-8') as f:json.dump(data,f,ensure_ascii=False,separators=(',',':'))
    else:data=json.loads(a.data.read_text(encoding='utf-8'))
    if a.deps:sys.path.insert(0,str(a.deps))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(4,2,figsize=(12,10),sharex='col')
    fig.subplots_adjust(left=.10,right=.98,bottom=.12,top=.88,hspace=.16,wspace=.22)
    fig.suptitle('Näheglut mit echtem ESP-NOW-Signal\nFunk und Ausgabe belegt · optischer Dunkelnachweis eingeschränkt',fontsize=15)
    for col,(name,title) in enumerate((('walk02','Gehversuch: Hinweg und begonnene Rückkehr'),('outage01','Funkausfall, Wiederkehr und bewusster Neustart'))):
        r=data['runs'][name]; ax=axes[:,col]
        success=[s for s in r['radio'] if s['outcome']==0]
        failed=[s for s in r['radio'] if s['outcome']!=0]
        ax[0].plot([s['time_s'] for s in r['radio']],[s['filtered_dbm'] if s['outcome']==0 else float('nan') for s in r['radio']],color='#245b91',lw=1.8,label='RSSI gefiltert')
        if failed:ax[0].scatter([s['time_s'] for s in failed],[-94]*len(failed),color='#b93435',s=13,marker='x',label='Sendefehler')
        ax[0].set_title(title,fontsize=10)
        ax[0].set_ylim(-98,-15);ax[0].legend(loc='lower left',fontsize=7)
        cmds=[c for c in r['commands'] if c['p'] is not None]
        ax[1].step([c['time_s'] for c in cmds],[c['p']/10 for c in cmds],where='post',color='#84518c',label='Auftrag p (bei ungültig ignoriert)')
        invalid=[c for c in cmds if c['valid']==0]
        if invalid:ax[1].scatter([c['time_s'] for c in invalid],[c['p']/10 for c in invalid],color='#b93435',s=28,marker='x',label='valid = 0')
        ax[1].set_ylim(-5,110);ax[1].legend(loc='lower left',fontsize=7)
        for ch,color,label in ((0,'#bd493c','Rot'),(1,'#428651','Grün')):
            ax[2].plot([s['time_s'] for s in r['tx']],[s['rgb'][ch] for s in r['tx']],color=color,label=label,lw=1.7)
        ax[2].set_ylim(-1,26);ax[2].legend(loc='upper right',fontsize=7,ncol=2)
        t=[s['time_s'] for s in r['camera']]
        ax[3].fill_between(t,[s['minimum'] for s in r['camera']],[s['maximum'] for s in r['camera']],color='#d5ab5b',alpha=.3)
        ax[3].plot(t,[s['mean'] for s in r['camera']],color='#9a650c',lw=1.6)
        ax[3].axhline(0,color='gray',ls=':',lw=.8)
        ax[3].set_xlabel('Sekunden seit erstem START (Hostzeit)')
        ax[3].text(.03,.10,'Dunkelkriterium nach Stop: '+('0/23 LEDs' if col==0 else '15/23 LEDs je Stop'),transform=ax[3].transAxes,fontsize=8,va='bottom',bbox={'facecolor':'white','alpha':.85,'edgecolor':'none'})
        for command in cmds:
            if command['op']=='stop' or (command['op']=='start' and command['generation']==2):
                for panel in ax:panel.axvline(command['time_s'],color='#555',ls='--',lw=.7,alpha=.7)
        for panel in ax:panel.grid(alpha=.18);panel.set_xlim(-5,68)
    for row,label in enumerate(('RSSI (dBm)','Auftrag p (%)','TX-Kanalwert (0–32)','Kameracodes über\nVorstartbasis')):axes[row,0].set_ylabel(label)
    fig.text(.5,.035,'TX und Kamera: feste 0,5-s-Mittel, Kameraband = Minimum/Maximum. Keine Anpassung der Zeitachsen.\nGestrichelt: gezielter Stop bzw. neuer Start. Kameracodes sind keine Luxwerte; Hintergrundabweichungen bleiben sichtbar.',ha='center',fontsize=8)
    fig.savefig(a.output,dpi=150)
    print(a.output)

if __name__=='__main__':main()
