# Einrichtungsschlüsseldatei: technische Qualifikation

Status: **Dateiformat v1 bestätigt, Produktintegration und Abnahme offen**,
01.11.2026. Bestätigt ist die [separate kennwortgeschützte Datei pro Box](../ui-ux/SETUP-SCHLUESSEL-VORLAGE.md).
Container und konkrete Parameter wurden anschließend mit Antwort
„Geprüftes Dateiformat v1 übernehmen (empfohlen)“ am 01.11.2026 bestätigt;
maßgeblich ist das [bestätigte Profil v1](setup-key-file-profile-proposal.md).
Physischer Erstnachweis und produktive Rechtevergabe bleiben ausdrücklich
ausgenommen. Historische Versuchsabschnitte unten bewahren den damaligen
Auswahlstand; sie heben diese spätere Formatentscheidung nicht auf.
Hieraus entstehen keine produktiven Schlüssel,
Setupanker, Eventrechte oder datenerhaltenden Ersatzwege.

Root führt die Vorprüfung; unabhängige Source-/Beleggegenprüfungen sowie die
begrenzte tatsächliche Android14-Funktionsprobe sind unten belegt. Produktive
Einrichtung und vollständige Android-/UI-Abnahme fehlen. Diese Datei hält Versuche
und seine Grenzen fest, keine neue Freigabekette für gewöhnliche Spielleitung.

## Ausgeführter Standardcontainer-Versuch

PKCS #8 `EncryptedPrivateKeyInfo` ist ein standardisierter Behälter für
verschlüsseltes privates Schlüsselmaterial. PBES2 verbindet eine Passwort-KDF
mit einem ausgewählten Verschlüsselungsverfahren.
[RFC 5958 §3](https://www.rfc-editor.org/rfc/rfc5958.html#section-3),
[RFC 8018 §6.2](https://www.rfc-editor.org/rfc/rfc8018.html#section-6.2).

Ein ausdrücklich privater lokaler Kompatibilitätsversuch verwendet den öffentlich
bekannten P-256-Testscalar **1**, ein öffentliches Testkennwort, feste Test-Salt/IV
und keine echten Zugangsdaten. JDK 21.0.8: PBKDF2-HMAC-SHA256, 600000 Iterationen,
AES-256-CBC, Salt16/IV16; Ergebnis 186 DER-Bytes. **Fünf Hostprüfungen** bestanden:
erneuter Dateireimport mit neuer Parser-/Cipherinstanz, Parameterrücklesung,
exakte private Testschlüsselrepräsentation, frische ECDSA-Signatur gegen den
bekannten öffentlichen Testschlüssel und Ablehnung des falschen Testkennworts.
Zusätzlich laden **zwei unabhängige OpenSSL-basierte Pythonprüfungen** genau
diese DER-Datei, bestätigen Kurve/Testscalar und falsches Kennwort.

Beobachtete Providerfalle: `EncryptedPrivateKeyInfo` verwirft im verwendeten JDK
die direkt vom PBE-Cipher gelieferten Parameter wegen des Algorithmusnamens.
Explizite `AlgorithmParameters("PBES2")` mit derselben DER-Parameterrepräsentation
ermöglichen den Standardcontainer. Beim späteren Lesen zeigt das JDK wiederum
`PBEWithHmacSHA256AndAES_256` als Namen. Ein Name allein ist daher weder feste
Wire-Validierung noch Anlass für einen generischen Algorithmusfallback.

Die gemessenen 1857 ms umfassen diesen gesamten Hostversuch; sie qualifizieren
weder Androidlatenz noch eine Passwort-/Iterationspolicy. Versuchssource und
synthetische Datei liegen ignoriert unter `private/p1a7-setup-qualification/`.
Sie sind keine Produktimplementation oder dauerhaft veröffentlichte Fixture.

## Noch offene Auswahl und Nachweise

Der Versuch belegt Standardinteroperabilität auf dem genannten Host. CBC allein
liefert hier keinen zusätzlichen authentifizierten Dateicontainer; der Versuch
belegt keine Integritätsgarantie, Boxzuordnung oder sichere Wiederherstellung.
Ein Kandidat mit standardisiertem authentifiziertem Container wird gesondert
gegenübergestellt, bevor eine technische Auswahl erfolgt. Beispielsweise
beschreibt JWE passwortbasiertes Schlüsselwrapping und authentifizierte Inhalte;
daraus allein ist keine Produktwahl abzuleiten.
[RFC 7518 §4.8](https://www.rfc-editor.org/rfc/rfc7518.html#section-4.8),
[RFC 7516 §3.1](https://www.rfc-editor.org/rfc/rfc7516.html#section-3.1).

Ein anschließender ausdrücklich synthetischer Hostversuch hat nun auch den
standardisierten JWE-Aufbau mit `PBES2-HS256+A128KW` und `A256GCM` ausgeführt:
**sieben JDK-Prüfungen** für erneutes Öffnen, Unwrap/PKCS8, frische Signatur,
falsches Kennwort sowie manipulierten Ciphertext und geschützten Header.
Ein unabhängiger Python/OpenSSL-Pfad bestätigt UTF-8-PBKDF2, AES-KW, GCM-AAD und
den wiederhergestellten öffentlichen Testscalar 1. Das Testkennwort enthält
Umlaut und einen ergänzenden Unicodecodepoint; die 374-Byte-Datei trägt ausdrücklich
`qualification-only`. Salt, CEK und IV sind öffentliche feste Testwerte, keine
qualifizierte Produktentropie.

Dieser zweite Versuch ist ebenfalls **kein allgemeiner JWE-Parser**, keine
Passwort-/Iterationspolicy, Boxbindung, Android- oder Setupqualifikation.
Bei diesem ersten Versuch standen endgültiges Header-/Dateiprofil, unabhängige
fachliche Bewertung und tatsächliche Providerprüfungen noch aus; die späteren
begrenzten Kandidaten-/Androidnachweise folgen unten.

Die GM-App hat aktuell minSdk33. Die Androidreferenz nennt die verwendeten
PBKDF2-/PBE-Varianten ab API26; tatsächliches Verhalten des vorgesehenen Providers
war damit allein noch nicht getestet. Keinen Provider außerhalb des Android-Keystores
still festpinnen. Exportierbarer Einrichtungsschlüssel und nicht exportierbarer
D0-Ursprungsschlüssel bleiben getrennte Lebensdauern.
[SecretKeyFactory](https://developer.android.com/reference/javax/crypto/SecretKeyFactory),
[Android-Kryptografiehinweise](https://developer.android.com/privacy-and-security/cryptography).

Vor Implementierung/Integration sind insbesondere zu prüfen: exaktes begrenztes
Wireprofil und Header-/Box-/Keybindung, keine Algorithmuswahl aus fremden Daten,
Kennwortkodierung, Ressourcenlimits vor KDF, frische Entropie, vollständige
Fehler-/Manipulationsfälle, unabhängige Interoperabilität und tatsächlicher
Close/Reimport/Challenge durch den importierten Schlüssel. Herkunfts-/Boxprüfung
und physisches Erst-Enrollment bleiben eigene echte Vertrauensnachweise.

Die beiden ersten Hostversuche enthalten keine Geräteprüfung, Provisionierung,
Release oder main-Merge. Die spätere separat autorisierte Androidprobe steht unten.

## Unveröffentlichter begrenzter JWE-Dateikern

Auf Basis des [Profilvorschlags](setup-key-file-profile-proposal.md) ist der
interne `SetupKeyFileCandidate` im JVM-Protokollmodul vorbereitet. Er ist weder
in eine Oberfläche noch eine Boxeinrichtung eingebunden. Root implementiert;
`module_digest_review` führte die andere Security-Sourceprüfung aus. Betroffen
ist zunächst ausschließlich der JVM-Dateikern; Androidprovider, Dateidialog,
Worker-/UI-Lifecycle und tatsächliche Enrollmentstrecke bleiben eigene Nachweise.

Geprüfter Quell-SHA256 am 31.10.2026:
`aab3cebd900a04998a7324d26e12b9dca5cd94f8adc8a7bd9298c95e7028e907`.
Die Gegenprüfung fand im exakten Stand keinen konkreten Bypass der Dateigrenzen,
Authentisierung vor Privatimport oder Schlüsselrollentrennung. Sie führte keine
Laufzeittests aus und bestätigt keine Androidkompatibilität.

Eigene tatsächliche JDK21-Prüfung nach Ergänzung der Gegenfälle:
`:core:protocol:test --offline --no-daemon --console=plain`, **111 Tests,
0 Fehler, 0 übersprungen**; darin fünf Tests des Kandidaten. Der unabhängige
Python-Generator unter `core/protocol/src/test/resources/setup-key-candidate/`
verwendet `cryptography` 50.0.2, öffentlich bekannten Scalar 1, feste öffentliche
Testentropie und Testkennwörter. Zwei AES-KW/GCM-Dateien werden tatsächlich von
JCA geöffnet, einschließlich UTF-8-Kennwort und anders angeordnetem/eingerücktem
Originalheader. Sechs weitere Dateien prüfen authentisierte unzulässige
Scalar-/DER-/Paarfälle und escaped doppelte Headerfelder. Die feste Testentropie
ist niemals ein Produktgenerator.

Ein tatsächlich neuer JCA-Testschlüssel wird ausschließlich als verschlüsselte
Datei und öffentliche Metadaten zurückgegeben, danach neu importiert. Eingaben
und zurückgegebene öffentliche Arrays sind getrennte Kopien; doppeltes Schließen
bleibt geschlossen. Testprovider-Zähler belegen für alle **661 kürzeren Präfixe**
der 661-Byte-Referenzdatei und zusätzliche JSON-/UTF-8-Gegenfälle: keine KDF und
kein Privatimport. Veränderte KW-/IV-/Ciphertext-/Tagbytes führen trotz ausgeführter
KDF zu keinem Privatimport; die unveränderte Referenz importiert tatsächlich.
Unterbrechung unmittelbar nach KDF erzeugt keinen Besitzer. Während der KDF
mutierte ursprüngliche Eingabearrays ändern den bereits kopierten Import nicht.
Die Testprovider werden danach wieder entfernt; der Produktkern pinnt sie nicht.

Das prüft Thread-Unterbrechung an den vorhandenen Grenzen, **keine Coroutine-
oder Compose-Cancellation**. Ein späterer Worker muss verworfene oder verspätete
`Opened`-Ergebnisse zuverlässig schließen. Tatsächliches Android-PBKDF2/UTF-8,
der genaue `AESWrap`-Alias mit WRAP/UNWRAP, GCM und minimaler PKCS8-Import auf
API33+ waren in diesem JDK-Prüfstand ungeprüft; die spätere einzelne API34-Probe
steht unten. Weiterhin fehlen die echte Datei-Wiederauswahl
und eine neue Boxchallenge aus dem importierten Schlüsselbesitzer. Die interne
Paarprobe und erfolgreiches Öffnen verleihen keine Verwaltungs- oder GM-Rechte.

Dieser historische Softwareprüfstand begründete für sich noch keine verbindliche
Container-/KDF-Produktwahl. Die ausdrückliche Nutzerentscheidung folgte später;
Integrations- und physische Erstbereitstellungsfreigabe entstehen aus dem Test nicht.

## Begrenzte Android-13-Sourcegegenprobe · 01.11.2026

Die tatsächlichen offiziellen AOSP-Quellen am Tag `android-13.0.0_r1` wurden nur
lesend bezogen. In der
[AES-Providerzuordnung](https://android.googlesource.com/platform/external/bouncycastle/+/refs/tags/android-13.0.0_r1/repackaged/bcprov/src/main/java/com/android/org/bouncycastle/jcajce/provider/symmetric/AES.java)
ist `Cipher.AESWRAP` aktiv auf den RFC3394-Wrapadapter registriert. Die
[PBKDF2-Providerzuordnung](https://android.googlesource.com/platform/external/bouncycastle/+/refs/tags/android-13.0.0_r1/repackaged/bcprov/src/main/java/com/android/org/bouncycastle/jcajce/provider/symmetric/PBEPBKDF2.java)
ordnet `PBKDF2WithHmacSHA256` der UTF-8-Variante zu; diese verwendet ausdrücklich
`PKCS5S2_UTF8`. Die internen Klassen werden vom Kandidaten nicht direkt aufgerufen:
er verwendet ausschließlich die normalen JCA-Factories ohne Providerpinning.

Die lokal unveränderten Quellkopien liegen nur im ignorierten Prüfbereich
`private/p1a7-setup-qualification/android-13`. SHA256 der tatsächlichen Sourcebytes:
AES.java `b6c0f671147198812460981a9a9462743ea1602011131e7c0eef4251e94f5cee`,
PBEPBKDF2.java `9c412329ff41625a57d877c48c90911a33af0b0d00a9f47fda18c4e902463c3b`.
Dies ist ein Sourcebefund für genau diesen Tag, keine ausgeführte Android-JCA-
Prüfung, Aussage über OEM-/andere API-Provider oder Zusage künftiger Aliasse.
Der unveränderte tatsächliche Android-/UI-/Boxqualifikationsrest bleibt offen.

## Tatsächliche autorisierte Android-14-Probe · 01.11.2026

Nach konkreter Nutzerwahl „Android-Testgerät bereitstellen“ wurde die getrennte
Prüfapp auf dem bereitgestellten **moto g14, API34/Android14** installiert,
ausgeführt und anschließend entfernt. Vorher war ihr eigenes Paket
`de.larpescape.setupqualification` abwesend; nach erfolgreicher Deinstallation
wurde die Abwesenheit erneut tatsächlich abgefragt. Keine Produktapp oder Box
wurde dafür provisioniert oder verändert.

Die separat gebaute APK hat SHA256
`c584fd89eb5237397656d5285bb02676eca6f262a97de924695c2eedb95cb466`,
Größe 2627781 Byte, minSdk33/targetSdk35, ausschließlich Self-Instrumentation,
allowBackup false und keine Berechtigungen/Activity/Service/Receiver/native .so.
APK-v2-Signaturprüfung ist erfolgreich. Der ignorierte lokale Prüfbereich
`private/p1a7-setup-qualification/android-probe` erhält editierbare Probe-/Manifest-/
Buildquellen und Herkunft; die eigentlichen Produktapps bleiben unberührt.
Damals ausgeführter Probe.java-SHA256:
`9f64923f713a7941c4ae4d3b92d50b692be70a6f29bc1be9ff0b342548459ed8`.
Die tatsächlich kopierte Kandidatenquelle entspricht unverändert `aab3cebd…`,
das tatsächliche APK-Asset unverändert den Pythonvektoren `876cb326…`.
APK und exakte editierbare Probe-Vorversion sind inzwischen getrennt unter
`private/p1a7-setup-qualification/android-probe-apks` erhalten; diese alte Variante
wird nicht mit dem unten beschriebenen erweiterten Lauf verwechselt.

Tatsächlicher Terminalcode `INSTRUMENTATION_CODE: -1` und **2014 Checks**:
zwei positive/sechs negative unabhängige Dateien, alle 661 kürzeren Präfixe,
falsche Box/falsches Kennwort, idempotentes Schließen, tatsächliches Cache-
Dateischließen/Wiederimport der öffentlichen Scalar1-Referenz und ein neuer
ephemerer synthetischer Erzeugungs-/Öffnungsrundlauf. Nur die bekannte öffentliche
Referenz wurde temporär in den Probe-Appcache geschrieben und wieder gelöscht;
neu erzeugte Testschlüsseldatei und Privatmaterial wurden nicht gespeichert
oder ausgegeben. Fehlender Provider und unerwartet akzeptierte Negativfälle
führen ausdrücklich zum Fehlerterminal statt zu einem positiven Test.

Gemeldete **separate Default-Factorysamples vor init**: PBKDF2 und AESWrap `BC`,
EC und GCM `AndroidOpenSSL`. Das ist keine Attestation der privaten tatsächlichen
Kandidateninstanzen; JCA kann Provider erst bei init auswählen oder wechseln.
Die Funktionsprobe selbst wurde dagegen tatsächlich auf Android ausgeführt.

Originalbelege stehen in
`private/p1a7-setup-qualification/device-run-20261101-012525/`:
install.txt, instrumentation.txt, uninstall.txt und summary.json. Root führte
aus; `module_digest_review` prüfte Source, konkrete APK-/Eingabehashes, Manifest,
Signatur und gespeicherte Ergebnisse unabhängig nach. Er rechnete die 2014 Checks
aus Source/Fixtures nach und bestätigte den konsistenten Erfolgs-/Cleanupbeleg;
er führte selbst keine Geräteaktion aus. Die tatsächliche nachträgliche Paket-
Abwesenheitsabfrage ist im Summary festgehalten, nicht als eigener roher pm-Log.

Dies qualifiziert exakt diese synthetische Probe auf diesem API34-Gerät. Es ist
kein ausgeführter API33-/anderer OEM-/Entropie-/Latenz-/Heaplöschungsnachweis,
kein nativer Dokumentpicker-/Coroutine-/UI-Lifecycletest, keine neue Boxchallenge
und keine physische Erstbereitstellung. Die konkrete Container-/KDF-Wahl wurde
erst anschließend ausdrücklich bestätigt. Neue produktive Setup-/Event-/
GM-Vertrauensgrenzen bleiben weiterhin gesondert auszuarbeiten und freizugeben.

### Ergänzte tatsächliche Android-Authentisierungsfehlerpfade

Im selben freigegebenen isolierten Testauftrag wurden fünf weitere Gegenfälle
ergänzt: je ein Bit in KW, IV, Ciphertext und Tag sowie semantisch gleiche,
syntaktisch zulässige Whitespaceänderung des Original-AAD-Headers. Candidate,
Fixtures und Manifest/Berechtigungen bleiben bytegleich; im APK änderte sich
ausschließlich der Probe-Dex. `module_digest_review` prüfte diese begrenzte
Ergänzung und die konkreten neuen Artefakte vor Ausführung unabhängig.

Tatsächliche neue APK-SHA256:
`31bf76076c86241f5911a4d9a0528ea6e65d4f79b173e2666a232aeb4bdf2391`,
Größe 2629165 Byte, Probe.java
`b52d2fd9ab2051eedcd0a0a62ce52dc39eab1abc343cdcd35bbd340c4404aec6`.
Erneut auf demselben moto g14/API34 ausgeführt: **2029 Checks, Terminalcode −1**.
Alle fünf Fälle verlangen tatsächlich `FILE_OR_PASSWORD`; ein bloßes
`UNAVAILABLE` oder unerwartetes Akzeptieren besteht keinen Test.

Installation, erfolgreicher Lauf, erneute Deinstallation und die tatsächliche
nachfolgende Paketabwesenheit sind unter
`private/p1a7-setup-qualification/device-authfaults-20261101-014943` erhalten.
Zusätzlich zum Summary gibt es den eigenen leeren `pm-after.txt` und Exitcode0
in `pm-after-exit.txt`. Der andere Reviewer hat diese gespeicherten Belege,
Source-/APK-/Assethashes und den Erfolgs-/Fehlerpfad unabhängig nachgelesen;
keine eigene zweite Geräteaktion durch ihn. Die ausdrücklich begrenzte
Factorysample-/Geräte-/API-/UI-/Box-/Einrichtungszuordnung bleibt unverändert.

## Übernahme als Dateikern v1 und besitzender Dateikoordinator

Nach der ausdrücklichen Formatentscheidung vom 01.11.2026 heißt die Bibliothek
`SetupKeyFileV1` und ist für den Androidanschluss öffentlich zugänglich. Gegenüber
der exakt archivierten Candidatequelle `aab3cebd…` sind ausschließlich Objektname,
Sichtbarkeit und KDoc geändert; der gesamte kryptografische Body ab der
Objektdeklaration ist nach dieser Deklarationsumbenennung exakt identisch.
Die alte Quelle bleibt unter
`private/p1a7-setup-qualification/source-aab3cebd900a04998a7324d26e12b9dca5cd94f8adc8a7bd9298c95e7028e907/`
erhalten. Die bisherigen Androidläufe gelten für ihre konkreten alten APKs;
sie werden nicht als neue Ausführung dieser Bibliothek oder des folgenden
Koordinators ausgegeben.

`gamemaster/setup/SetupKeyFileWorker` verbindet den Dateikern mit frischen
Input-/Outputstreams auf einem IO-Dispatcher. Er kopiert Box und Kennwort vor
der ersten Suspension, schließt den tatsächlich gelesenen Stream vor dem
Privatimport und liest höchstens 4097 Bytes vor der Formatentscheidung. Ein
importierter Schlüssel bleibt vollständig innerhalb seines besitzenden
`useOpened`-Scopes und wird dort bei Erfolg, Exception und Coroutine-Abbruch
geschlossen. Ein nach Abbruch spät zurückkehrender JCA-Aufruf liefert keinen
Schlüssel mehr an den Consumer. Beliebige Provider-IO/KDF wird damit nicht
als sofort unterbrechbar oder zeitlich begrenzt ausgegeben.

Tatsächlich ausgeführter lokaler Gradlelauf:
`:gamemaster:testDebugUnitTest :core:protocol:test --offline --no-daemon --console=plain`.
**35 GM-Modultests, darunter neun neue Workerfälle, sowie 111 Protokollmodultests,
jeweils ohne Fehler oder übersprungene Fälle.** Die Protokolltests waren beim
abschließenden gemeinsamen Lauf unverändert aktuell; der zuvor tatsächlich
ausgeführte V1-Umbenennungslauf hatte dieselben 111 Tests neu ausgeführt.
Der erste GM-Testbuild scheiterte vor Ausführung an einem testseitigen
Providerkonstruktor mit Stringversion; die zum Android-Stub passende numerische
Version wurde korrigiert. Dieser Fehlversuch zählt nicht als positiver Lauf.

Die neun Workerfälle prüfen tatsächliche neue Erzeugung/Streamschluss/Wiederimport,
Consumerfehler, Abbruch im Consumer, die 4097-Byte-Grenze, Read-/Close- sowie
Write-/Flush-/Closefehler, Abbruch während eines blockierenden Reads nach dessen
Rückkehr und Box-/Kennwortmutation über die Suspension. Ein weiterer Gegenfall
hält echte JCA-KDF-Arbeit zurück, bricht den Coroutinejob ab und lässt das
Kryptoergebnis danach zurückkehren: genau ein tatsächlich importierter privater
Testschlüssel erhält den beobachteten Destroyaufruf und wird niemals an den
Consumer geliefert. Das beweist eine Besitzfreigabe, keine forensische
Provider-/Heaplöschung. Testprovider sind ausschließlich Testinstrumentierung;
die Implementierung pinnt weiterhin keinen Provider.

Geprüfte Quellen-SHA256:

- V1 `46a4b12bf02934964d31d3d671533fe1c0562db4d105a56af0cd2e6c4c4aad8a`.
- Protokolltests `b6a6bca148782c4b999efb3c91dc42a00517649b7b7d71fced31e78aefd1ec9e`.
- Worker `b2bb0b38a4004d248a1dfc9ab6d9f8d863d99b7b38fc3fe53f583be9b657fd33`.
- Workertests `3c93130f8af24d0db447fb86cd7a21252148ec685731f37acc922c5c5af80ee0`.

XML-Belege liegen unter den jeweiligen `build/test-results`-Verzeichnissen.
Source-/Testgegenprüfung durch `module_digest_review` ohne bestätigte Lücke;
anschließend bestätigte er unabhängig alle vier Quellenhashes, die neun
Workerfälle und die XML-Summen 35/111 ohne Fehler oder Skips. Er führte keinen
eigenen Test- oder Gerätelauf aus. Nativer
Dokumentpicker, gerenderte Oberfläche, Prozessneustart, neue Androidausführung,
frische Boxchallenge und physische Erstbereitstellung bleiben offen. Ein normaler
HMAC-Zugangsimport wird durch diesen Dateikern weder verändert noch privilegiert.

### Nachgelagerte Prüfung des veröffentlichten Commits

Commit `c183dd0f14fa23f183fd13dd87b81cfc26f819a9` enthält exakt neun
Setup-/Android-Dateien. Ein reines Git-Archiv unter
`private/setup-key-c183dd0f-source` schließt sämtliche lokalen neuen Owner-/
Ordering-/Wiringdateien aus. Dort wurden alle 27 Gradletasks neu ausgeführt:
GM 35 einschließlich Worker 9 und Protokoll 111 einschließlich fünf V1-Tests, jeweils
ohne Fehler oder Skips. Der andere Reviewer prüfte Dateiliste und Archiv-XML.

Anschließend unabhängig qualifiziert: **zwölf erfolgreiche PR-Jobs in sechs
Workflows**, alle Originalcheckoutlogs auf Prüfmerge
`b2ae7b93233ec1e0df1585979060b472dbc30c15`. Seine Eltern sind exakt Zielbasis
`c4f1f824c6274e9a8d7676212a2a96be4d4a77b9` und Head c183dd0f. Ereignis
pull_request, Attempt 1, Actions-App und abschließend unveränderte Refs bestätigt.

[Quality 41261595360](https://github.com/Sven401/Larp-Escape/actions/runs/41261595360)
enthält tatsächliche JVM-XML mit GM 35/Worker 9, Protokoll 111/fünf V1-Tests und
Dev-App 77 ohne Fehler oder Skips sowie vier erfolgreiche Firmwarevarianten.
Android-Artefakt 13064734474, SHA256
`acbdf2a30e1ecbd0b1181e988d95ef62c91c6b51c8a7ff479d85b29eaa1ddfdb`,
lokal `private/ci-c183-review`. Native 41261595352 bestätigt 87/87 CTests ohne
Skips, tatsächliche PSA-Komponenten und beide SDK-Images am Pin
`9a97f6c54ec638111ce55cd36581b3c192f15207` samt rekursiven Submodulen.
LED 41261595450, NFC 41261595667, ESP 41261595390 und Rollen 41261595453 sind
ebenfalls vollständig erfolgreich. Diese Prüfungen ersetzen keine neue
Android-Geräteausführung und qualifizieren nicht den nachfolgenden Ownercheckpoint.
PR #135 bleibt Draft, unmerged, ohne Approval-/Merge-/Geräteaktion.

Der nachfolgende Head `ee5c87a541088fe734559ea010355a8aac343527` enthält denselben
Dateikern und Worker und ist ebenfalls mit allen zwölf PR-Jobs unabhängig
qualifiziert. [Quality 41263905631](https://github.com/Sven401/Larp-Escape/actions/runs/41263905631)
bestätigt erneut tatsächliche XML mit GM 35/Worker 9, Protokoll 111/fünf V1-Tests
und Dev-App 77 ohne Fehler/Skips. Androidarchiv 13066970897, SHA256
`8dc9cf17bac549306b35fa4b95fa0e2b677a41e38330d86f6433f6f0f191c445`,
lokal `private/ci-ee5-review`. Den neuen nativen Ownerbeleg dokumentiert der
[eigene Bericht](../spielpakete/rules-progress-owner-v1.md); auch dieser Lauf
ersetzt keine neue Android-Geräteausführung oder physische Erstbereitstellung.
