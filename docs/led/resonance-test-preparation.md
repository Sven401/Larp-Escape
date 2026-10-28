# Resonanzkreis: Umsetzung und Geräteprüfung

Historische Vergleichsbasis für den nächsten getrennten Prüflauf:
[ursprüngliche Standby-Ripples im Live-Referenzstand](legacy-wake-ripple-analysis.md).
Sie verwenden andere Puls-, Loslass- und Spielstartregeln als der neue Effekt.

Stand: **18. Oktober 2026**. Auftrag: erster neuer LED-Effekt aus dem gemeinsam
geschärften Entwurf. Basis: `fcb91216b335db77d07da702056d8b9b33969b9d`.
Für den endgültigen Prüfstand wurde der inzwischen integrierte Stand
`0b0843d5fce475d14705c42c21f4f8bf4314df30` ausschließlich in den eigenen
Arbeitsbranch übernommen; ESP-NOW/RSSI und Kameraprüfung bleiben erhalten.
**Implementiert; Softwareprüfstand siehe [Prüfbericht](resonance-test-results-2026-10-18.md).
Produktinstallation und gemeinsame Sichtabnahme sind offen.**
Die [isolierte Kameraprüfung](resonance-camera-results-2026-10-18.md) ergänzt
inzwischen optische Belege mit synthetischen Eingängen; reale Wake-Betätigung
und das neue BLE-App-/Firmwarepaar bleiben getrennt offen.

## Verhalten

In der echten Companion-App unter **ESP → LEDs → Resonanzkreis → Start**.
Auswahl allein sendet nichts. Ohne aktive Wake-Eingänge bleibt der gestartete
Effekt dunkel. Jeder aktive Eingang lässt seinen Bereich in Blau/Cyan
mit 2,4 Sekunden Periode atmen; die vier Phasen sind versetzt. Alle vier
gleichzeitig aktiven Eingänge lassen den ganzen Streifen innerhalb von
800 Millisekunden in ein gemeinsames ruhiges Leuchten übergehen. Dieses bleibt
nur so lange bestehen, wie alle vier Eingänge aktiv sind.

Loslassen nimmt den betreffenden Bereich beim nächsten Renderframe zurück;
die übrigen aktiven Bereiche atmen weiter. Ein ungültiger I2C-Lesestand macht
den gesamten Resonanzkreis dunkel. Nach einem wieder gültigen Stand folgt das
Licht den Eingängen erneut. Der Effekt bleibt dabei gestartet;
ein dunkler Frame ist kein Stop oder abgeschlossener Einmaleffekt.

Start ersetzt einen anderen Effekt und startet auch den Resonanzkreis bewusst
neu. Stop und erkannter Sitzungs-/Verbindungsverlust fordern wie bisher einen
vollständigen Schwarzframe an. Wiederverbinden und Eingangssignale starten
keinen Effekt. Die Ausgabegrenze von 32/255 pro Farbkanal bleibt bestehen;
der Resonanzkreis selbst verwendet höchstens 24/255.

Dies ist die vorhandene eigenständige Baugruppenbedienung, kein automatisch
gestarteter Standby-Effekt des Gesamtspiels. Legacy-Spielcontroller und
konfigurierbare Spielprofile wurden nicht umgestellt. Die spätere
Spielintegration muss den Wechsel bei vier aktiven Wake-Eingängen
mit der sichtbaren Zusammenführung abstimmen; sie ist hier nicht abgenommen.

## Eingang und räumliche Zuordnung

Quelle: bestehende Wake-HIGH-Auswertung in
[`WakeInputs.cpp`](../../firmware/led/src/WakeInputs.cpp), vorhandene
Wake-Effektursprünge in [`GameState.cpp`](../../lib/GameState/GameState.cpp).
Die Herkunft ist belegt, die Eignung der neuen Lichtbereiche am Gehäuse ist
noch zu prüfen. Die verschiebbaren App-Kacheln verändern keine Pins
oder LED-Zuordnung.

| Eingang | MCP 0x20 | bisheriger Ursprung (nullbasiert) | neuer LED-Bereich (nullbasiert) |
| --- | --- | ---: | --- |
| Wake 1 | B7 | 22 | 19–22 |
| Wake 2 | B6 | 2 | 0–6 |
| Wake 3 | B4 | 16 | 13–18 |
| Wake 4 | B3 | 10 | 7–12 |

Jede LED gehört dem nächstgelegenen Ursprung; bei gleichem Abstand gewinnt
die kleinere Wake-Nummer. Der Wake-Leser pollt nominal alle 20 ms, die
Lichtausgabe nominal alle 33 ms. Das ist keine gemessene Reaktionszeit.
Roh-HIGH bedeutet die bisherige Aktivinterpretation, keine neue elektrische
Qualifikation. Der [Wake-Prüfplan](../wake/test-preparation.md)
und der offene Chip-/B7-Nachweis gelten weiter.

## Passendes Prüf-Paar bereitstellen

Der [V2-Vertrag](ble-wire-v2.md#resonanzkreis-erweiterung--18-oktober-2026)
ergänzt Effekt-ID 4 und ein authentisiertes Unterstützungsbit. Neue App mit
alter V2-Firmware: bestehende Effekte bleiben bedienbar, Resonanzkreis-Start
ist mit Hinweis gesperrt. Alte App mit neuer Firmware: das früher reservierte
Bit wird abgewiesen; für die neue Firmware zuerst die neue App bereitstellen.
Die neue App erhält die Fähigkeit über Ergebnisnachrichten hinweg, bis die
Verbindung endet. Sie erfindet daraus keine frischen Wake-Werte.

Builds aus dem Repository-Stamm:

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python firmware/led/tests/run_camera_tests.py
python -m platformio run -d firmware/led -e larp-led-product -e larp-nfc-test -e larp-matrix-test -e larp-espnow-test -e larp-led-camera-lab -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:lintRelease :app:assembleDebug :app:assembleRelease --no-daemon
```

Für den ersten Sichtlauf die Standardvariante `larp-led-product` verwenden.
Die NFC-/Matrixvarianten sind getrennte Builds. Debug-APK:
`companion/android/app/build/outputs/apk/debug/app-debug.apk`.
Firmware: `.pio/led-product/larp-led-product/firmware.bin`.
Diese generierten Artefakte bleiben außerhalb Git.

Vor einer **gesondert beauftragten Installation** das tatsächlich installierte
App-/Firmwarepaar feststellen und den chipgebundenen Flash-Rückweg gemäß
[Wake-Gerätebericht](../wake/device-test-2026-10-15.md) am angeschlossenen Chip
abgleichen. Private Zugangsdaten und Flashabbilder gehören nicht ins Repository.
Diese Umsetzung führt weder Installation noch Geräteupload aus.

## Gemeinsamer Sichtlauf

Pro Fall Appmeldung, Wake-Anzeige und tatsächlich beobachtetes Licht getrennt
notieren. Eine bestätigte Startantwort beweist die Ausgabeübertragung, nicht
die Erkennbarkeit oder räumliche Richtigkeit des Lichts.

| Fall | Handlung | Erwartete Sichtbeobachtung |
| --- | --- | --- |
| 1 | Verbinden; Resonanzkreis nur auswählen; Wake betätigen | Kein Lichtstart |
| 2 | Alle Eingänge inaktiv; Start | Resonanzkreis aktiv gemeldet, Streifen dunkel |
| 3 | Wake 1, 2, 3, 4 einzeln mindestens 3 s halten und loslassen | Jeweils genau der passende Bereich atmet und erlischt beim Loslassen |
| 4 | Zwei, dann drei Eingänge halten | Nur deren Bereiche atmen; noch kein ruhiges Gesamtleuchten |
| 5 | Vierten Eingang hinzunehmen und mindestens 1 s halten | Alle Bereiche gehen gemeinsam in ruhiges Gesamtleuchten über |
| 6 | Einen Eingang loslassen, danach erneut hinzunehmen; für alle vier wiederholen | Sein Bereich wird dunkel; die anderen atmen. Erneutes vollständiges Halten führt wieder zusammen |
| 7 | Während Gesamtleuchten erneut Start drücken | Zusammenführung beginnt bewusst neu |
| 8 | Während Atmen und während Gesamtleuchten Stop drücken | Box bestätigt Aus nach Schwarzübertragung; gesamter Streifen aus |
| 9 | Während Resonanzkreis zu Funkeln/Ripple wechseln und starten | Neuer Effekt ersetzt den Resonanzkreis |
| 10 | Während Gesamtleuchten Verbindung beenden; erneut verbinden | Streifen aus; kein automatischer Neustart trotz aktiver Eingänge |

Lesefehler/ungültige Masken und Wiederkehr gültiger Eingänge sind zunächst
gezielte Hosttests. Für einen physisch beobachteten Fehlerfall erst einen
geeigneten getrennten Fehleraufbau vereinbaren; keine Leitungen am laufenden
Aufbau als Teil dieser Anleitung entfernen.

**Abnahme des Spielnutzens:** Nach kurzer Einführung sollen die Beteiligten
ohne App erkennen, welcher ihrer vier Bereiche noch fehlt, und das gemeinsame
Leuchten durch gleichzeitiges Halten erreichen. Beobachtete Verwechslungen,
schlecht sichtbare Bereiche oder zu kurze/zu lange Zusammenführung festhalten.
Eine grüne Softwareprüfung ersetzt diesen Versuch nicht.

## Ergebnisbogen (noch nicht ausgeführt)

- Datum, Boxidentität, Chip/Rückweg abgeglichen:
- Quellcommit und SHA-256 des tatsächlich installierten APK-/Firmwarepaars:
- Fall 1–10: bestanden / fehlgeschlagen / nicht geprüft, Beobachtung:
- Raumlicht und räumliche Zuordnung der vier Bereiche:
- Zusammenarbeit ohne App verständlich: ja / nein, Beobachtung:
- Offene Fehlerfälle und nächste konkrete Anpassung:
