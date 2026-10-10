#!/usr/bin/env python3
"""Compile and run the C++11 DG control wire adapter against the actual diagnostic owner and fake ports."""
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
    sources = ["test/test_diagnostic_transport/test_main.cpp",
               "src/CompanionIntegration/DiagnosticTransport.cpp", "src/CompanionIntegration/DiagnosticControl.cpp", "src/Companion/Identity.cpp",
               "src/Companion/IdentityAccess.cpp", "src/Companion/AuthorityState.cpp"]
    with tempfile.TemporaryDirectory(prefix="larp-diagnostic-control-") as directory:
        output = Path(directory) / "diagnostic-test.exe"
        command = [str(compiler), "-std=c++11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-I", str(root / "include"), *[str(root / item) for item in sources], "-o", str(output)]
        subprocess.run(command, check=True, env=environment, timeout=60)
        result = subprocess.run([str(output)], check=True, env=environment, timeout=30,
                                capture_output=True, text=True)
        # Independent client interpretation of the actual C++ response, including
        # reserved bytes, endianness, UUID order and every correlation field.
        expected = (b"DG\x01\x82" + struct.pack("<I", 1) + bytes([1]) + bytes(15)
                    + struct.pack("<Q", 123)
                    + bytes.fromhex("00000000000040008000000000000002")
                    + bytes.fromhex("0000000000004000800000000000000b")
                    + bytes([0, 3, 1, 0]) + struct.pack("<QQI", 110, 10110, 60000))
        lines = result.stdout.splitlines()
        actual = [line.removeprefix("grantHex=") for line in lines if line.startswith("grantHex=")]
        if actual != [expected.hex()]:
            raise ValueError("C++ DG grant response differs from independent wire fixture")
        print("\n".join(line for line in lines if not line.startswith("grantHex=")))
        print("Independent client: all 88 grant response bytes match.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
