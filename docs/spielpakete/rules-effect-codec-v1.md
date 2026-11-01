# Rules-Effektbelege v1

Status: Darstellungs-/Headercodec implementiert und unabhängig auf dem Host geprüft;
vollständiger tatsächlicher Effektbesitzer noch offen. 01.11.2026, Root,
Teil von [Draft #135](https://github.com/Sven401/Larp-Escape/pull/135).
Kein physischer Effekt, echte Beobachtung oder D1-Commit folgt aus diesem Codec.

Exakte deterministische CBOR-Map mit Integerlabels 0 bis 7:

| Label | Inhalt |
| --- | --- |
| 0 | Textdomain `larp.rules.effect.v1` |
| 1 | ursprüngliche Anwendungsoperation, nichtnull bstr16 |
| 2 | vollständig gebundener ursprünglicher Katalogdigest, bstr32 |
| 3 | nullbasierter Intentindex innerhalb des reservierten Envelopeprofils |
| 4 | ursprünglicher beanspruchender Besitzerboot, nichtnull bstr16 |
| 5 | ursprüngliche erwartete logische Fence |
| 6 | Phase: 1 Claim, 2 Result |
| 7 | Handlerresult: 0 None, 1 Returned, 2 Failed, 3 Unknown |

Fence ist genau das Zehnerarray aus dem [Rules-Codec](rules-progress-codec-v1.md):
`[progress,profile,trust,facts,ledger,resources,rights,capacity,lastIssued,activeTime]`,
sämtliche Werte uint64 ohne Kürzung. Der rohe Index ist kleiner als das
Envelope-Reservationsprofil von 32; der tatsächliche Owner muss zusätzlich
den kleineren konkreten Plan-/Intentbestand prüfen. Keine Actor-/GM-/
Signaturprojekte werden aus dieser Darstellung erfunden.

Claim erlaubt ausschließlich None; Result ausschließlich Returned/Failed/Unknown.
Keine unbekannten/fehlenden/duplizierten Felder, angehängten Bytes, Tags, indefinite
Container oder nichtminimalen Integer-/Längendarstellungen. Höchstens 3072 Bytes
im unveränderten Journalpayload. Der Decoder kopiert ausschließlich feste Werte,
keine Eingangsviews überleben. Überlappung wird vor Änderung abgewiesen; bei
validem Speicher werden Fehlerausgaben geleert. Ein altes Flag/Count nach einem
Alias-Frühfehler gilt niemals als neuer erfolgreicher Aufruf.

## Header und tatsächliche Phasen

`validateEffectRecord` prüft die exakte eingefrorene Reservation für diesen Index:
claimId/resultId nichtnull, unterschiedlich und keine Parentoperation. Der echte
ProgressRecord muss den passenden EffectClaim/EffectResult-Kind und genau diese
Operation-ID tragen; issueCount, firstIssued und lastIssued sind null. Physische
Journalrevision wird **nicht** mit logischer Fortschrittsrevision gleichgesetzt.

Der tatsächliche Owner muss zusätzlich die ursprüngliche Parent-Envelope,
Application-/Katalog-/Contextbindung, tatsächlichen Rawpayload-SHA, sämtliche
ursprünglichen Intentcauses, die vollständig eingefrorenen Requests und die
einmalige Claim→Resultfolge prüfen. Ein Index ersetzt oder kürzt keinen Beweis.
Die erwartete logische Fence und der Boot gehören zum tatsächlich ursprünglich
beanspruchten Versuch; ein verspätetes Result darf sie nicht auf heutige Werte
umdeuten. Ein Result ist kein neuer Rechte-/Geräteauftrag.

Vor lokaler Übergabe muss der Claim dauerhaft bestätigt und die aktuelle lokale
Zulässigkeit tatsächlich geprüft sein. Noch vorbereitete Intents ohne Claim
haben keine physische Wirkung. Nach Crash wird ein Claim ohne vollständiges
Result unknown, niemals automatisch erneut übergeben. Reserved Claim-/Result-
slots bleiben nach Application-Final gesperrt; kein späterer Plan stiehlt sie.

**HandlerReturned ist keine physische Beobachtung und kein D0 observed.**
Geräte-/Sensorbeobachtung benötigt einen eigenen tatsächlichen Nachweis.
Handlerfehler oder Unsicherheit rollen bestätigten fachlichen Fortschritt nicht
zurück; ein gespeicherter Erfolg allein bestätigt keine Mechanik.

## Tatsächlicher Prüfstand

Eigene feste Python-Byteorakel für fünf Frames und 1515 C++11/Wpedantic/Werror-
Prüfungen: sämtliche kürzeren Präfixe/Outputkapazitäten, exakte 64-Bit-Werte,
unkanonische Formen, Header-/Phasen-/Bereichsfehler, Alias-Frühfehler, Clearing
und Wiederverwendung. Eigene Implementierungstests sind keine unabhängige Freigabe.
Raw-SHA, echtes Journal, tatsächlicher Owner, Rechte, Ausgabehardware, native SDK
und Stromausfallwirkung sind mit diesen Tests nicht qualifiziert. Kein Upload,
Release, main-Merge oder Abschluss von #93.

Nachgelagerte unabhängige Gegenprüfung durch `quality_p1`: acht eigene vollständige
Python-Byteorakel, 46 fehlerhafte Frames und **3160 C++-Prüfungen** einschließlich
aller kürzeren Präfixe/Kapazitäten, 30 Phasen-/Handlerkombinationen, vollständiger
uint64-Fence-/Boot-/Parentwerte, Header-/Reservierungsfehler sowie Alias/Clearing/
Wiederverwendung. Keine verbleibende bestätigte P1/P2 im begrenzten Codecumfang.
Geprüfter Body-SHA256:
`da108eee46c91194646520ddb2600c6882c0fe066fb5e2258196d53da82066bf`;
Header `09f72004a76c116aba6f903dc9107b1b95242ca9fab8cc64b74d0401796ca975`.
[Eigene unabhängige Quellen und Reproduktion](../../test/fixtures/rules-effect-independent-review/README.md).
Auch diese Gegenprüfung behauptet keine tatsächliche Claimreihenfolge im Owner,
Speicherschreibung, Geräteausgabe oder physische Beobachtung.

Die [nachgelagerte native Qualifikation am Head0e9f7ad3](p1a7-checkpoint.md) führt
die 3160 Codecassertions tatsächlich erneut aus; vollständige CI87/87 und beide
SDK-Builds sind unabhängig zugeordnet. Diese getrennten Compile-/Laufzeitbelege
ersetzen weiterhin keinen tatsächlichen Effektowner oder physische Beobachtung.
