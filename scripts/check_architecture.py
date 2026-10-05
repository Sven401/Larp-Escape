#!/usr/bin/env python3
"""Check direct literal includes at the project's hardware-independent boundaries.

This is a small architecture guard, not a C++ preprocessor or a transitive include
graph. Conditional literal includes are checked too; macros and unresolved
 project includes fail rather than silently bypassing the check. Compiler and
 host tests remain responsible for types, symbols and indirect dependencies.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys
import tempfile
import unittest


LAYER_ROOTS = (
    ("include/GameDomain", "domain"),
    ("src/GameDomain", "domain"),
    ("include/GameConfiguration", "configuration"),
    ("src/GameConfiguration", "configuration"),
    ("include/GameExecution", "execution"),
    ("src/GameExecution", "execution"),
    ("include/GamePorts", "ports"),
    ("include/GameApplication", "application"),
    ("include/GameRuntime", "runtime"),
    ("include/Companion", "companion"),
    ("src/Companion", "companion"),
    ("lib/Machine", "machine"),
    ("lib/GameState", "application"),
    ("lib/GameApplication", "application"),
)
ALLOWED_DEPENDENCIES = {
    "domain": {"domain"},
    "configuration": {"domain", "configuration"},
    "execution": {"domain", "configuration", "ports", "execution"},
    "ports": {"domain", "ports"},
    "runtime": {"domain", "runtime"},
    "machine": {"machine"},
    "application": {"domain", "ports", "runtime", "machine", "application", "configuration", "execution"},
    "companion": {"domain", "application", "companion", "configuration"},
}
SOURCE_SUFFIXES = {".h", ".hpp", ".hh", ".hxx", ".c", ".cpp", ".cc", ".cxx", ".inl", ".ipp"}
PURE_LIBRARIES = {"machine", "gamestate", "gameapplication"}
HARDWARE_PARTS = {"freertos", "config", "fl"}
HARDWARE_HEADERS = {"wire.h", "spi.h", "bounce2.h", "esp.h", "config.h"}
HARDWARE_PREFIXES = (
    "arduino", "fastled", "wifi", "esp_", "freertos", "adafruit", "dfplayer", "softwareserial",
)
COMMENT_OR_STRING = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)
INCLUDE = re.compile(r"^[\t ]*#[\t ]*include\b[\t ]*(?P<argument>[^\n]*)", re.MULTILINE)
HEADER = re.compile(r'^[\t ]*(?:<(?P<angle>[^>]+)>|"(?P<quote>[^"]+)")')


@dataclass(frozen=True)
class Violation:
    path: str
    line: int
    reason: str

    def __str__(self) -> str:
        return f"{self.path}:{self.line}: {self.reason}"


def without_comments(source: str) -> str:
    def preserve_lines(match: re.Match[str]) -> str:
        token = match.group(0)
        if not token.startswith(("//", "/*")):
            return token
        return "".join("\n" if character == "\n" else " " for character in token)

    return COMMENT_OR_STRING.sub(preserve_lines, source)


def layer_of(path: Path, root: Path) -> str | None:
    try:
        relative = path.relative_to(root).as_posix().casefold()
    except ValueError:
        return None
    for prefix, layer in LAYER_ROOTS:
        if relative.startswith(prefix.casefold() + "/"):
            return layer
    return None


def resolve_include(target: str, source: Path, search_roots: list[Path]) -> Path | None:
    for directory in [source.parent, *search_roots]:
        candidate = (directory / target).resolve()
        if candidate.is_file():
            return candidate
    return None


def inspect(root: Path) -> tuple[list[Violation], int, int]:
    root = root.resolve()
    if not (root / "platformio.ini").is_file():
        raise ValueError(f"{root}: no platformio.ini; pass the project directory with --root")

    libraries = sorted((root / "lib").iterdir()) if (root / "lib").is_dir() else []
    library_roots = [path for path in libraries if path.is_dir()]
    adapters = [path for path in library_roots if path.name.casefold() not in PURE_LIBRARIES]
    adapter_parts = {path.name.casefold() for path in adapters}
    adapter_headers = {
        header.name.casefold()
        for adapter in adapters
        for header in adapter.rglob("*")
        if header.is_file() and header.suffix.casefold() in SOURCE_SUFFIXES
    }
    search_roots = [root / "include", root / "src", *library_roots]
    violations: list[Violation] = []
    checked_files = 0
    checked_includes = 0

    for prefix, source_layer in LAYER_ROOTS:
        directory = root / prefix
        if not directory.is_dir():
            continue
        for source in sorted(directory.rglob("*")):
            if not source.is_file() or source.suffix.casefold() not in SOURCE_SUFFIXES:
                continue
            checked_files += 1
            relative = source.relative_to(root).as_posix()
            content = without_comments(source.read_text(encoding="utf-8-sig"))
            for directive in INCLUDE.finditer(content):
                checked_includes += 1
                line = content.count("\n", 0, directive.start()) + 1
                header = HEADER.match(directive.group("argument"))
                if header is None:
                    violations.append(Violation(relative, line, "non-literal #include cannot be checked"))
                    continue
                target = (header.group("angle") or header.group("quote")).replace("\\", "/")
                parts = target.casefold().split("/")
                filename = parts[-1]
                if (set(parts) & (HARDWARE_PARTS | adapter_parts) or
                        filename in HARDWARE_HEADERS or filename.startswith(HARDWARE_PREFIXES)):
                    violations.append(Violation(relative, line, f"hardware include '{target}' in {source_layer}"))
                    continue

                dependency = resolve_include(target, source, search_roots)
                if dependency is None:
                    if filename in adapter_headers:
                        reason = f"hardware adapter include '{target}' in {source_layer}"
                    elif header.group("quote") is not None:
                        reason = f"unresolved project include '{target}'; add an explicit architecture rule"
                    else:
                        # Unresolved angle includes may be standard/portable external
                        # headers. Known hardware names are rejected above.
                        continue
                    violations.append(Violation(relative, line, reason))
                    continue

                dependency_layer = layer_of(dependency, root)
                if dependency_layer is None:
                    reason = f"include '{target}' leaves the reviewed pure layers"
                elif dependency_layer not in ALLOWED_DEPENDENCIES[source_layer]:
                    reason = f"forbidden direction {source_layer} -> {dependency_layer} via '{target}'"
                else:
                    continue
                violations.append(Violation(relative, line, reason))

    if checked_files == 0:
        raise ValueError(f"{root}: no source files found in the reviewed pure layers")
    return violations, checked_files, checked_includes


def self_test() -> bool:
    class GuardFixtures(unittest.TestCase):
        def check_fixture(self, files: dict[str, str]) -> list[Violation]:
            with tempfile.TemporaryDirectory(prefix="larp-architecture-") as temporary:
                root = Path(temporary)
                (root / "platformio.ini").write_text("; architecture fixture\n", encoding="utf-8")
                for name, content in files.items():
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(content, encoding="utf-8")
                return inspect(root)[0]

        def test_accepts_reviewed_directions_and_standard_headers(self) -> None:
            failures = self.check_fixture({
                "include/GameDomain/Types.h": "#include <array>\n",
                "include/GamePorts/IClock.h": '#include "GameDomain/Types.h"\n',
                "include/GameApplication/Status.h": '#include "GameDomain/Types.h"\n',
                "include/Companion/Message.h": '#include "GameApplication/Status.h"\n',
                "lib/Machine/State.h": "#include <functional>\n",
                "lib/GameApplication/Controller.h": '#include "GamePorts/IClock.h"\n#include "State.h"\n',
            })
            self.assertEqual([], failures)

        def test_hardware_names_report_exact_source_line(self) -> None:
            failures = self.check_fixture({"include/GameDomain/Types.h": "\n#include <Arduino.h>\n"})
            self.assertEqual(1, len(failures))
            self.assertEqual("include/GameDomain/Types.h", failures[0].path)
            self.assertEqual(2, failures[0].line)
            self.assertIn("hardware include", failures[0].reason)

        def test_named_hardware_families_are_rejected_without_installed_libraries(self) -> None:
            for header in ("Arduino.h", "FastLED.h", "WiFi.h", "esp_now.h", "Config.h",
                           "freertos/FreeRTOS.h", "fl/fx/twinklefox.h", "DFPlayerMini_Fast.h",
                           "Adafruit_PN532.h"):
                with self.subTest(header=header):
                    failures = self.check_fixture({"include/GameDomain/Types.h": f"#include <{header}>\n"})
                    self.assertEqual(1, len(failures))
                    self.assertIn("hardware include", failures[0].reason)

        def test_domain_cannot_include_a_port_through_a_relative_path(self) -> None:
            failures = self.check_fixture({
                "include/GameDomain/Types.h": '#include "../GamePorts/IClock.h"\n',
                "include/GamePorts/IClock.h": "#pragma once\n",
            })
            self.assertEqual(1, len(failures))
            self.assertIn("domain -> ports", failures[0].reason)

        def test_commented_includes_are_ignored_and_line_numbers_are_preserved(self) -> None:
            failures = self.check_fixture({"include/GameDomain/Types.h":
                '// #include <Arduino.h>\n/*\n#include <WiFi.h>\n*/\n#include <freertos/task.h>\n'})
            self.assertEqual(1, len(failures))
            self.assertEqual(5, failures[0].line)

        def test_adapter_files_cannot_hide_behind_a_generic_filename(self) -> None:
            failures = self.check_fixture({
                "include/GameDomain/Types.h": '#include "../../lib/Sensor/Generic.h"\n',
                "lib/Sensor/Generic.h": "#pragma once\n",
            })
            self.assertEqual(1, len(failures))

        def test_macros_and_unclassified_project_headers_fail_explicitly(self) -> None:
            failures = self.check_fixture({
                "include/GameDomain/Types.h": '#include HEADER_MACRO\n#include "Common/Other.h"\n',
                "include/Common/Other.h": "#pragma once\n",
            })
            self.assertEqual(2, len(failures))
            self.assertIn("non-literal", failures[0].reason)
            self.assertIn("reviewed pure layers", failures[1].reason)

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(GuardFixtures))
    return result.wasSuccessful()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="PlatformIO project root (default: this script's repository)")
    parser.add_argument("--self-test", action="store_true", help="run isolated stdlib fixtures")
    arguments = parser.parse_args()
    if arguments.self_test:
        return 0 if self_test() else 1
    try:
        violations, files, includes = inspect(arguments.root)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Architecture guard could not run: {error}", file=sys.stderr)
        return 2
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        print(f"Architecture guard failed: {len(violations)} violation(s).", file=sys.stderr)
        return 1
    print(f"Architecture guard passed: {files} files, {includes} direct includes checked.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
