# Firmware-v2-Snapshot fuer die App

**Status:** unveraenderter, lesender Quell-Snapshot; keine App-/Transportabnahme
**Quelle:** Firmware-Worktree `codex/svensbranch-regression`, Commit `c9654c3c1365f4ada2dd5b6ce82bc5ac9373459b`, `docs/companion/`
**Kopiert und geprueft:** 7. Oktober 2026

Die acht Dateien unter `fixtures/` wurden aus `docs/companion/fixtures/v2/` kopiert, die drei Schemata unter `schemas/` aus `docs/companion/`. Auch die Firmware-README wurde unveraendert kopiert. SHA-256 wurde fuer jede lokale Datei mit der Quelle verglichen; alle 12 Paare stimmten ueberein. Dieser Snapshot ist keine neue App-Vertragsversion und aendert `companion/contract/v0` nicht.

| Datei | SHA-256 |
| --- | --- |
| `schemas/status-v2.schema.json` | `A70208FD7BF8B95CEC8278B0DF9960210D6EABE18DDCCFAF3A381BC11BD19366` |
| `schemas/game-command-v2.schema.json` | `3E3A987BA473672CCA00D1034D248A0FD7D7631FA0395B3195483C91B33CF197` |
| `schemas/game-reply-v2.schema.json` | `87BC5536A700E26AF5E83BF696714B57A9C531A64D3190A1D506348F46EAC5D0` |
| `fixtures/status-ready.json` | `EB14EC710ADF2D7922B6F986F20378994965E30A446F9BC5670D65E4F9C680BC` |
| `fixtures/status-running.json` | `AB6D2440DB9336C58222654AEC2397E8EDEFB77599625B44667B7F6E34E08AFF` |
| `fixtures/command-start.json` | `A92CD55AD68816CB467DFBA34D64EEE288269DB34959A3DE1CDB6A897AC5FB9F` |
| `fixtures/command-stale-state.json` | `196C9BDE9619D465EAA7996B640AE61F16C6011EB0322B036CBA83A2F479BAE1` |
| `fixtures/reply-executed.json` | `A1FED9AC2EC24B1C96AA7C5027571EB5589F38922269A4BF1DB95740286C8098` |
| `fixtures/reply-duplicate.json` | `FECBCEF0743DF546227CD21BAC32775F29451F197A07C8B33DF420C2CE9FA253` |
| `fixtures/reply-conflict.json` | `9D3EBD02B1A4596A8C41A5E5B721A7217097B243F41E3ECDB0C648C879B6B0CC` |
| `fixtures/reply-stale-state.json` | `139CDA5DF6334A04186FF89B8B09801592D19B93FFCBECF09C0A5902EE41E76A` |
| `fixtures/README.md` | `AE464CD47508CFF0D2BE239677AEE4184BEBA06E767D301A2D88CC992EDCD9B6` |

Die Firmware-Dokumentation bezeichnet diese Fixtures als aus dem Hostpfad erzeugt und dort bytegenau geprueft. Diese Aussage gehoert zum Firmwarestand, nicht zu einer von der App erneut ausgefuehrten Firmwarepruefung. Die App prueft ihre Projektion separat gegen die lokalen Kopien. Identitaet, Anmeldung, Replyrouting, Funk, Profiltransfer und Android sind weiterhin offen. Eine sichtbare Lease oder `game.control.core` sind keine Schreibberechtigung.
