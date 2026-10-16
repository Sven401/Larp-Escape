# Erstbefüllung: NDEF-Inhalt durch neue Erkennung prüfen

Stand: 16. Oktober 2026. Ausgangsbefund: [physischer NTAG213-Rundlauf](physical-tag-test-2026-10-16.md).

## Ursache und Beleggrenze

Beim leeren Testtag lieferte der Android-Schreibaufruf regulär zurück.
Der unmittelbar folgende `getNdefMessage()`-Aufruf lieferte keine Nachricht;
nach erneutem Vorhalten wurden alle 59 geschriebenen Bytes bestätigt.
Der Vergleich in `VerifiedNfcWrite` war korrekt: Ohne passende Rücklesung
durfte die App keinen Erfolg melden. Die Annahme, dass dieser Getter nach
jeder Erstbefüllung bereits eine neue Funklesung erzwingt, war dagegen falsch.

Im Android-14-AOSP-Pfad wird bei schon erkannter NDEF-Technologie die native
NDEF-Erkennung übersprungen. Die native Lesefunktion prüft die bei der
Erkennung gespeicherte Nachrichtengröße. Ist sie noch null, liefert sie eine
leere Antwort ohne erneute NDEF-Funklesung; der Schreibpfad aktualisiert
diese Variable nicht. Das erklärt den Übergang leer → beschrieben und die
erfolgreiche Lesung nach neuer Erkennung:
[öffentlicher Ndef-Getter](https://android.googlesource.com/platform/frameworks/base/+/refs/tags/android-14.0.0_r1/core/java/android/nfc/tech/Ndef.java#266),
[Abkürzung in checkNdef](https://android.googlesource.com/platform/packages/apps/Nfc/+/refs/tags/android-14.0.0_r1/nci/src/com/android/nfc/dhimpl/NativeNfcTag.java#335),
[native Größenprüfung](https://android.googlesource.com/platform/packages/apps/Nfc/+/refs/tags/android-14.0.0_r1/nci/jni/NativeNfcTag.cpp#269),
[NDEF-Parsing im Systemdienst](https://android.googlesource.com/platform/packages/apps/Nfc/+/refs/tags/android-14.0.0_r1/src/com/android/nfc/NfcService.java#2001).

Das echte moto g14 meldet Android 14, Build-Inkrement 54518607 und
`com.android.nfc` Version 14/34 unter `/system_ext/app/Nfc_st`.
Die lokale DEX-Prüfung dieser System-App bestätigt dieselbe Abkürzung in
`StNativeNfcTag.checkNdef`: vorhandene Technologie 6 führt ohne
`checkNdefWithStatus` zu `true`. `writeNdef` ruft `doWrite` ohne anschließende
NDEF-Erkennung auf. SHA-256 der untersuchten System-APK:
`ea7060eca5c16256632753de75cfe42f8210952699dac2d2d821ecd6609b1492`.
Die native ST-Bibliothek wurde nicht disassembliert; die vollständige native
Herstellerursache ist aus AOSP, dem identischen Java-Pfad und dem Geräteverhalten
abgeleitet. Die System-APK wird nicht veröffentlicht.

`Tag.rediscover()` und `Ndef.reconnect()` fehlen im öffentlichen SDK 35;
dies wurde mit `javap` am lokalen `android.jar` geprüft. Einfaches
`close()/connect()` ist keine verlässliche neue NDEF-Erkennung:
[RF-Neuauswahl statt normaler Discovery](https://android.googlesource.com/platform/packages/apps/Nfc/+/refs/tags/android-14.0.0_r1/nci/jni/NativeNfcManager.cpp#393).

## Korrektur

Nur die belegte Kombination **vorher wirklich leer, Schreibaufruf erfolgreich,
unmittelbare Rücklesung weiterhin null** erhält eine eigene ausstehende
Prüfphase. Ein leerer Rückgabewert bei vorher nichtleerem Tag, falsche Bytes
oder echte Ausnahmen bleiben unklar. Ein System ohne diesen Leerfall kann
weiterhin direkt nach bytegleicher Rücklesung Erfolg melden.

Die App schließt den alten Port und startet ReaderMode einmal über die
öffentlichen APIs neu. Nur ein neues Tagobjekt aus der neuen Callbackgeneration
darf die Prüfung fortsetzen. UID und vollständige Nachricht müssen dem
persistierten Schreibziel entsprechen. Der neue Auftrag liest ausschließlich;
es wird nicht nochmals geschrieben. Journal und Schreibsperre bleiben bis
zur Auflösung erhalten. Ohne neue Erkennung erscheint nach zwei Sekunden
der Hinweis, denselben Tag erneut anzuhalten.

Seitenwechsel und Pause verwerfen die automatische Prüfung, erhalten aber
den offenen Journaleintrag. Alte Callbacks dürfen den neuen Zustand nicht
überschreiben; ein alter Port darf nach neuer Erkennung nicht nochmals
verbunden oder geschlossen werden. Bei fehlgeschlagenem Readerneustart
fordert die App das erneute Öffnen der NFC-Seite an.

Die bisherige Diagnosebezeichnung `nfc_rf_read_complete bytes=0` war für
diesen Systempfad zu stark. Neue Logs heißen `nfc_ndef_read_result` und
weisen die neue Erkennung separat aus. Eine leere Systemantwort wird damit
nicht als tatsächlich erfolgte Funklesung bezeichnet.

## Prüfung

Geprüfter Code: `c0f76ee5123ef51252341b21c18a636abb6f6bc2`.
Am 16. Oktober 2026 lokal erfolgreich:

- `:core:protocol:test` und `:app:testDebugUnitTest`: 105 Tests,
  keine Fehler; darunter 18 neue Regressionstests für Erstbefüllung,
  Abbruch, Journal, falsche Rücklesung und Port-Lebensdauer.
- `:app:assembleDebug` und `:app:lintDebug`: erfolgreich;
  Lint meldet 0 Fehler und 3 bestehende Warnungen.
- Unabhängige Codeprüfung ohne konkrete Befunde. Die reale Neuerkennung
  ist ein separater Gerätenachweis und wird nicht aus Hosttests abgeleitet.

APK SHA-256:
`2d8030388dd201a658a424b3a31af443f37f9a0e5c7c5f0df8ea46023195d2a3`.
Signatur mit der zuvor installierten App abgeglichen; mit Datenerhalt
installiert und die installierte APK auf dem Handy bytegleich geprüft.
Die Box-Firmware wurde für diese Korrektur nicht verändert.

Die GitHub-Prüfungen auf genau diesem Code-Commit sind ebenfalls erfolgreich:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/38819322325),
[LED product](https://github.com/Sven401/Larp-Escape/actions/runs/38819322278),
[NFC probe](https://github.com/Sven401/Larp-Escape/actions/runs/38819322246).

## Gerätenachweis

**Bestanden am 16. Oktober 2026 um 01:00 Uhr, Europe/Berlin:** dieselbe
moto-g14-Installation, ein weiterer beschreibbarer NDEF-Tag mit 137 Byte
Android-Kapazität, vor dem erfolgreichen Schreiben frisch als leer gelesen.
Das genaue Chipmodell dieses weiteren Tags wurde nicht gesondert abgefragt.
Der Nutzer löste den Kontakt durch Abheben und Wiederanlegen aus und bestätigte
die Durchführung. Die automatische Prüfung wurde kontinuierlich protokolliert:

| Ortszeit | Ereignis |
| --- | --- |
| 01:00:43.824 | Vorher-Lesung: `null`, 0 Byte |
| 01:00:43.825–43.998 | Genau ein Schreibaufruf, regulär abgeschlossen |
| 01:00:44.000 | Unmittelbare Lesung erneut `null`; neue Erkennung erforderlich |
| 01:00:44.019 | `VERIFICATION_REQUIRED`, Journal bleibt erhalten |
| 01:00:44.021 | App startet ReaderMode neu |
| 01:00:44.265 | Neue Callbackgeneration erkennt den Tag |
| 01:00:44.307–44.308 | 59 Byte gelesen; gleiche UID und vollständiger Inhalt entsprechen dem Schreibziel |
| 01:01:00.758 und 01:01:01.712 | Spätere unabhängige Tagkontakte lesen erneut 59 Byte |

Die automatische Inhaltsbestätigung erfolgte 310 ms nach Abschluss des
Schreibaufrufs. Dazwischen gab es keinen weiteren Schreibaufruf, keinen
Timeout und keine Vordergrundunterbrechung. Die spätere direkt erfasste
App-Anzeige zeigte „Tagdaten gelesen · Prüfsumme bestätigt“, Kennwert
`20261015`, Flags `1` und Stationsbesuch `1, 1, 0`. Die kurzzeitige
Erfolgsanzeige unmittelbar nach der automatischen Prüfung wurde nicht als
Bild erfasst; der belegte aktuelle Code setzt sie bei diesem protokollierten
vollständigen Vergleich. Ein weiterer vollständiger Box-Rundlauf wurde für
diese App-Korrektur nicht durchgeführt.

Vor diesem erfolgreichen Lauf gab es einen getrennten Schreibversuch mit
`IOException` während des Schreibaufrufs; neue Erkennungen lasen anschließend
weiterhin einen leeren Tag. Die Ursache dieser Funk-/Schreibausnahme wurde
nicht abschließend bestimmt. Weitere vorbereitete Aufträge wurden durch
Drehen des Handys verworfen: Android protokollierte `CONFIG_ORIENTATION`
und den Neustart der Activity. Diese Fälle sind keine erfolgreichen Prüfungen
der Korrektur. Ein versuchter systemweiter NFC-Neustart öffnete den normalen
Android-Auswahldialog und wurde verworfen; er ist nicht mit dem Neustart des
ReaderMode innerhalb der App gleichzusetzen.

Für den erfolgreichen Lauf wurde das Hochformat testseitig fixiert. Die
vorherigen Dreheinstellungen wurden danach wiederhergestellt und ausgelesen;
NFC ist eingeschaltet. Die App ist wieder mit bestätigtem Zugang an der
unveränderten Wake-Box verbunden, mit Live-Pegeln und erhaltener Anordnung
**2 / 3 / 4 / 1**. Es wurde kein neuer Box-Flash durchgeführt.

Bereinigter Nachweis: [Ereignisse und Prüfsummen](evidence/first-write-readback-2026-10-16.json).
Lokale Rohbelege: `work/evidence/readback-fix-01/`, insbesondere
`nfc-live-events.txt`, `25-live-armed.xml`, `26-live-result.xml` und
`29-final-wake.xml`. Rohbelege mit Tagkennung bleiben lokal. Der Nachweis
gilt für dieses Handy und diesen Tag; weitere Android-Stacks und Tagfamilien
bleiben ungeprüft.
