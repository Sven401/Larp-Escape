"""Actual decode failures + explicit unauthenticated raw-evidence boundaries."""
import argparse,json,subprocess
from pathlib import Path
from run_traces import verify_application
from cryptography.exceptions import InvalidSignature
HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser();p.add_argument("--catalog-binary",required=True);p.add_argument("--trace-binary",required=True);a=p.parse_args()
    source=json.loads((HERE/"malformed.json").read_text(encoding="utf-8"));counts={};accepted=0
    for domain,binary,option in (("catalog",a.catalog_binary,"--decode-catalog"),("application",a.trace_binary,"--decode-application")):
        cases=[x for x in source["cases"] if x["domain"]==domain]
        for entry in source["allShorterPrefixes"]:
            if entry["domain"]==domain:
                raw=bytes.fromhex(entry["hex"])
                cases.extend({"name":entry["name"]+f"-prefix{n}","hex":raw[:n].hex(),"accepted":False,"cryptoRejected":False} for n in range(len(raw)))
        r=subprocess.run([str(Path(binary).resolve()),option],input="".join(x["hex"]+"\n" for x in cases),capture_output=True,text=True,encoding="utf-8",timeout=300)
        assert r.returncode==0,(domain,r.returncode,r.stderr)
        lines=r.stdout.splitlines();assert len(lines)==len(cases),(domain,len(lines),len(cases))
        for case,line in zip(cases,lines):
            if domain=="catalog":
                values=list(map(int,line.split()));assert not case["accepted"]
                assert values[0]==0 and values[1]!=0 and values[2:]==[0,0,0,0],(case["name"],line)
            else:
                v=json.loads(line)
                assert not v["ready"] and v["admissionEmpty"],(case["name"],v)
                if not case["accepted"]:
                    assert v["scope"]==0 and v["error"]!=0 and not v["decoded"] and v["model"] is None and all(v[k]==0 for k in ("emissions","evidence","proofs","outbox","bytes")),(case["name"],v)
                else:
                    assert v["scope"]==2 and v["decoded"],(case["name"],v)
                    accepted+=1
                    if case["cryptoRejected"]:
                        from run_traces import verify_fact
                        try:verify_fact(v["model"]["6"][0])
                        except (InvalidSignature,ValueError,AssertionError):pass
                        else:raise AssertionError(case["name"]+" must fail independent cryptographic verification")
        counts[domain]=len(cases)
    print(f"{counts} actual malformed/prefix decodes; {accepted} explicitly unauthenticated representation-only cases; no executable plan/admission")
if __name__=="__main__":main()
