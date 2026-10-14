"""Fail the build if PlatformIO selects a transitive version instead of our pins.

The package manager may download unused registry duplicates. This hook validates
actual LibraryBuilder inputs after dependency resolution, not directory names.
"""
Import("env")

import json
from pathlib import Path
import subprocess

PINS = {
    "NeoPixelBus": ("1d7ff38f14d04a976f9c6e365c83a232bcba04fc", "https://github.com/Makuna/NeoPixelBus.git"),
    "ESPAsyncWebServerWLED": ("39b830c852054444ea12a8b5d7fcb4fa004a89d7", "https://github.com/Aircoookie/ESPAsyncWebServer.git"),
}
original_configure = env.ConfigureProjectLibBuilder


def git(path, *arguments):
    return subprocess.run(["git", "-C", str(path), *arguments], check=True,
                          capture_output=True, text=True).stdout.strip()


def configure_with_verified_pins(build_env):
    result = original_configure.clone(build_env)()
    selected = {dependency.name: dependency for dependency in result.depbuilders}
    evidence = {}
    for name, (revision, origin) in PINS.items():
        dependency = selected.get(name)
        if dependency is None:
            raise RuntimeError(f"Required pinned dependency was not selected: {name}")
        path = Path(dependency.path)
        if not (path / ".git").exists() or git(path, "rev-parse", "HEAD") != revision:
            raise RuntimeError(f"Wrong source selected for {name}: expected Git {revision}, got {path}")
        if git(path, "remote", "get-url", "origin") != origin:
            raise RuntimeError(f"Unexpected source origin for {name}")
        git(path, "diff", "--quiet")
        git(path, "diff", "--cached", "--quiet")
        evidence[name] = {"revision": revision, "origin": origin, "source": str(path)}
    tcp = selected.get("AsyncTCP")
    if tcp is None or tcp.version != "3.4.7":
        raise RuntimeError("AsyncTCP must resolve to the explicit 3.4.7 pin")
    evidence["AsyncTCP"] = {"version": tcp.version, "source": tcp.path}
    output = Path(build_env.subst("$BUILD_DIR")) / "dependency-provenance.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print("Verified selected WLED dependencies: NeoPixelBus 1d7ff38, ESPAsyncWebServer 39b830c, AsyncTCP 3.4.7")
    return result


env.AddMethod(configure_with_verified_pins, "ConfigureProjectLibBuilder")
