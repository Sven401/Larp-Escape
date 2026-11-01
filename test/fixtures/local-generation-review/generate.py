"""Independent physical record bytes; IEEE CRC32 is not authentication."""
from pathlib import Path
import json,zlib
HERE=Path(__file__).resolve().parent
def record(generation,boot=bytes(range(1,17))):
 b=bytearray(b'LARPLG01'+(1).to_bytes(2,'little')+(64).to_bytes(2,'little')+bytes(4)+generation.to_bytes(8,'little')+boot+bytes(20));assert len(b)==60;return bytes(b)+zlib.crc32(b).to_bytes(4,'little')
def build():
 rows=[]
 for g in [1,255,256,65535,65536,2**32-1,2**32,2**63,2**64-2,2**64-1]:rows.append(dict(name='valid-'+str(g),raw=record(g).hex(),generation=str(g),valid=True))
 rows.append(dict(name='valid-last-boot-byte',raw=record(1,bytes(15)+b'\x01').hex(),generation='1',valid=True))
 original=record(1)
 for i in range(64):
  for bit in range(8):
   b=bytearray(original);b[i]^=1<<bit;rows.append(dict(name='bit-'+str(i)+'-'+str(bit),raw=b.hex(),valid=False))
 for i in list(range(16))+list(range(40,60)):
  b=bytearray(original);b[i]^=1;b[60:]=zlib.crc32(b[:60]).to_bytes(4,'little');rows.append(dict(name='structural-'+str(i),raw=b.hex(),valid=False))
 rows += [dict(name='zero-generation-valid-crc',raw=record(0).hex(),valid=False),dict(name='zero-boot-valid-crc',raw=record(1,bytes(16)).hex(),valid=False)]
 return rows
if __name__=='__main__':(HERE/'records.json').write_text(json.dumps(build(),indent=2)+'\n',encoding='utf8',newline='\n')
