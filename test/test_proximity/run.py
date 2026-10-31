"""Build native proximity core with the shared NearGlow core and real host HMAC."""
import hmac
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]

def main():
    compiler = shutil.which(os.environ.get("CXX", "g++"))
    if not compiler and os.name == "nt":
        candidate = Path.home()/".platformio/packages/toolchain-gccmingw32/bin/g++.exe"
        if candidate.is_file(): compiler = str(candidate)
    if not compiler: raise SystemExit("C++ compiler unavailable")
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).parent)+os.pathsep+env.get("PATH", "")
    key = bytes(range(32))
    a, b = bytes([2,1,2,3,4,5]), bytes([2,1,2,3,4,6])
    pair = hmac.digest(key, b"LARP-PROX-PAIR/1"+a+b, "sha256")[:16]
    packet = b"LPRX"+bytes([1,1,0,0])+pair+a+b+bytes([1])*16+(1).to_bytes(4,"little")+bytes([7])*16+bytes(16)
    packet += hmac.digest(key, packet, "sha256")
    with tempfile.TemporaryDirectory(prefix="larp-proximity-") as folder:
        folder = Path(folder)
        (folder/"golden.h").write_text("constexpr unsigned char Golden[] = {"+",".join(map(str,packet))+"};\n")
        out = folder/("proximity.exe" if os.name == "nt" else "proximity")
        args = [compiler,"-std=c++17","-Wall","-Wextra","-Wpedantic","-Werror","-I",str(folder),
                "-I",str(ROOT/"firmware/idf-proximity/main"),"-I",str(ROOT/"firmware/led/include"),
                str(ROOT/"test/test_proximity/main.cpp"),str(ROOT/"firmware/led/src/NearGlow.cpp"),
                "-lbcrypt" if os.name == "nt" else "-lcrypto","-o",str(out)]
        subprocess.run(args,check=True,env=env)
        subprocess.run([str(out)],check=True,env=env)
        adapter = folder/("adapter.exe" if os.name == "nt" else "adapter")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Wpedantic","-Werror",
            "-I",str(ROOT/"test/test_proximity/stubs"),"-I",str(ROOT/"firmware/idf-proximity/main"),
            "-I",str(ROOT/"firmware/led/include"),str(ROOT/"test/test_proximity/adapter.cpp"),
            str(ROOT/"firmware/led/src/NearGlow.cpp"),"-o",str(adapter)],check=True,env=env)
        subprocess.run([str(adapter)],check=True,env=env)
        vectors = json.loads((ROOT/"test/test_proximity/ble_vectors.json").read_text(encoding="utf-8"))
        (folder/"ble_vectors.h").write_text("namespace fixtures {\n"+"\n".join("const unsigned char "+name+"[] = {"+",".join(map(str,bytes.fromhex(value)))+"};" for name,value in vectors.items())+"\n}\n",encoding="utf-8")
        ble = folder/("ble.exe" if os.name == "nt" else "ble")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Wpedantic","-Werror",
            "-I",str(folder),"-I",str(ROOT/"firmware/idf-proximity/main"),"-I",str(ROOT/"firmware/led/include"),
            str(ROOT/"test/test_proximity/ble.cpp"),str(ROOT/"firmware/idf-proximity/main/ProximityBleProtocol.cpp"),
            str(ROOT/"firmware/led/src/NearGlow.cpp"),"-lbcrypt" if os.name == "nt" else "-lcrypto","-o",str(ble)],check=True,env=env)
        subprocess.run([str(ble)],check=True,env=env)
        runtime = folder/("ble-runtime.exe" if os.name == "nt" else "ble-runtime")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Wpedantic","-Werror",
            "-I",str(ROOT/"test/test_proximity/stubs"),"-I",str(ROOT/"firmware/idf-proximity/main"),"-I",str(ROOT/"firmware/led/include"),
            str(ROOT/"test/test_proximity/ble_runtime.cpp"),str(ROOT/"firmware/idf-proximity/main/ProximityBleProtocol.cpp"),
            str(ROOT/"firmware/led/src/NearGlow.cpp"),"-o",str(runtime)],check=True,env=env)
        subprocess.run([str(runtime)],check=True,env=env)
        bearer = folder/("ble-bearer.exe" if os.name == "nt" else "ble-bearer")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Wpedantic","-Werror",
            "-I",str(ROOT/"test/test_proximity/ble_stubs"),"-I",str(ROOT/"firmware/idf-proximity/main"),"-I",str(ROOT/"firmware/led/include"),
            str(ROOT/"test/test_proximity/ble_bearer.cpp"),str(ROOT/"firmware/idf-proximity/main/ProximityBleBearer.cpp"),
            str(ROOT/"firmware/idf-proximity/main/ProximityBleProtocol.cpp"),str(ROOT/"firmware/led/src/NearGlow.cpp"),"-o",str(bearer)],check=True,env=env)
        for case in ("roundtrip","interval45","notify-error","stale-queued","stale-partial","busy","full-in","full-out","copy","epochs"):
            subprocess.run([str(bearer),case],check=True,env=env)

if __name__ == "__main__": main()
