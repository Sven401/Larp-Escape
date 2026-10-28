#!/usr/bin/env python3
"""Check the explicitly reviewed C/C++ formatting baseline with clang-format 19.1.7.

Baseline: all hardware-free LAYER_ROOTS, the adapter files named below and the
reviewed host-suite directories named below. Older ESP32 configuration,
ArduinoClock, EspNowCommunicator and test_regression are outside this baseline.
The default is read-only (--dry-run --Werror). --list shows every selected file;
--fix modifies only that validated list using the repository's .clang-format.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

# Read-only checks must not create project-local __pycache__ files.
sys.dont_write_bytecode = True

from check_architecture import LAYER_ROOTS, SOURCE_SUFFIXES


FORMATTER_VERSION = "19.1.7"
ADAPTER_FILES = (
    "scripts/patches/pn532-1.3.4/read_detected.cpp",
    "src/main.cpp",
    "src/ProfileTool/main.cpp",
    "src/ProfileTool/CommandFrames.h",
    "src/ProfileTool/CommandFrames.cpp",
    "lib/PsaPeerProofCrypto/PsaPeerProofCrypto.h",
    "lib/PsaPeerProofCrypto/PsaPeerProofCrypto.cpp",
    "firmware/idf/main/app_main.cpp",
    "src/ProfileTool/Preview.h",
    "src/ProfileTool/Preview.cpp",
    "src/ProfileTool/Play.h",
    "src/ProfileTool/Play.cpp",
    "src/ProfileTool/Activation.h",
    "src/ProfileTool/Activation.cpp",
    "src/ProfileTool/TemporaryProfileFile.h",
    "src/ProfileTool/TemporaryProfileFile.cpp",
    "src/ProfileTool/ProfileReceive.h",
    "src/ProfileTool/ProfileReceive.cpp",
    "src/ProfileTool/PeerRegistryRecord.h",
    "src/ProfileTool/PeerRegistryRecord.cpp",
    "src/ProfileTool/IdentityRecord.h",
    "src/ProfileTool/IdentityRecord.cpp",
    "src/ProfileTool/ProfileEnvelope.h",
    "src/ProfileTool/ProfileEnvelope.cpp",
    "src/RuntimeProbe/main.cpp",
    "src/ProfileRuntimeProbe/main.cpp",
    "src/GattsAbiProbe/main.cpp",
    "lib/GattsWriteAdmission/GattsWriteAdmission.h",
    "lib/GattsWriteAdmission/GattsWriteAdmission.cpp",
    "lib/GattsWriteAdmission/SdkGattsWrite.h",
    "lib/GattsWriteAdmission/SdkGattsLifecycle.h",
    "lib/GattsWriteAdmission/SdkGattSourceAbi.h",
    "lib/GattsWriteAdmission/SdkPeerSecurityAbi.h",
    "lib/GattsWriteAdmission/Esp32GattsSource.h",
    "lib/GattsWriteAdmission/Esp32GattsSource.cpp",
    "lib/GattsWriteAdmission/Esp32GattsPeerAuth.cpp",
    "lib/GattsWriteAdmission/Esp32GattsOutput.cpp",
    "lib/FreeRtosPeerAuthRuntime/FreeRtosPeerAuthRuntime.h",
    "lib/FreeRtosPeerAuthRuntime/FreeRtosPeerAuthRuntime.cpp",
    "lib/GattsWriteAdmission/GattsServiceDefinition.h",
    "lib/GattsWriteAdmission/GattsServiceEvent.h",
    "lib/GattsWriteAdmission/SdkGattsService.h",
    "lib/GattsWriteAdmission/SdkGattsService.cpp",
    "lib/GattsWriteAdmission/Esp32GattsService.h",
    "lib/GattsWriteAdmission/Esp32GattsService.cpp",
    "lib/GattsWriteAdmission/FreeRtosGattsConnection.h",
    "lib/GattsWriteAdmission/FreeRtosGattsConnection.cpp",
    "lib/GattsWriteAdmission/FreeRtosGattsResponseQueue.h",
    "lib/GattsWriteAdmission/FreeRtosGattsResponseQueue.cpp",
    "lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.h",
    "lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp",
    "lib/FreeRtosProfileFrameQueue/FreeRtosProfileFrameQueue.h",
    "lib/FreeRtosProfileFrameQueue/FreeRtosProfileFrameQueue.cpp",
    "lib/FreeRtosCommandFrameQueue/FreeRtosCommandFrameQueue.h",
    "lib/FreeRtosCommandFrameQueue/FreeRtosCommandFrameQueue.cpp",
    "lib/FreeRtosPeerChallengeIssueRuntime/FreeRtosPeerChallengeIssueRuntime.h",
    "lib/FreeRtosPeerChallengeIssueRuntime/FreeRtosPeerChallengeIssueRuntime.cpp",
    "lib/FreeRtosPeerChallengeReadRuntime/FreeRtosPeerChallengeReadRuntime.h",
    "lib/FreeRtosPeerChallengeReadRuntime/FreeRtosPeerChallengeReadRuntime.cpp",
    "lib/FreeRtosPeerProofQueue/FreeRtosPeerProofQueue.h",
    "lib/FreeRtosPeerProofQueue/FreeRtosPeerProofQueue.cpp",
    "lib/Esp32PeerProofCrypto/Esp32PeerProofNonce.h",
    "lib/Esp32PeerProofCrypto/Esp32PeerProofNonce.cpp",
    "lib/Esp32PeerProofCrypto/Esp32PeerProofCrypto.h",
    "lib/Esp32PeerProofCrypto/Esp32PeerProofCrypto.cpp",
    "lib/FreeRtosPeerProofGrantRuntime/FreeRtosPeerProofGrantRuntime.h",
    "lib/FreeRtosPeerProofGrantRuntime/FreeRtosPeerProofGrantRuntime.cpp",
    "lib/FreeRtosPeerRegistryRuntime/FreeRtosPeerRegistryRuntime.h",
    "lib/FreeRtosPeerRegistryRuntime/FreeRtosPeerRegistryRuntime.cpp",
    "lib/Esp32PeerRegistryStore/Esp32PeerRegistryStore.h",
    "lib/Esp32PeerRegistryStore/Esp32PeerRegistryStore.cpp",
    "lib/Esp32DeviceIdentityStore/Esp32DeviceIdentityStore.h",
    "lib/Esp32DeviceIdentityStore/Esp32DeviceIdentityStore.cpp",
    "lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.h",
    "lib/Esp32DeviceIdentityStore/Esp32StartupIdentityEntropy.cpp",
    "lib/Esp32ProfileStore/Esp32ProfileStore.h",
    "lib/Esp32ProfileStore/Esp32ProfileStore.cpp",
    "lib/Esp32ProfileStore/Esp32ProfileStagingStore.h",
    "lib/Esp32ProfileStore/Esp32ProfileStagingStore.cpp",
    "lib/FreeRtosProfileChannel/FreeRtosProfileChannel.h",
    "lib/FreeRtosProfileChannel/FreeRtosProfileChannel.cpp",
    "lib/CompanionStatusStore/CompanionStatusStore.h",
    "lib/CompanionStatusStore/CompanionStatusStore.cpp",
    "lib/DFMinniHandler/DFMinniHandler.h",
    "lib/DFMinniHandler/DFMinniHandler.cpp",
    "lib/EspNowRuntime/EspNowRuntime.h",
    "lib/EspNowRuntime/EspNowRuntime.cpp",
    "lib/SerialGameDiagnostics/SerialGameDiagnostics.h",
    "lib/SerialGameDiagnostics/SerialGameDiagnostics.cpp",
    "lib/LedFx/GameLightEffects.h",
    "lib/LedFx/GameLightEffects.cpp",
    "lib/LedFx/burst.h",
    "lib/LedFx/burst.cpp",
    "lib/LedFx/ripple.h",
    "lib/LedFx/ripple.cpp",
    "lib/MCPHandler/MCPHandler.h",
    "lib/MCPHandler/MCPHandler.cpp",
    "lib/Keymatrixwrapper/Keymatrix.h",
    "lib/Keymatrixwrapper/Keymatrix.cpp",
    "lib/NFCReader/NFCReader.h",
    "lib/NFCReader/NFCReader.cpp",
)
TEST_ROOTS = (
    "test/test_application_loop",
    "test/test_card_uid_copy",
    "test/test_crystal_input_batch",
    "test/test_led_effects",
    "test/test_matrix_configuration",
    "test/test_companion_protocol",
    "test/test_configured_control",
    "test/test_device_logic",
    "test/test_game_application",
    "test/test_game_history",
    "test/test_led_runtime",
    "test/test_machine_lifecycle",
    "test/test_mcp_adapter",
)


def collect_files(root: Path) -> list[Path]:
    if not (root / "platformio.ini").is_file() or not (root / ".clang-format").is_file():
        raise ValueError(f"{root}: requires platformio.ini and .clang-format; pass --root")
    files: set[Path] = set()
    for prefix, _ in LAYER_ROOTS:
        directory = root / prefix
        files.update(path.resolve() for path in directory.rglob("*")
                     if path.is_file() and path.suffix.casefold() in SOURCE_SUFFIXES)
    for relative in ADAPTER_FILES:
        path = root / relative
        if not path.is_file():
            raise ValueError(f"format baseline file is missing: {relative}")
        files.add(path.resolve())
    for relative in TEST_ROOTS:
        directory = root / relative
        if not directory.is_dir():
            raise ValueError(f"format baseline test directory is missing: {relative}")
        files.update(path.resolve() for path in directory.rglob("*")
                     if path.is_file() and path.suffix.casefold() in SOURCE_SUFFIXES)
    # Validate the complete set before the formatter may modify the first file.
    for path in files:
        if not path.is_relative_to(root):
            raise ValueError(f"format baseline file leaves the project root: {path}")
    if not files:
        raise ValueError("no source files found in the reviewed format baseline")
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def find_formatter(requested: str | None) -> Path:
    if requested:
        candidate = Path(requested).expanduser()
        if candidate.is_file():
            return candidate.resolve()
        found = shutil.which(requested)
    else:
        found = shutil.which("clang-format")
    if not found:
        raise ValueError("clang-format not found; install clang-format==19.1.7 or pass --formatter")
    return Path(found).resolve()


def verify_version(formatter: Path) -> None:
    result = subprocess.run([str(formatter), "--version"], capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=30)
    version = re.search(r"clang-format version (\d+\.\d+\.\d+)", result.stdout + result.stderr)
    if result.returncode or version is None:
        raise ValueError(f"cannot read clang-format version: {(result.stdout + result.stderr).strip()}")
    if version.group(1) != FORMATTER_VERSION:
        raise ValueError(f"clang-format {FORMATTER_VERSION} required; found {version.group(1)}")


def check(root: Path, formatter: Path, files: list[Path], fix: bool) -> int:
    flags = [f"--style=file:{root / '.clang-format'}", "--Werror", "--fail-on-incomplete-format"]
    flags.extend(["-i"] if fix else ["--dry-run", "--ferror-limit=5"])
    failures = 0
    for path in files:
        relative = path.relative_to(root).as_posix()
        command = [str(formatter), *flags, str(path)]
        try:
            result = subprocess.run(command, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=60)
        except subprocess.TimeoutExpired:
            failures += 1
            print(f"FAIL {relative}: formatter exceeded 60 seconds", file=sys.stderr)
            continue
        if result.returncode:
            failures += 1
            print(f"FAIL {relative}: formatter exit code {result.returncode}", file=sys.stderr)
            if result.stdout:
                print(result.stdout.rstrip(), file=sys.stderr)
            if result.stderr:
                print(result.stderr.rstrip(), file=sys.stderr)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="PlatformIO project root (default: this script's repository)")
    parser.add_argument("--formatter", help="clang-format executable; defaults to PATH")
    parser.add_argument("--fix", action="store_true", help="format only the reviewed baseline in place")
    parser.add_argument("--list", action="store_true", help="list the reviewed baseline and exit without changes")
    arguments = parser.parse_args()
    try:
        root = arguments.root.expanduser().resolve()
        files = collect_files(root)
        if arguments.list:
            for path in files:
                print(path.relative_to(root).as_posix())
            return 0
        formatter = find_formatter(arguments.formatter)
        verify_version(formatter)
        failures = check(root, formatter, files, arguments.fix)
    except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"Format check could not run: {error}", file=sys.stderr)
        return 2
    operation = "formatting" if arguments.fix else "format check"
    if failures:
        print(f"Baseline {operation} failed: {failures}/{len(files)} files.", file=sys.stderr)
        return 1
    print(f"Baseline {operation} passed: {len(files)} files; clang-format {FORMATTER_VERSION}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
