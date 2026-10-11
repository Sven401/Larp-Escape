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
| `btm_ble_int.h` | `2f864b04ecd34d04ac84ca2d33ca0806edf91732` | `a2c6c5e146fed250dec43614e502c04a47465e3733c68e7d3f18522a4e8ef452` |
| `bluedroid_user_config.h` | `be0f6bf33dff1326992ac649e0047896c40655b3` | `3dcd99ef49c3a4828dcf7f59732453b40576e8f175408d60fefbd0c4ccf607ce` |
| `bt_target.h` | `d4b9462359f8c8f4d01317f36dec2785c07da5b3` | `64e33d0928e03c4d1738851e40fb24b9fff291305ab43fd88740f99c5384ab08` |
| `bt_types.h` | `be803d50d22402f34257e88c03ecc131f159ba8d` | `b1a8d8c8d84155be65646f2cac35a1bd8abdd656f194c2309b58866999d319a0` |
| `bt_common.h` | `c2eff8777a105e114e6cf9716ba8c5562f563fba` | `3a96651561a198a05500fbd57152d30d8b2a530109608d523ec3b92093ac1fdc` |
| `btm_api.h` | `ff4fc233f85f3f595fd40b19e610d5a8e200477c` | `e1d307c962d21ab72aa6a2056b3234d0fd9f8c24a11a655a10698a9d9b69c84b` |
| `hcidefs.h` | `7b9cb75af670d590dda87b795ae2318b8ab1ab65` | `1954310b1228386b8ae8710910e3122084f954bbaf71328e371f30672484bde9` |

Upstream: [btm_int.h](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_int.h),
[btm_ble.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_ble.c),
[btm_dev.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/btm/btm_dev.c),
[hcimsgs.h](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/include/stack/hcimsgs.h),
[hciblecmds.c](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/stack/hcic/hciblecmds.c).
The test also pins the locally installed Arduino ESP32 `libbt.a` by SHA-256.
Matching file hashes do not prove that the upstream source was compiled into
that archive with identical configuration and structure offsets.

The added private headers are unchanged source files fetched from the same
official tag. `scripts/check_f1_record_layout.py` pins their SHA-256 hashes and
extracts the entire original record plus nested key/BLE and connection-parameter
declarations for Xtensa static assertions. It uses the installed Arduino SDK
configuration and compares selected instruction offsets with the pinned archive
through `scripts/check_f1_record_binary_path.py`. The test never reads bond data.
