# Android NFC capability probe v1 (development only)

Status 2026-10-11: the debug APK exposes a separate 30-second, explicitly
started HCE echo probe on the ESP screen. The release manifest contains no HCE
service or provisional AID. The run is process-local and disarmed by default;
process death, Stop, timeout, and HCE deactivation end it. The service answers
the exact v1 SELECT/challenge APDUs only, with a public run ID and public
challenge. The UI labels every local observation `UNVERIFIED` until a matching
box report is received through the original BLE generation. It cannot confer
GM role, diagnostic grant, identity, or device success.

No device installation or NFC exchange is claimed here. A bounded private
journal now stores app build, UTC times, observed HCE feature/NFC state, the
public run ID/challenge and at most twelve named events. Restart closes an
interrupted arm as `APP_RESTARTED`, never re-arms it. An explicit button exports
a frozen JSON snapshot; local assessment is `UNCLEAR` and device correlation
is `UNVERIFIED`. Matching a box report through the original BLE generation is
still required before a user-led end-to-end result. The PN532 reader/APDU path is also
not verified on the original box. BLE generation replacement cannot yet close
this separate probe because the reader correlation path is not installed. AID
registration and a production security
contract remain separate gates.

Official basis checked 2026-10-11: [Android HCE overview](https://developer.android.com/develop/connectivity/nfc/hce)
and [HostApduService](https://developer.android.com/reference/android/nfc/cardemulation/HostApduService).
The official guidance calls for a system-bound exported service, an `other`
category AID group, a feature check, and immediate nonblocking APDU responses;
the implementation uses those. The project basis is
`C:/Users/seide/Projects/Larp-Escape-AI/docs/companion/offizielle-grundlagen.md`.

Checks: `:core:protocol:test --tests de.larpescape.protocol.NfcCapabilityV1Test`,
`:app:testDebugUnitTest --tests de.larpescape.companion.data.nfc.NfcCapabilityEvidenceJournalV1Test`,
`:app:assembleDebug`, and `:app:processReleaseManifest` passed offline with
JDK 21 and the installed Android SDK. Merged manifests were checked: the
HCE declaration appears only in debug.
