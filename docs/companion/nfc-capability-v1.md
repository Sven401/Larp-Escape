# NFC reader–phone capability probe v1

**Status (2026-10-11):** development-only wire and evidence contract. It is
not NFC authentication, a BLE OOB pairing procedure, a Game Master role, or a
diagnostic grant. A bounded firmware codec and PN532 frame parser have host
tests; the Android source includes a debug-only HCE service. There is no
product PN532 caller, installed HCE test build, or observed phone–box NFC
exchange. The productive security goal and gates are in the canonical AI
Werkbank plan `plans/spielleiter-nfc-authentisierung.md`.

## Purpose and admission

The user starts a 30-second capability run in the Android app and holds that
phone to the original box. Android checks `FEATURE_NFC_HOST_CARD_EMULATION`
at runtime and arms one volatile 16-byte random run ID. The box may use the
sole PN532 reader only after an explicit, exclusive game/reader fence; a second
PN532 instance or concurrent game poll is forbidden. This probe may neither
write a tag nor change a key, role, GATT permission, game state or diagnostic
grant. Missing reader ownership, Bluetooth correlation, timeout, deactivation
or app process restart ends the run as incomplete. No automated retry of an
uncertain exchange.

The box learns the phone's run ID only from the validated SELECT response;
there is no current App→Box BLE start command that could supply it earlier.
It records that ID as an untrusted correlation value and the app later
compares it with its locally generated ID. A zero ID or an ID already consumed
in the current boot is rejected. Neither an echoed ID nor the separate box
challenge authenticates the phone.

The provisional AID `F04C4152504E4643` is for a local development build only.
It must be replaced by an appropriately registered product AID before
deployment. The HCE service uses Android category `other`, requires
`BIND_NFC_SERVICE`, and answers only while the user-started run is armed.
The service must never answer with a credential or a proof of identity.

## Exact APDU frames

All byte strings below are hexadecimal. Frame sizes are fixed; any different
CLA/INS/P1/P2, length, version, trailing byte or status closes the attempt.
The box generates its challenge from its active CSPRNG; the app run ID and
challenge are public correlation values, not secrets.

| Direction | Bytes | Length | Meaning |
| --- | --- | ---: | --- |
| Box → phone | `00 A4 04 00 08 F0 4C 41 52 50 4E 46 43 00` | 14 | SELECT provisional AID |
| Phone → box | `4E 43 01` + run ID (16 bytes) + `90 00` | 21 | Armed HCE test, version 1; box learns run ID here |
| Box → phone | `80 10 00 00 10` + box challenge (16 bytes) | 21 | One capability challenge |
| Phone → box | `4E 43 01` + exact box challenge (16 bytes) + `90 00` | 21 | Echo only, no authentication |

`6A 82` means this test application is unavailable; `69 85` means no armed
user run or an expired/consumed run; `6A 80` means a malformed APDU. Android
may return these status words but must not auto-arm from a reader request.
The box accepts only the two exact success frames on the same reader session.
It discards the phone UID and never treats an echo as possession of a key.

## Correlated evidence

The Android report stores schema `nfc_capability_v1`, app build, UTC start/end,
run ID, HCE feature/enabled observation, SELECT and challenge receipt status,
deactivation/timeout, and the public challenge with a bounded event count.
The firmware report stores the same run ID and challenge, chip-bound device
identity, boot ID, reader-owner ticket, PN532 init/SAM/SELECT/exchange status,
device times, and a distinct `probe_echo_ok` boolean. The app must receive
that device report over the same original BLE generation or mark device
correlation `UNVERIFIED`; a local HCE callback alone is not a box result.
Neither report records private keys, arbitrary APDU payloads, unrelated UIDs
or a positive GM/diagnostic result. Export/import preserves `PASS`, `FAIL`,
`UNCLEAR` and `NOT_TESTED` separately. A successful echo proves only that the
chosen phone and PN532 exchanged these bytes during this run.

## Implementation and checks

First add a pure bounded codec/state-machine fake on both sides with golden
frames above and negative cases: wrong AID, wrong length/version/status,
stale run, repeated command, changed challenge, timeout, deactivation, reader
or BLE generation replacement and a busy game reader. The Android HCE service
and PN532 adapter follow that verified contract. The PN532 adapter must audit
the actual Adafruit `inDataExchange` and ISO-DEP path in the pinned library;
the existing UID-only `NFCReader::readCard` does not establish APDU support.
Only after a chip-bound upload preflight may the user operate the test in the
app and export a completed report. The current box has intermittent PN532
readiness; failure must stay visible rather than restarting into an apparent
success. Productive authentication requires a separate reviewed cryptographic
contract and protected key/revocation path.

Android basis: [Host-based card emulation](https://developer.android.com/develop/connectivity/nfc/hce)
and [HostApduService](https://developer.android.com/reference/android/nfc/cardemulation/HostApduService).
Moto G14 `ZY22J7CSQD` reported `android.hardware.nfc.hce` and
`android.hardware.nfc.hcef` through a read-only `pm list features` on
2026-10-11; the debug HCE build has not been installed or run on that phone,
and no reader exchange has been observed.
