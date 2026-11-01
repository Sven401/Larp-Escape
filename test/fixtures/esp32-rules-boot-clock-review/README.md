# Independent ESP32 Rules boot-clock audit

Actual Esp32RulesBootClock.cpp, existing Esp32StartupIdentityEntropy.cpp and
Companion/Identity.cpp are compiled and linked, not replaced by test classes.
Only three narrow SDK headers/functions are stubbed: enable/disable startup
entropy, fill random bytes, and signed64 esp_timer_get_time. Strict C++11/O2/
Wall/Wextra/Wpedantic/Werror: **15 isolated fresh-process cases /426 assertions**.
Compile-time checks additionally reject public construction/copy/move/assignment
and verify both actual IClock/IManualClock inheritance interfaces.

Own test surface only: test/fixtures/esp32-rules-boot-clock-review.
No product changes, private-field access, test friend, reset hook or alternate
clock implementation. Each fresh startup uses a NEW process; a running process
cannot reconstruct the singleton. Repeated instance/begin calls preserve the
same16-byte boot and acquire no new entropy or timestamp.

Cases prove exact enable -> one nonnull16-byte fill -> disable -> timer call
order; all16 bytes copied, no lazy initialization from boot/read/now, all-zero
and unwritten/null Identity failure, negative initial/sample time, permanent
failure clearing and no reroll, equal timestamps, exact common boot/ms domain,
zero initial time, >32-bit milliseconds and INT64_MAX microseconds. Even a
microsecond regression inside the SAME exposed millisecond permanently closes
both ports. Cross-port regression cannot bypass the shared fence. Reentry at
fill/disable/initial timer/later sample leaves nested calls closed and does not
corrupt the outer operation or reacquire entropy. A last-byte-only nonzero label
is representationally valid; this deliberately makes no entropy-quality claim.

The SDK random API returns void: no nonexistent error status is invented. Its
modeled zero/unwritten output is rejected by the ACTUAL Identity::valid; entropy
is already disabled on both that path and the later negative-clock failure.
Public APIs take references, not nullable pointers; no invalid C++ null reference
is constructed to fake a fault test. The random stub checks the actual passed
buffer is nonnull and exactly16 bytes. Exceptions/crashes inside the SDK are
not modeled as ordinary return failures.

Source-read integration check: both SDK app_main call beginEarlyStartup before
their runtime begin/queue/proof operations and return on failure. The BLE source
constructor only retains its ingress reference. This is SOURCE ORDER evidence,
not execution of app_main or proof of RF/ADC/I2S/entropy behavior. No hardware
measurement, sleeping/deep-sleep time guarantee, SMP/ISR/thread-safety, startup
resource exclusion, RNG quality, persistent identity, actor rights, NVS, D0 key,
Running transition or device deployment follows. Supported profile is the
explicit exclusive early startup and subsequent one serialized worker.

Reviewed freeze:

- Esp32RulesBootClock.h: c756660a4cb07df48506d033cd2e0a4e01b31a28171facfde9d93579f434c4c2
- Esp32RulesBootClock.cpp: 37d401d5a2c1c89e8967db052abebe6724fc4b96be67651acdfdd5ab0d4bc134
- existing Esp32StartupIdentityEntropy.cpp: 12e25f8d772cdfb122abc506d439c211154653d3e74ee6277862fbf39b669c3a

```powershell
python -B test/fixtures/esp32-rules-boot-clock-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

Own build directory private/esp32-rules-boot-clock-review (parents created for
fresh archives). --binary /absolute/path/to/review skips compilation and runs
all15 separate processes on either host. Link main.cpp with the three actual
sources listed above, include our stubs FIRST, then include/, lib/Esp32RulesBootClock,
lib/Esp32DeviceIdentityStore. No PSA/crypto backend or platform macro required.
Real pinned-SDK compilation of both images remains a separate mandatory gate.

Owned six files: main.cpp, run.py, README.md, stubs/esp_timer.h,
stubs/esp_random.h, stubs/bootloader_random.h. Local/Timer application-correlation
policy remains a separate open contract question and is not fixed by this clock.
