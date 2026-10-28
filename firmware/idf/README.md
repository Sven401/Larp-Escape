# Native ESP-IDF qualification

This is the first native build of the existing hardware-independent core and
real FreeRTOS channels. The entry point only creates channels: no Bluetooth,
GPIO, I2C, UART, Wi-Fi, NVS erase or game authorisation is started. It is a
qualification image, not a replacement for the installed box firmware.

Target: classic ESP32, 4 MiB flash. Candidate SDK: ESP-IDF v6.1, commit
`fff9895c82d744c7237be8847347bdd1b07c6643`; use its submodule/tool versions.
No Arduino component or third-party runtime dependency is included yet.
The existing partition CSV is referenced unchanged; this does not prove on-device
data compatibility. No OTA rollback claim: the table contains one application slot.

After installing/exporting that exact SDK, run from this directory:

```powershell
idf.py set-target esp32
idf.py build
```

Do not flash as part of build qualification. Device testing requires the existing
chip-specific recovery procedure and a separately authorised upload.

Host tests and the profile tool can now be built from the repository root with
CMake/Ninja, without PlatformIO or Arduino:

```powershell
cmake -S . -B private/host-build -G Ninja
cmake --build private/host-build
ctest --test-dir private/host-build --output-on-failure
```

Unity 2.6.1 is fetched at its full commit pin. An already checked-out copy can
be supplied with `-DFETCHCONTENT_SOURCE_DIR_UNITY=<path>`. configured-control
uses the existing deterministic SDK/RTOS models, not actual peer cryptography
or a Bluetooth stack. Do not confuse these tests with a new TLS/SMP proof.

For the real new P-256 backend, add this option to host configuration:

```powershell
cmake -S . -B private/host-psa -G Ninja -DLARP_PSA_IDF_PATH="$env:IDF_PATH"
cmake --build private/host-psa
ctest --test-dir private/host-psa --output-on-failure
```

This requires a current host compiler (locally verified with GCC 16.2 from
w64devkit 2.9.1; the old PlatformIO MinGW 5.1 lacks a needed Windows header).
Use a fresh build directory when changing compiler. The host-only forwarding
headers in `test/psa_host_compat` resolve two leftover Espressif include paths;
they contain no algorithms and do not modify the SDK. Firmware uses the normal
IDF port, not these host headers. The SDK source guard checks the pinned Git
root, recursive submodule commits and tracked modifications before either build.

Windows installation was exercised in a private workspace using the SDK's
`idf_tools.py install --targets=esp32` and `install-python-env --features=core`,
then `export.ps1`. Use short paths (or a short checkout) for deeply nested GCC
headers. A partially extracted tool can still report a version: verify that
`xtensa-esp-elf/bin/as.exe` exists below the toolchain root before trusting the
installation. Local workarounds and remaining product gates are in
`docs/migration/esp-idf-qualification-2026-10-28.md` at the repository root.

The SDK/toolchain and official CI image are pinned. Third-party firmware
components have not yet been introduced, so no Component Manager lockfile is
fabricated. A genuine manifest and resolved lockfile are required when they are.
