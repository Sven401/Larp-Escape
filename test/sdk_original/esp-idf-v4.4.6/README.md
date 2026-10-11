# Original ESP-IDF v4.4.6 BLE source pins

These unmodified files were obtained from the official
`espressif/esp-idf` Git tag `v4.4.6`, under
`components/bt/host/bluedroid/stack/`. The original license headers are
retained. They are test inputs; the build does not replace or patch the Arduino
SDK. The Git blob IDs and SHA-256 values are:

| File | Git blob | SHA-256 |
| --- | --- | --- |
| `btm_int.h` | `57854e0a915dc3b08187d1f1b63a9704aa49bf0f` | `002a845683e602548f903926d21a25c6a628247582fd2bb52dde15923b53eff3` |
| `btm_ble.c` | `c236d5d8aaf72f894d8977b9e900512e6d236c75` | `2a1805a0643666d671d24ca5a1e5c5072a509aa88fb53140b513c3161d5a7f65` |
| `btm_dev.c` | `d83da09907f75a079dbe5b79d979f4876d903207` | `9212e1c3d62efaaf582e7f0bffd60487acf328ef8b0a2b40aeaf230e5fae39f7` |
| `hcimsgs.h` | `0b54ed2e1654109c3db3e060fd73525e6fdf73f9` | `31ba1c2c3e3371e2f22dff60629a78f3b4dd9a6f777af6d06bf14a37ed051956` |
| `hciblecmds.c` | `729134686e2d9463d30f75e78123c9f8262809c9` | `c3e357e85e6baf9ab53283fa4a36c9e03133172820bd60d5f99d34db7da301ee` |

Upstream: [btm_int.h](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_int.h),
[btm_ble.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_ble.c),
[btm_dev.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_dev.c),
[hcimsgs.h](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/include/stack/hcimsgs.h),
[hciblecmds.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/hcic/hciblecmds.c).
The test also pins the locally installed Arduino ESP32 `libbt.a` by SHA-256.
Matching file hashes do not prove that the upstream source was compiled into
that archive with identical configuration and structure offsets.
