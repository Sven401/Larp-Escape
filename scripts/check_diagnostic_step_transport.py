#!/usr/bin/env python3
"""Compile and run the C++11 bounded diagnostic step transport and client reassembly."""
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
sys.dont_write_bytecode = True
from check_headers import find_compiler

def main():
    root = Path(__file__).resolve().parents[1]
    compiler = find_compiler(None)
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
    sources = ["test/test_diagnostic_step_transport/test_main.cpp",
               "src/CompanionIntegration/DiagnosticStepTransport.cpp", "src/CompanionIntegration/DiagnosticControl.cpp", "src/Companion/Identity.cpp",
               "src/Companion/IdentityAccess.cpp", "src/Companion/AuthorityState.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-diagnostic-control-") as directory:
        output = Path(directory) / "diagnostic-test.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-I", str(root / "include"), *[str(root / item) for item in sources], "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        result = subprocess.run([str(output)], check=True, env=environment, timeout=30,
                                capture_output=True, text=True)
        # Independent client vectors for actual C++ serialized bytes.
        prefix = (b"DG\x01\x04" + struct.pack("<I", 17) + bytes([1]) + bytes(15)
                  + struct.pack("<Q", 123)
                  + bytes.fromhex("00000000000040008000000000000001")
                  + bytes.fromhex("0000000000004000800000000000000a")
                  + struct.pack("<II", 1, 1000))
        wake = (prefix + bytes([0, 1, 0, 0, 0, 0, 0, 0]) + struct.pack("<QQ", 100, 200)
                + bytes([0, 0, 1, 0, 0, 0, 64, 1])
                + b"".join(struct.pack("<QBB", 110+i, 1+i%4, 3 if i%2 == 0 else 0)
                           for i in range(64)))
        led = (prefix + bytes([1, 0, 0, 16, 0, 0]) + struct.pack("<HQQ", 500, 100, 600)
               + bytes([0, 0, 1, 1, 2, 0, 0, 1]))
        lines = result.stdout.splitlines()
        for name, expected in (("wakeHex", wake), ("ledHex", led)):
            actual = [line.split("=", 1)[1] for line in lines if line.startswith(name+"=")]
            if actual != [expected.hex()]:
                raise ValueError(f"C++ {name} differs from independent client bytes")
        print("\n".join(line for line in lines if not line.startswith(("wakeHex=", "ledHex="))))
        print("Independent client vectors: 744-byte Wake and 104-byte LED replies match.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
