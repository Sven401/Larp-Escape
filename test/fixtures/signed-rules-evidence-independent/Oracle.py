"""Independent canonical CBOR oracle for the versioned documented storage schema."""
def head(major,n):
    if n < 24:return bytes([(major<<5)|n])
    for width,ai in ((1,24),(2,25),(4,26),(8,27)):
        if n < 2**(width*8):return bytes([(major<<5)|ai])+n.to_bytes(width,"big")
    raise ValueError("uint64")

def encode(v):
    if type(v) is bool:return b"\xf5" if v else b"\xf4"
    if type(v) is int:return head(0,v) if v>=0 else head(1,-v-1)
    if type(v) is str:
        b=v.encode("utf-8");return head(3,len(b))+b
    if type(v) is list:return head(4,len(v))+b"".join(encode(x) for x in v)
    if type(v) is dict:
        if set(v)=={"b"}:
            b=bytes.fromhex(v["b"]);return head(2,len(b))+b
        keys=sorted(map(int,v))
        return head(5,len(keys))+b"".join(encode(k)+encode(v[str(k)] if str(k) in v else v[k]) for k in keys)
    raise TypeError(type(v))

def decode(raw):
    pos=0
    def read():
        nonlocal pos
        assert pos<len(raw)
        first=raw[pos];pos+=1;major=first>>5;ai=first&31
        if major==7:
            assert ai in (20,21);return ai==21
        if ai<24:n=ai
        else:
            assert ai in (24,25,26,27)
            width=1<<(ai-24);assert pos+width<=len(raw)
            n=int.from_bytes(raw[pos:pos+width],"big");pos+=width
            assert n >= (24 if width==1 else 1<<(8*(width//2)))
        if major==0:return n
        if major==1:return -n-1
        if major in (2,3):
            assert pos+n<=len(raw);b=raw[pos:pos+n];pos+=n
            return {"b":b.hex()} if major==2 else b.decode("utf-8",errors="strict")
        if major==4:return [read() for _ in range(n)]
        assert major==5
        out={};last=-1
        for _ in range(n):
            key=read();assert type(key) is int and key>last;last=key;out[str(key)]=read()
        return out
    result=read();assert pos==len(raw);return result
