# Herkunft der Stopdiagnose

18.10.2026, Sitzung `01a150c8-48be-7362-acef-143e74396303`.
Auftrag: vorhandenen Näheglut-Stop-/Dunkelbefund klären.
Projektbasis `7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f`, Aufnahmequelle `e26bd93`.

`audit.py` ist eine neue, eigenständige Offline-Diagnose. Sie liest die
vorhandenen CAL-/ROI-Dateien, `walk-analysis.json`, `capture.json`, `camera.json`
und ausgewählte Original-PNGs aus walk02/outage01 sowie die Goldauswertung.
Originalauswerter und genaue Belegfingerabdrücke stehen im
[Versuchsbericht](../../../espnow/naeheglut-result-2026-10-18.md).
`result.json` enthält die reproduzierten Kriterien, zusätzliche Randstatistik
und Eingabe-/Quellhashes. Kein Gerätezugriff; kein Ersatz eines bestehenden
Abnahme-Assessors und keine nachträgliche Abnahmekorrektur.

Ausgeführt mit Python 3.12, OpenCV 4.13.0 und NumPy aus dem bestehenden
Kameralabor. 650 ausgewählte Bildhashes geprüft, Originalbasis und sämtliche
drei Stopkriterien reproduziert, 386 schwarze TX-Ereignisse kontrolliert.
Randpixel werden aus dem bestehenden Aufnahmeradius gewählt, alle CAL-Pixel
ausgeschlossen. Ergebnisse sind explorativ; keine kontrollierte Dunkelfläche.

Aufruf und Grenzen: [Diagnose](../../naeheglut-stop-diagnose-2026-10-18.md).
Andere Kamera-/ROI-Layouts und neue Aufnahmen wurden nicht erprobt. Kein
Aufstieg zu einem allgemeinen Werkzeug/Skill und keine neue Dauerregel.
