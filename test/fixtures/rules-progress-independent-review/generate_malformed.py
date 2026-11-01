"""Independent hostile canonical storage bytes derived from preserved actual inputs."""
import copy,json
from pathlib import Path
from Oracle import encode
HERE=Path(__file__).resolve().parent

def generate():
    catalogs=json.loads((HERE/"catalog-goldens.json").read_text(encoding="utf-8"))
    applications=json.loads((HERE/"application-goldens.json").read_text(encoding="utf-8"))
    pair=next(v["model"] for v in catalogs.values() if v["model"]["2"][1] and len(v["model"]["7"])==2)
    app=max(applications,key=lambda x:len(x["codecApplication"]))["model"]
    rows=[]
    def add(domain,name,model,accepted=False,crypto=False):
        rows.append({"domain":domain,"name":name,"hex":encode(model).hex(),"accepted":accepted,"cryptoRejected":crypto})
    def cat(name,change):
        x=copy.deepcopy(pair);change(x);add("catalog",name,x)
    cat("unknown-language",lambda x:x.__setitem__("2",[3,True]))
    cat("nonbool-manual",lambda x:x.__setitem__("2",[2,1]))
    cat("context-wrong-width",lambda x:x["1"].__setitem__(0,{"b":"01"*15}))
    cat("uint64-overflow-negative",lambda x:x["1"].__setitem__(8,-1))
    cat("unterminated-string-pool",lambda x:x["3"].__setitem__("b",x["3"]["b"][:-2]+"01"))
    cat("string-offset-outside",lambda x:x["4"].__setitem__(-1,65535))
    cat("node-unknown-op",lambda x:x["5"][0].__setitem__(1,255))
    cat("node-unknown-type",lambda x:x["5"][0].__setitem__(2,255))
    cat("node-int32-overflow",lambda x:x["5"][0].__setitem__(3,2**31))
    cat("node-int32-underflow",lambda x:x["5"][0].__setitem__(3,-2**31-1))
    cat("node-child-count9",lambda x:x["5"][0].__setitem__(6,9))
    cat("rule-generation-overflow",lambda x:x["7"][0][0].__setitem__(9,2**32))
    cat("rule-guard-outside",lambda x:x["7"][0][0].__setitem__(3,65535))
    cat("action-target-outside",lambda x:x["6"][0].__setitem__(2,65535))
    cat("subject-empty-view-nonzero-start",lambda x:x["12"][0].__setitem__(6,[1,0]))
    cat("subject-view-outside",lambda x:x["12"][0].__setitem__(6,[0,65535]))
    cat("subject-epoch-width15",lambda x:x["11"][0].__setitem__("b","11"*15))
    cat("subject-name-outside",lambda x:x["12"][0].__setitem__(0,65535))
    cat("port-nonbool-direction",lambda x:x["10"][0].__setitem__(2,0))
    cat("resource-unknown-type",lambda x:x["13"][0].__setitem__(1,255))
    cat("unknown-map-key",lambda x:x.__setitem__("16",0))
    cat("missing-map-key",lambda x:x.pop("15"))
    def application(name,change):
        x=copy.deepcopy(app);change(x);add("application",name,x)
    application("zero-operation",lambda x:x.__setitem__("1",{"b":"00"*16}))
    application("outbox-sequence-mismatch",lambda x:x["6"][0][0][0].__setitem__(3,999))
    application("prediction-both-changed",lambda x:(x["4"][8][0][3].__setitem__(3,999),x["6"][0][0][0].__setitem__(3,999)))
    application("prediction-overflow",lambda x:x["4"][1].__setitem__(8,2**64-1))
    application("missing-outbox",lambda x:x.__setitem__("6",[]))
    application("duplicate-outgoing-id",lambda x:(x["4"][8].append(copy.deepcopy(x["4"][8][0])),x["6"].append(copy.deepcopy(x["6"][0]))))
    application("missing-required-proof",lambda x:x["5"].pop())
    application("duplicate-proof-id",lambda x:x["5"].append(copy.deepcopy(x["5"][0])))
    application("zero-proof-sequence",lambda x:x["5"][0][0][0].__setitem__(3,0))
    application("unknown-fact-type",lambda x:x["5"][0][0].__setitem__(7,3))
    application("empty-proof-cose",lambda x:x["5"][0][2].__setitem__("b",""))
    application("oversized-proof-cose",lambda x:x["5"][0][2].__setitem__("b","00"*2049))
    application("unknown-input-kind",lambda x:x["4"][2][1].__setitem__(1,4))
    application("nonbool-input-known",lambda x:x["4"][2][1].__setitem__(8,1))
    application("input-int32-overflow",lambda x:x["4"][2][1].__setitem__(9,2**31))
    application("input-int32-underflow",lambda x:x["4"][2][1].__setitem__(9,-2**31-1))
    application("nonowned-dependency-view",lambda x:x["4"][8][0].__setitem__(4,[len(x["4"][4]),1]))
    application("noncanonical-empty-view",lambda x:x["4"][8][0].__setitem__(4,[1,0]))
    application("unknown-map-key",lambda x:x.__setitem__("8",0))
    application("missing-map-key",lambda x:x.pop("7"))
    # Opaque COSE is transported, not authenticated by this representation codec.
    x=copy.deepcopy(app);packet=bytearray.fromhex(x["6"][0][2]["b"]);packet[-1]^=1;x["6"][0][2]["b"]=packet.hex();add("application","opaque-invalid-signature-unauthenticated",x,True,True)
    x=copy.deepcopy(app);x["6"][0][1]["b"]="00"*64;add("application","opaque-invalid-public-key-unauthenticated",x,True,True)
    # Historical raw data can be decoded but can never create private live admission.
    x=copy.deepcopy(app);x["4"][2][1][1]=3;raw=x["4"][2][1][11];raw[0]=copy.deepcopy(x["4"][0]);raw[1]=copy.deepcopy(x["3"]);raw[2]=copy.deepcopy(x["4"][1]);raw[8]=copy.deepcopy(x["1"]);x["7"]={"b":"01"};add("application","raw-historical-override-never-live-admission",x,True)

    historical=copy.deepcopy(x)
    wrong_program=copy.deepcopy(historical);wrong_program["4"][2][1][11][1]={"b":"ff"*32}
    add("application","raw-program-fingerprint-substitution",wrong_program)
    clock=copy.deepcopy(historical);clock["4"][1][9]+=1
    add("application","raw-clock-planning-ahead-unauthenticated",clock,True)
    clock=copy.deepcopy(historical);clock["4"][2][1][11][2][9]+=1
    add("application","raw-clock-original-ahead-reject",clock)
    for field in range(9):
        clock=copy.deepcopy(historical);clock["4"][2][1][11][2][field]+=1
        add("application",f"raw-clock-changed-logical-fence-{field}",clock)
    # Defined opaque byte field permits a representation-only exact size boundary;
    # these bytes are NOT asserted to be valid Owner authorization evidence.
    x=copy.deepcopy(app)
    for size in range(32768-len(encode(x))-8,32768-len(encode(x))+8):
        x["7"]={"b":"00"*size}
        if len(encode(x))==32768:break
    else:raise AssertionError("exact 32768 boundary")
    add("application","exact-32768-representation-only",x,True)
    x["7"]["b"]+="00";add("application","32769-representation-limit",x)
    prefixes=[{"domain":"catalog","name":"actual-populated-catalog","hex":min(catalogs.values(),key=lambda x:len(x["canonical"]))["canonical"]},
              {"domain":"application","name":"actual-signed-proof-outbox-application","hex":max(applications,key=lambda x:len(x["codecApplication"]))["codecApplication"]}]
    for domain,model in (("catalog",pair),("application",app)):
        raw=encode(model)
        keys=sorted(map(int,model));duplicate=bytes([0xa0+len(keys)])+b"".join(encode(k)+encode(model[str(k)]) for k in keys[:-1])+encode(keys[-2])+encode(model[str(keys[-1])])
        rows.extend([{"domain":domain,"name":"duplicate-map-key-same-field-count","hex":duplicate.hex(),"accepted":False,"cryptoRejected":False},
                     {"domain":domain,"name":"forbidden-root-tag","hex":(b"\xd2"+raw).hex(),"accepted":False,"cryptoRejected":False},{"domain":domain,"name":"trailing-byte","hex":(raw+b"\x00").hex(),"accepted":False,"cryptoRejected":False},
                     {"domain":domain,"name":"nonminimal-map-length","hex":(bytes([0xb8,raw[0]&31])+raw[1:]).hex(),"accepted":False,"cryptoRejected":False},
                     {"domain":domain,"name":"indefinite-map","hex":(b"\xbf"+raw[1:]+b"\xff").hex(),"accepted":False,"cryptoRejected":False}])
    return {"scope":"representation faults; opaque crypto failures stay unauthenticated; no positive Owner admission","cases":rows,"allShorterPrefixes":prefixes}
if __name__=="__main__":(HERE/"malformed.json").write_text(json.dumps(generate(),indent=2)+"\n",encoding="utf-8")
