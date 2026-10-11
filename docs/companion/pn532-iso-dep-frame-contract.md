# PN532 ISO-DEP frame boundary (host-only, 2026-10-11)

This is a bounded parsing contract for the future NFC capability path. It is
not wired into the product reader and does not establish working NFC hardware,
an Android NFC exchange, a Spielleiter role, or diagnostic authorization.

`GameRuntime/Pn532IsoDepFrame.h` accepts only a complete, unpadded PN532
normal response frame (`00 00 FF LEN LCS D5 RESP ... DCS 00`), at most 64 bytes.
The transport must pass the actual byte count including checksum and postamble.
InListPassiveTarget must be the response to `MaxTg=1, BrTy=0` (106 kbps Type A):
`D5 4B NbTg=1 Tg=1 SENS_RES[2] SEL_RES NFCIDLength NFCID1 ATS`.
NFCID1 is 4, 7, or 10 bytes; the ISO-DEP bit in SEL_RES is set; ATS includes
its TL byte and at least T0, with TL equal to the complete remaining ATS size.
`NbTg=0` is an explicit no-target result and never retains a previous target.

InDataExchange must be `D5 41 Status APDU-response` with `Status=00`, exactly
the response length expected by the caller (currently 2 or 21 bytes), and an
output buffer large enough for it. A two-byte response is only an accepted
frame length; the APDU state machine separately checks SW1/SW2. A nonzero
status, chaining flag, invalid checksum, missing postamble, extra bytes,
truncation, or stale reader ticket clears the fake's selected target.
The fake's ticket lifetime is bounded to at most 30 seconds.

The pinned Adafruit PN532 1.3.4 `inDataExchange()` reads a fixed packet buffer
and silently shortens a response to `*responseLength`; it cannot provide this
exact-frame guarantee as-is. A future adapter must prove how it obtains the
actual PN532 byte count and must revalidate the full frame before copying data.
Do not infer this from a successful call to the current library API.

Sources: [NXP PN532 User Manual UM0701-02](https://www.nxp.com/docs/en/user-guide/141520.pdf),
[NXP AN10609 PN532 C106 application note, Type A ISO-DEP example](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/nfc/4394/1/PN532C106_Application%2BNote_v1.2.pdf?inline=true),
and the locally pinned `Adafruit PN532` 1.3.4 source at
`.pio/libdeps/az-delivery-devkit-v4-single-app-capacity/Adafruit PN532/Adafruit_PN532.cpp`
(`inListPassiveTarget`, `inDataExchange`).

Focused proof: `platformio test -c platformio.pn532-frame.ini -e
host-pn532-iso-dep-frame` (4 host cases passed, 2026-10-11). No COM3 access,
upload, RF trial, Android user action, or electrical test was performed.
