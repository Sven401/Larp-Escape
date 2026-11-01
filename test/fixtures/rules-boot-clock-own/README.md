# Native boot/clock lifecycle seams

Actual `Esp32RulesBootClock` and unchanged `Esp32StartupIdentityEntropy` sources,
strict C++11/O2/Werror, six separate processes: **192 assertions**. SDK functions
are explicit test stubs. No reset/test friend/private access is added to the
product singleton. Test repeatability creates new OS processes, never new
in-process boot domains.

Checks: exact enable/fill16/disable ordering, uninitialized/reentrant closure,
null entropy, negative initial/later time, permanent sub-millisecond regression,
shared IClock/IManualClock and unchanged boot copies, full signed timer width,
repeat begin without entropy or clock refresh, deleted copy/move/public ctor.

`run.py --compiler <g++>` or `--binary <exe>`. CMake runs the lifecycle target
on host/native, while **both actual SDK images independently compile the new
component against real SDK headers**. Host seam success is not a hardware RNG,
ADC/I2S/RF-conflict, cold/warm-reset, sleep, timing-quality or SMP qualification.

First call is at the start of both native `app_main` entry points, before any
explicit subsystem begin. Failure returns from startup. Those images remain
qualification probes: no NVS, roles, grants, Running, radio start or device
upload is introduced. Later owner construction must use this same service and
its copied boot; no active runtime coordinator is instantiated by this change.
