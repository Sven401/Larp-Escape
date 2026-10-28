# Belege der Kristallsiegel-Lichtversuche vom 20.10.2026

[Bericht und Grenzen](../../../kristallsiegel-live-results-2026-10-20.md).
Dieser Ordner enthält ausgewählte Bilder, Herkunftshashes und die tatsächlich
verwendeten Auswertungsquellen. Vollständige Rohbilder und Geräteprotokolle liegen
im privaten Laufverzeichnis `private/kristallsiegel-20261020` des ursprünglichen
Projektcheckouts. Flashdaten gehören nicht in diesen Belegordner.

Mit der vorbereiteten Kamera-Pythonumgebung (OpenCV 4.13, NumPy):

```text
python analyze_digital.py --raw PRIVATE_RUN --output NEW_DIGITAL_RESULT.json
python collect_live_evidence.py --raw PRIVATE_RUN --output NEW_EVIDENCE_DIR
```

`derive_index_map.py` ist die unveränderte Quelle der ursprünglichen
Koordinatenableitung. Sie erwartet Rohdateien neben dem Skript. Zur Wiederholung
das Skript in ein neues privates Auswertungsverzeichnis kopieren und dort
`index-map.json`, `mapping-rois.json`, `mapping-cal/capture.json`,
`mapping-cal/events.json` sowie das in `index-map-camera.json` unter
`provenance.context` bezeichnete Originalbild in derselben Unterstruktur
bereitstellen. Die Ausgabedateien `index-map-camera.json`/`.png` dürfen dort noch
nicht existieren. Rohdaten nicht hierfür in das Repository kopieren.

`digital-evidence.json` bestätigt digitale Transferprüfungen, keine Kameraabnahme.
`evidence.json` beschreibt Bildauswahl und Aufnahmegrenzen. Die Animation zeigt
einen gekürzten, auf 100-ms-Schritte abgetasteten Kameraauszug mit wiederholten
Bildern über Aufnahmelücken. Die PNGs enthalten keine Farbkorrektur; Zuschnitt,
Skalierung, Kachelanordnung und Beschriftungen sind in den Quellen nachvollziehbar.
