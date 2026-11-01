"""Independent canonical byte oracle. No product encoder/import."""
import json
from pathlib import Path

def head(m,n):
 if n<24:return bytes([m*32+n])
 k=1 if n<256 else 2 if n<65536 else 4 if n<2**32 else 8
 return bytes([m*32+{1:24,2:25,4:26,8:27}[k]])+n.to_bytes(k,'big')
def c(v):
 if isinstance(v,int):return head(0,v)
 if isinstance(v,bytes):return head(2,len(v))+v
 if isinstance(v,str):return head(3,len(v.encode()))+v.encode()
 if isinstance(v,list):return head(4,len(v))+b''.join(map(c,v))
 return head(5,len(v))+b''.join(c(k)+c(x) for k,x in v.items())
def build():
 rows=[]
 limits=[0,23,24,255,256,65535,65536,2**32-1,2**32,2**64-1]
 for phase,result in [(1,0),(2,1),(2,2),(2,3)]:
  for index in (0,31):
   m={0:'larp.rules.effect.v1',1:bytes(range(1,17)),2:bytes(range(32)),3:index,4:bytes(range(17,33)),5:limits,6:phase,7:result}
   rows.append({'name':f'valid-{phase}-{result}-{index}','valid':True,'hex':c(m).hex(),'phase':phase,'result':result,'index':index})
 base={0:'larp.rules.effect.v1',1:bytes(range(1,17)),2:bytes(range(32)),3:31,4:bytes(range(17,33)),5:limits,6:1,7:0}
 def bad(name,b):rows.append({'name':name,'valid':False,'hex':b.hex()})
 for field,values in {0:['larp.rules.effect.v2','larp.rules.effect.v1x'],1:[b'',bytes(16),bytes(15),bytes(17)],2:[bytes(31),bytes(33)],3:[32,65536,2**64-1],4:[bytes(16),bytes(15),bytes(17)],5:[limits[:-1],limits+[0]],6:[0,3,255,2**64-1],7:[1,2,3,4,255,2**64-1]}.items():
  for i,v in enumerate(values):
   m=dict(base);m[field]=v;bad(f'field-{field}-{i}',c(m))
 for k in range(8):
  m=dict(base);del m[k];bad(f'missing-{k}',c(m))
 m=dict(base);m[8]=0;bad('extra-field',c(m))
 pairs=list(base.items());pairs[2],pairs[3]=pairs[3],pairs[2]
 bad('order',head(5,8)+b''.join(c(k)+c(v) for k,v in pairs))
 pairs=list(base.items());pairs[-1]=(6,0)
 bad('duplicate-key',head(5,8)+b''.join(c(k)+c(v) for k,v in pairs))
 raw=c(base)
 for name,b in [('tag',b'\xc0'+raw),('trailing',raw+b'\x00'),('indef-map',b'\xbf'+raw[1:]+b'\xff'),('nonminimal-map',b'\xb8\x08'+raw[1:]),('nonminimal-key',raw[:1]+b'\x18\x00'+raw[2:]),('negative-index',raw.replace(b'\x03\x18\x1f',b'\x03\x20',1)),('null-index',raw.replace(b'\x03\x18\x1f',b'\x03\xf6',1)),('bool-index',raw.replace(b'\x03\x18\x1f',b'\x03\xf5',1)),('oversize',raw+bytes(3073))]:bad(name,b)
 return rows
if __name__=='__main__':Path(__file__).with_name('vectors.json').write_text(json.dumps(build(),indent=2)+'\n',encoding='utf8')
