# Produktstand · 31. Oktober 2026

Status: **bestätigtes Product-Owner-Update; Gesamtstatus Gelb**.
Quelle: Nutzerupdate vom 31.10.2026 und ausdrücklicher Einlagerungsauftrag
in Sitzung `01a1929f-81b8-7dd1-88e7-a9d86482027c`.
Abgleich: Integrationsstand `ec1c52084e286892ada6540159e4c8036a907d04`,
Gerätebericht am unten verlinkten Dokumentationscommit gelesen und PR-Metadaten
am 31.10.2026 über GitHub ausgelesen. Keine neuen Geräteversuche oder CI-Läufe
für diese Einlagerung durchgeführt.

Ein autonomer Zweistationen-Prototyp mit Livebedienung am Handy ist am Gerät
nachgewiesen. Der vollständige Weg vom selbst erstellten Spielpaket bis zum
kooperierenden Mehrboxspiel bleibt offen.

## Gerätebeleg: Implementierung 07750d4

Gemeinsam gerätegeprüfte App-/Firmwareimplementierung:
[`07750d44e5d9053acf1c0d1af9143087cf1bbf79`](https://github.com/Sven401/Larp-Escape/commit/07750d44e5d9053acf1c0d1af9143087cf1bbf79).
Der [Gerätebericht bei Dokumentationsstand 31d6cca](https://github.com/Sven401/Larp-Escape/blob/31d6ccabe38e719ab4b04f57f35b489351f20b1d/docs/led/s3-nearglow-live-integration-2026-10-31.md)
führt Installationshashes, getrennte Beobachtungen, Prüfungen und Grenzen.

- Larp Dev 0.3.3 zeigt beide ESP32-S3 getrennt mit Partnerfunk,
  Helligkeitsziel, LED-Ausgabe und Datenalter. Vier einstellbare
  Funkstärke-/Helligkeitspunkte werden je Station gespeichert und bestätigt.
  Sichtbare Abdunkelung und anschließende Aufhellung sind nutzerbestätigt.
- Beide Stationen laufen bei geschlossener App autonom weiter. Beim Abschalten
  von B erkennt A den fehlenden Partner und wird dunkel. Nach Wiederanschließen
  kehren Partnerfunk und Glut ohne Startbefehl zurück; die Kurve bleibt erhalten.
- Nach Behebung eines App-Absturzes und von Bluetooth-Pufferproblemen gelangen
  fünf neue Anmeldungen je Station und ein dreiminütiger Lauf mit je 351
  frischen, fehlerfreien Funkproben.

Das ist ein Kurzlauf mit zwei S3 und einem Moto G14. Entfernungskalibrierung,
Langzeitbetrieb, allgemeine automatische Handy-Wiederverbindung und vollständige
Spielintegration bleiben offen. Der Neustart mit erhaltener Kurve ersetzt keine
umfassende Stromausfallprüfung, insbesondere nicht während eines NVS-Commits.

## PR-Prüfstand: Head 31d6cca

[PR #119](https://github.com/Sven401/Larp-Escape/pull/119),
Head `31d6ccabe38e719ab4b04f57f35b489351f20b1d`, ist laut frisch gelesenen
PR-Metadaten offen, Entwurf und nicht integriert. Integration und gesonderte
Freigabe stehen aus. Der Head ist kein zusätzlicher Geräteprüfstand.

**Ausgelesen aus dem Product-Owner-Update:** zwölf Jobs in sechs PR-Workflows
erfolgreich. Die verlinkten Ergebnisse wurden für diese Ablage nicht erneut
ausgeführt oder vollständig über die Actions-API nachgeprüft:

| Nachweis | Vorhandener Lauf |
| --- | --- |
| Quality | [41215280924](https://github.com/Sven401/Larp-Escape/actions/runs/41215280924) |
| LED/Android | [41215280916](https://github.com/Sven401/Larp-Escape/actions/runs/41215280916) |
| Native Qualifikation | [41215280837](https://github.com/Sven401/Larp-Escape/actions/runs/41215280837) |
| NFC | [41215280852](https://github.com/Sven401/Larp-Escape/actions/runs/41215280852) |
| ESP-NOW | [41215280855](https://github.com/Sven401/Larp-Escape/actions/runs/41215280855) |
| Rollenregeln | [41215280843](https://github.com/Sven401/Larp-Escape/actions/runs/41215280843) |

Diese Ergebnisse ersetzen weder eine Prüfung eines späteren Heads/Prüfmerges
noch die ausstehende Integrations- oder Produktfreigabe.

## Produktpfad und nächste Arbeit

Paketvertrag, Rechnerreferenz und erster Paketleser sind integriert.
[Produktplan und Abnahmetore](../docs/veranstaltungsarchitektur-umsetzung.md)
und [Paketleser mit begrenztem Prüfversprechen](../docs/spielpakete/p1a1-json-reader.md)
bleiben die fachlichen Quellen. Als Nächstes folgen vollständige
Paket-/Modulprüfung und Regelplanung, danach Geräteausführung und GM-Installation.
Der Zweistationen-Prototyp liefert dafür einen konkreten Funk- und Bedienbeleg.

Ein belastbarer Freigabetermin bleibt offen. Das Freigabetor ist weiterhin ein
fachlich anderes drittes Spiel, das nach Einfrieren der Plattform allein durch
neue Spielpakete entsteht; siehe [SP-01](../docs/spielpakete-architektur.md).
