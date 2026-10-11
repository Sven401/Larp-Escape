# APP_MUTUAL development transcript fixture v1

**Status, 2026-10-11:** host-only, one-shot fixture format. This is deliberately
separate from the semantic [APP_MUTUAL_V1 contract](app-mutual-v1.md) and its
[GATT matrix](app-mutual-gatt-matrix-v1.md). It chooses no production cipher
suite, enrollment channel or NFC protocol. No GATT value carries this frame;
no box or phone key is generated, loaded, enrolled or verified. Its only end
state is `TranscriptCheckedClosed`, which means an untrusted input matched the
locally supplied fixture bytes. It is **not cryptographic proof** and cannot
publish authority, assign Game Master, loosen ATT permissions or grant
diagnosis. The future productive Game Master target remains NFC; this USB
development fixture must not obstruct flashing, debugging or rollback.

The frame is exactly 289 bytes, network byte order, with no extension or
trailing bytes. Every value is a test input. `AMD1` (bytes 0–3), version 1,
suite marker `FF` (**no selected suite**), ordered role bytes `42` (box) and
`50` (phone) start the frame. Its fixed fields follow:

| Offset | Length | Field |
| ---: | ---: | --- |
| 8 | 6 | box eFuse base MAC as fixture chip identity |
| 14 | 16 | stable box device label |
| 30 | 8 | current boot ID |
| 38 | 4 | original product-source registration token |
| 42 | 4 | SDK source generation |
| 46 | 1 | GATT interface |
| 47 | 6 | peer BLE address observed by original source |
| 53 | 4 | locally minted connection-birth token |
| 57 | 2 | current SDK connection handle |
| 59 | 16 | phone installation ID |
| 75 | 16 | box key ID |
| 91 | 65 | box uncompressed P-256 public-key bytes |
| 156 | 16 | phone key ID |
| 172 | 65 | phone uncompressed P-256 public-key bytes |
| 237 | 16 | box nonce |
| 253 | 16 | phone nonce |
| 269 | 4 | attempt number |
| 273 | 4 | box enrollment generation |
| 277 | 4 | phone enrollment generation |
| 281 | 4 | box revocation generation |
| 285 | 4 | phone revocation generation |

Decoder input must be exactly 289 bytes. It rejects the wrong magic, version,
suite marker, role order, empty identifiers/nonces and missing birth or attempt.
The public-key check is only the 65-byte uncompressed shape with at least one
nonzero coordinate byte; it does **not** establish a curve point or ownership.
The one-shot fake compares every byte to a locally provided fixture, closes on
any mismatch, and rejects a second frame. Changing the expected attempt,
nonce, key, boot or birth rejects a previous frame. Repeating the same fixture
in a fresh fake remains possible: there is no trusted randomness, persistent
replay ledger, signature, ECDH, KDF, Finished or protected key store here.

Run `platformio -c platformio.app-mutual-dev.ini test -e host-app-mutual-dev -v`
for the bounded codec and negative checks. No device run is implied. A real
APP_MUTUAL path would first need an independently reviewed suite and encoding,
trusted key/enrollment/rollback design, reachable encrypted pre-Finished GATT
channel, full role/ATT migration, and correlated user-run evidence. The NFC
security plan may instead replace this development fixture entirely.
