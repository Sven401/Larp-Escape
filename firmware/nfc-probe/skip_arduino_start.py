"""Use our SDK entry point without Arduino initArduino/NVS auto-repair.

This filters one translation unit in this project's build, without changing the
installed framework or other targets. SDK libraries remain the pinned baseline.
"""
from pathlib import Path

Import("env")
framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
startup = (framework / "cores/esp32/main.cpp").resolve()
if not startup.is_file():
    raise RuntimeError("Pinned Arduino startup source unavailable")


def keep_source(node):
    return None if Path(node.srcnode().get_abspath()).resolve() == startup else node


env.AddBuildMiddleware(keep_source)
