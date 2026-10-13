"""Read-only lab connection evidence; requires pyserial (PlatformIO runtime)."""
import pathlib
import re
import serial
import time

out = pathlib.Path(__file__).resolve().parents[1] / ".local" / "evidence"
out.mkdir(parents=True, exist_ok=True)
port = serial.Serial(port=None, baudrate=115200, timeout=.2)
port.dtr = False
port.rts = False
port.port = "COM3"
captured = bytearray()
try:
    port.open()
    end = time.monotonic() + 35
    while time.monotonic() < end:
        captured.extend(port.read(min(4096, port.in_waiting or 1)))
        if len(captured) > 131072:
            raise RuntimeError("Serial capture limit exceeded")
finally:
    port.close()
    text = captured.decode("utf-8", "replace")
    text = re.sub(r"(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", "[address omitted]", text)
    (out / "esp-serial.txt").write_text(text, encoding="utf-8")
    print(text)
