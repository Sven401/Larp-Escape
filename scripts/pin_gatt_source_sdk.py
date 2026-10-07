"""Reject a different private SDK binary for the isolated source-hook probe."""
import hashlib
from pathlib import Path
Import("env")
framework=Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
library=framework/"tools/sdk/esp32/lib/libbt.a"
expected="1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787"
if not library.is_file() or hashlib.sha256(library.read_bytes()).hexdigest()!=expected:
    raise RuntimeError("private GATT source ABI: libbt.a differs from the reviewed pin; re-audit required")
print("GATT source SDK binary pin: verified (isolated probe only)")
