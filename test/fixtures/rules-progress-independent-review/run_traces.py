"""Six unchanged P0 traces, actual D0 writer/verifier, independent storage bytes."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
from Oracle import encode,decode
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec,utils
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
OLD=HERE.parent/"rules-planner-review"
spec=importlib.util.spec_from_file_location("trace_protocol",OLD/"run.py")
protocol=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol)

def blob(raw,pos):
    first=raw[pos];pos+=1;assert first>>5==2;size=first&31
    if size>=24:
        assert size in (24,25,26,27);width=1<<(size-24);size=int.from_bytes(raw[pos:pos+width],"big");pos+=width
    assert pos+size<=len(raw)
    return raw[pos:pos+size],pos+size

def verify_fact(record):
    ref,xy,cose=record;xy=bytes.fromhex(xy["b"]);packet=bytes.fromhex(cose["b"])
    assert packet[:2]==b"\xd2\x84" and len(xy)==64
    protected,pos=blob(packet,2);assert packet[pos]==0xa0
    payload,pos=blob(packet,pos+1);signature,pos=blob(packet,pos)
    assert pos==len(packet) and len(signature)==64
    cosekey=b"\xa5\x01\x02\x03\x26\x20\x01\x21\x58\x20"+xy[:32]+b"\x22\x58\x20"+xy[32:]
    kid=hashlib.sha256(cosekey).digest();assert len(cosekey)==77
    assert protected==b"\xa2\x01\x26\x04\x58\x20"+kid
    key=ec.EllipticCurvePublicNumbers(int.from_bytes(xy[:32],"big"),int.from_bytes(xy[32:],"big"),ec.SECP256R1()).public_key()
    signed=encode(["Signature1",{"b":protected.hex()},{"b":b"larp.event.v1".hex()},{"b":payload.hex()}])
    key.verify(utils.encode_dss_signature(int.from_bytes(signature[:32],"big"),int.from_bytes(signature[32:],"big")),signed,ec.ECDSA(hashes.SHA256()))
    fact=decode(payload);assert encode(fact)==payload
    body=fact["9"];assert fact["0"]==fact["1"]==1 and body[4] is True
    assert hashlib.sha256(payload).hexdigest()==ref[1]["b"]
    assert [fact["2"],fact["4"],body[0],body[1]]==ref[0]
    assert [fact["8"],fact["7"],fact["3"],fact["5"],body[3],body[2]]==ref[2:]
    assert fact["6"]==1
    return fact

def verify_application(row):
    raw=bytes.fromhex(row["codecApplication"]);model=row["model"]
    assert encode(model)==raw and decode(raw)==model
    cat=bytes.fromhex(row["catalog"]);assert encode(row["catalogModel"])==cat
    assert hashlib.sha256(cat).hexdigest()==model["3"]["b"]
    plan=model["4"];proofs=model["5"];outbox=model["6"]
    assert len(outbox)==len(plan[8])<=8
    for record in proofs:verify_fact(record)
    for i,record in enumerate(outbox):
        payload=verify_fact(record);emission=plan[8][i];ref=record[0]
        assert ref[0]==emission[3] and ref[7]==emission[5]
        assert ref[0][3]==plan[1][8]+i+1
        assert ref[6]==row["catalogModel"]["12"][emission[0]-1][1]
        start,count=emission[4];assert payload["9"][5]==[x[0] for x in plan[4][start:start+count]]
    assert len(plan[2][1][11])==19 and len(plan[2][1][11][3])==9
    return len(proofs)+len(outbox)

def main():
    p=argparse.ArgumentParser();g=p.add_mutually_exclusive_group(required=True);g.add_argument("--compiler");g.add_argument("--binary");p.add_argument("--build-dir",default="private/rules-progress-independent-review");p.add_argument("--update-goldens",action="store_true");a=p.parse_args();env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env["PATH"]=str(cc.parent)+os.pathsep+env.get("PATH","");build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/"trace.exe"
        sources=[ROOT/"src"/x for x in ("GameConfiguration/CanonicalPackageJson.cpp","GameConfiguration/PackageDeclarations.cpp","GameConfiguration/PackageModules.cpp","GameExecution/RulesPlanner.cpp","GameExecution/RulesProgressCodec.cpp","Companion/D0Fact.cpp","Companion/D0FactWriter.cpp")]
        subprocess.run([str(cc),"-std=c++11","-O2","-Wall","-Wextra","-Wpedantic","-Werror","-I"+str(ROOT/"include"),str(HERE/"trace.cpp"),*map(str,sources),"-lbcrypt","-o",str(binary)],check=True,env=env)
    source=(ROOT/"docs/spielpakete/fixtures/p0-v1.json").read_bytes().replace(b"\r\n",b"\n")
    assert hashlib.sha256(source).hexdigest()=="58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605"
    packages=json.loads((OLD/"reference-packages.json").read_text(encoding="utf-8"));traces=json.loads((OLD/"reference-traces.json").read_text(encoding="utf-8"))["traces"]
    r=subprocess.run([str(binary)],input="".join(protocol.protocol(p,t) for p,t in zip(packages,traces)),capture_output=True,text=True,encoding="utf-8",env=env,timeout=180)
    assert r.returncode==0,(r.returncode,r.stderr,r.stdout[-1000:])
    actual=[];applications=[];signatures=0;checks=0;catalogs=[]
    for line in r.stdout.splitlines():
        row=json.loads(line)
        if "codecCatalog" in row:
            assert encode(row["catalogModel"]).hex()==row["codecCatalog"];catalogs.append(row);continue
        if "codecStats" in row:
            checks=row["codecStats"];continue
        if "codecApplication" in row:
            signatures+=verify_application(row);applications.append(row)
        else:actual.append(row)
    count=0
    for trace in traces:
        for index,step in enumerate(trace["steps"]):
            row=actual[count];row.pop("audit",None)
            assert row==dict(step["state"],status=step["status"]),(trace["id"],index,row,step)
            count+=1
    assert count==len(actual) and len(catalogs)==len(traces)
    catalog_path=HERE/"trace-catalog-goldens.json"
    actual_catalogs={t["id"]:c for t,c in zip(traces,catalogs)}
    if a.update_goldens:catalog_path.write_text(json.dumps(actual_catalogs,indent=2)+"\n",encoding="utf-8")
    else:assert actual_catalogs==json.loads(catalog_path.read_text(encoding="utf-8")),"preserved actual trace catalog changed"
    path=HERE/"application-goldens.json"
    if a.update_goldens:path.write_text(json.dumps(applications,indent=2)+"\n",encoding="utf-8")
    preserved=json.loads(path.read_text(encoding="utf-8"))
    for row in preserved:verify_application(row)
    decoded=subprocess.run([str(binary),"--decode-application"],input="".join(x["codecApplication"]+"\n" for x in preserved),capture_output=True,text=True,encoding="utf-8",env=env,check=True,timeout=180)
    output=[json.loads(x) for x in decoded.stdout.splitlines()];assert len(output)==len(preserved)
    for source,dest in zip(preserved,output):assert dest["scope"]==2 and dest["decoded"] and not dest["ready"] and dest["admissionEmpty"] and dest["model"]==source["model"]
    sizes=[len(bytes.fromhex(x["codecApplication"])) for x in applications];cats=[len(bytes.fromhex(x["codecCatalog"])) for x in catalogs]
    print(f"{len(traces)} unchanged traces / {count} full state transitions; {len(applications)} actual planner+signed application byte oracles; {signatures} independently verified COSE proof/outbox occurrences; catalog {min(cats)}..{max(cats)} B; application {min(sizes)}..{max(sizes)} B; {checks} C++ assertions")
if __name__=="__main__":main()
