"""Actual owner automaton, real catalog SHA, explicitly synthetic bounded ports."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def source_blob_sha256(raw):
 # Git text checkout may contain CRLF; preserve every other original byte.
 return hashlib.sha256(raw.replace(b"\r\n",b"\n")).hexdigest()

def main():
 p=argparse.ArgumentParser();p.add_argument("--compiler");p.add_argument("--binary");p.add_argument("--case",default="all");p.add_argument("--build-dir",default="private/manual-owner-review");a=p.parse_args();env=dict(os.environ)
 if a.binary:binary=Path(a.binary).resolve()
 else:
  assert a.compiler;cc=Path(a.compiler).resolve();env["PATH"]=str(cc.parent)+os.pathsep+env.get("PATH","");build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/"owner.exe"
  sources=[ROOT/"src"/x for x in ("GameConfiguration/CanonicalPackageJson.cpp","GameConfiguration/PackageDeclarations.cpp","GameConfiguration/PackageModules.cpp","GameExecution/RulesPlanner.cpp","GameExecution/RulesProgressCodec.cpp","GameExecution/ManualOverrideOwner.cpp")]
  subprocess.run([str(cc),"-std=c++11","-O2","-Wall","-Wextra","-Wpedantic","-Werror","-I"+str(ROOT/"include"),str(HERE/"cli.cpp"),*map(str,sources),"-lbcrypt","-o",str(binary)],check=True,env=env)
 original=json.loads((HERE/"catalog.json").read_text(encoding="utf-8"))
 catalogs=HERE.parent/"rules-progress-independent-review/catalog-goldens.json"
 artifacts=HERE.parent/"manual-override-v2-review/vectors.json"
 assert source_blob_sha256(catalogs.read_bytes())==original["sourceCatalogGoldensSHA256"]
 assert source_blob_sha256(artifacts.read_bytes())==original["sourceArtifactsSHA256"]
 assert hashlib.sha256(bytes.fromhex(original["canonicalCatalog"])).hexdigest()==original["sha256"]
 source=json.loads(catalogs.read_text(encoding="utf-8"))[original["sourceArtifactFixture"]]
 assert source["canonical"]==original["canonicalCatalog"] and source["sha256"]==original["sha256"]
 r=subprocess.run([str(binary),a.case],input=original["canonicalCatalog"]+"\n"+original["sha256"]+"\n",text=True,capture_output=True,env=env,timeout=60)
 print(r.stdout,end="");print(r.stderr,end="");assert r.returncode==0,r.returncode
if __name__=="__main__":main()
