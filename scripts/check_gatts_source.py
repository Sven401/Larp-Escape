"""Independent counterpart for the actual source-posting Host observation."""
from copy import deepcopy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True

EXPECTED={"scope":"host_pinned_source_model", "frameCopies":13, "sdkPosts":13,
          "sourceCalls":13, "packedConnection":257, "firstTransaction":1,
          "lastTransaction":13, "nextRequest":2, "running":True,
          "rfInitialized":False, "deviceMeasured":False}

def validate(value):
    if not isinstance(value,dict) or set(value)!=set(EXPECTED):
        raise ValueError("source posting observation fields differ")
    for key,expected in EXPECTED.items():
        if type(value[key]) is not type(expected) or value[key]!=expected:
            raise ValueError(f"source posting correlation differs: {key}")

def check_gatts_source(base):
    path=base/"fixtures/gatts/source-posting-observation.json"
    actual=json.loads(path.read_text(encoding="utf-8"))
    validate(actual)
    negatives=[]
    for key,value in EXPECTED.items():
        bad=deepcopy(actual)
        bad[key]=not value if type(value) is bool else value+1 if type(value) is int else "radio_verified"
        negatives.append(bad)
    for key,value in (("packedConnection",1),("packedConnection",256),("sdkPosts",True),
                      ("sourceCalls",0),("lastTransaction",1)):
        bad=deepcopy(actual); bad[key]=value; negatives.append(bad)
    bad=deepcopy(actual); bad["peerDelivered"]=True; negatives.append(bad)
    for bad in negatives:
        try: validate(bad)
        except ValueError: continue
        raise ValueError("incorrect source observation accepted")
    return len(negatives)

if __name__=="__main__":
    print(f"Source posting counterpart: {check_gatts_source(Path(__file__).resolve().parents[1]/'docs/companion')} negatives rejected; Host SDK model only")
