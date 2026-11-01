"""Independent actual artifact/compiler/program storage qualification."""
import argparse,copy,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
from Oracle import encode,decode
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
OLD=HERE.parent/"manual-override-v2-review"
spec=importlib.util.spec_from_file_location("artifact_protocol",OLD/"run.py")
protocol_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol_module)

def main():
    p=argparse.ArgumentParser();g=p.add_mutually_exclusive_group(required=True);g.add_argument("--compiler");g.add_argument("--binary");p.add_argument("--build-dir",default="private/rules-progress-independent-review");p.add_argument("--update-goldens",action="store_true");a=p.parse_args()
    env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env["PATH"]=str(cc.parent)+os.pathsep+env.get("PATH","")
        build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/"catalog.exe"
        source=[ROOT/"src"/x for x in ("GameConfiguration/CanonicalPackageJson.cpp","GameConfiguration/PackageDeclarations.cpp","GameConfiguration/PackageModules.cpp","GameConfiguration/PackageMetadata.cpp","GameConfiguration/ProfileReader.cpp","GameConfiguration/ProfileValidation.cpp","GameExecution/RulesPlanner.cpp","GameExecution/PackageArtifacts.cpp","GameExecution/RulesProgressCodec.cpp")]
        subprocess.run([str(cc),"-std=c++11","-O2","-Wall","-Wextra","-Wpedantic","-Werror","-I"+str(ROOT/"include"),str(HERE/"cli.cpp"),*map(str,source),"-lbcrypt","-o",str(binary)],check=True,env=env)
    cases=json.loads((OLD/"vectors.json").read_text(encoding="utf-8"));payload="".join(protocol_module.protocol(c) for c in cases)
    r=subprocess.run([str(binary)],input=payload,capture_output=True,text=True,encoding="utf-8",env=env,timeout=180)
    assert r.returncode==0,r.stderr
    lines=iter(r.stdout.splitlines());golden={};bad=[];positives=0;last_checks=0
    for c in cases:
        status=next(lines).split();summary=json.loads(next(lines));result=json.loads(next(lines))
        accepted=c["expected"]=="IntegrityCheckedUnauthenticated"
        assert (status[0]=="A")==accepted,(c["name"],status)
        if not accepted:
            assert result=={"skipped":True} and status[2]=="0",c["name"]
            if "expectedError" in c:assert status[1]==c["expectedError"],c["name"]
            continue
        positives+=1
        model=result["model"];expected=encode(model)
        assert expected.hex()==result["catalog"],c["name"]
        assert decode(expected)==model,c["name"]
        golden[c["name"]]={"model":model,"canonical":expected.hex(),"sha256":hashlib.sha256(expected).hexdigest()}
        last_checks=result["checks"]
        # Derived group values are immutable persisted data, not repairable hints.
        for index,rule in enumerate(model["7"]):
            changed=copy.deepcopy(model)
            choices=[0,len(model["7"])+1] if rule[0][8] else [1]
            for group in choices:
                if group==rule[0][8]:continue
                changed["7"][index][0][8]=group
                bad.append((c["name"]+f"-group{index}-{group}",encode(changed)))
    assert next(lines,None) is None
    golden_path=HERE/"catalog-goldens.json"
    if a.update_goldens:golden_path.write_text(json.dumps(golden,indent=2)+"\n",encoding="utf-8")
    else:assert golden==json.loads(golden_path.read_text(encoding="utf-8")),"preserved compiler/model/independent-byte golden changed"
    d=subprocess.run([str(binary),"--decode-catalog"],input="".join(raw.hex()+"\n" for _,raw in bad),capture_output=True,text=True,env=env,timeout=180,check=True)
    output=d.stdout.splitlines();assert len(output)==len(bad)
    for (name,raw),line in zip(bad,output):
        values=list(map(int,line.split()));assert values[0]==0 and values[2:]==[0,0,0,0],(name,line)
    print(f"{len(cases)} actual artifact sets -> {positives} actual compiled catalog roundtrips/independent byte oracles; {len(bad)} stored-group counterexamples; {last_checks} C++ checks")
if __name__=="__main__":main()
