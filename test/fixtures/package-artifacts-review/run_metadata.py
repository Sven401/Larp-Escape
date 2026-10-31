"""Independent approved metadata-profile counter check on the real compiler."""
import argparse,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def protocol(c,content_failure=-1,module_failure=-1):
    lines=[f"{len(c['modules'])} {content_failure} {module_failure}"]
    for m in c['modules']:lines.extend([m['moduleId'],m['version'],m['digest'],m['bytes']])
    return '\n'.join(lines+[c['content']])+'\n'
def main():
    p=argparse.ArgumentParser(__doc__);b=p.add_mutually_exclusive_group(required=True);b.add_argument('--compiler');b.add_argument('--binary');p.add_argument('--build-dir',default='private/package-artifacts-review');a=p.parse_args();env=dict(os.environ)
    if a.binary:binary=Path(a.binary).resolve()
    else:
        cc=Path(a.compiler).resolve();env['PATH']=str(cc.parent)+os.pathsep+env.get('PATH','');build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);binary=build/'metadata.exe';sources=[ROOT/'src/GameConfiguration'/f for f in ['CanonicalPackageJson.cpp','PackageDeclarations.cpp','PackageModules.cpp','PackageMetadata.cpp']];subprocess.run([str(cc),'-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(ROOT/'include'),str(HERE/'metadata.cpp'),*map(str,sources),'-lbcrypt','-o',str(binary)],check=True,env=env)
    cases=json.loads((HERE/'metadata-vectors.json').read_text(encoding='utf8'));payload=''.join(protocol(c) for c in cases)
    first=cases[0];faults=[]
    for source in ['content','module']:
        count=len(first['content'] if source=='content' else first['modules'][0]['bytes'])
        for offset in [0,count//2,count]:
            payload+=protocol(first,offset if source=='content' else -1,offset if source=='module' else -1)+protocol(first);faults.append((source,offset))
    r=subprocess.run([str(binary)],input=payload,capture_output=True,text=True,encoding='utf8',env=env,check=True);lines=iter(r.stdout.splitlines())
    def healthy(c,line):
        parts=line.split();assert parts[0]=='A',(c['name'],line)
        assert int(parts[3])==c['expectedMetadata'] and int(parts[4])==c['expectedRaw'] and bool(int(parts[5]))==(c['expectedMetadata']>32768),(c['name'],line,c['expectedMetadata'],c['expectedRaw'])
    for c in cases:healthy(c,next(lines))
    for source,offset in faults:
        line=next(lines);parts=line.split();assert parts[0]=='R' and parts[2]=='2',(source,offset,line);healthy(first,next(lines))
    assert next(lines,None) is None
    print(len(cases),'exact shared metadata vectors and',len(faults),'source-failure/clean-reuse pairs passed')
if __name__=='__main__':main()
