# Original ESP-IDF v4.4.6 BLE source pins

These two unmodified files were obtained from the official
`espressif/esp-idf` Git tag `v4.4.6`, path
`components/bt/host/bluedroid/stack/btm/`. The original license headers are
retained. They are test inputs; the build does not replace or patch the Arduino
SDK. The Git blob IDs and SHA-256 values are:

| File | Git blob | SHA-256 |
| --- | --- | --- |
| `btm_int.h` | `57854e0a915dc3b08187d1f1b63a9704aa49bf0f` | `002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3` |
| `btm_ble.c` | `c236d5d8aaf72f894d8977b9e900512e6d236c75` | `2a1805a0643666d671d24ca5a1e5c5072a509aa88fb53140b513c3161d5a7f65` |

Upstream: [btm_int.h](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_int.h),
[btm_ble.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_ble.c).
The test also pins the locally installed Arduino ESP32 `libbt.a` by SHA-256.
Matching file hashes do not prove that the upstream source was compiled into
that archive with identical configuration and structure offsets.
