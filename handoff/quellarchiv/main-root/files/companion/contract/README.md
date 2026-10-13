# Companion-Verträge und Referenzfixtures

Die native Android-App verwendet den v2-Erwartungsstand unter
[firmware-v2/](firmware-v2/SNAPSHOT.md). Android-Build und Protokolltests
beginnen bei [android/README.md](../android/README.md).

## Firmware v2

`firmware-v2/` ist ein unveränderter, datierter Quellsnapshot aus Firmwarecommit
`c9654c3c1365f4ada2dd5b6ce82bc5ac9373459b`: acht Hostfixtures, drei Schemata
und die Firmware-README. Herkunft und Prüfsummen stehen in
[SNAPSHOT.md](firmware-v2/SNAPSHOT.md). Der Kotlin-FrameAssembler verwendet
vier dieser Fixtures in JVM-Tests; JS-Projektionen bleiben lesende Referenzen.

Vom Produkt-Repositoryroot aus:

```powershell
node companion/contract/scripts/verify-firmware-v2-snapshot.mjs
```

Die Integritätsprüfung bestätigt die Kopie, nicht das vollständige Schema oder
die Kompatibilität mit einem inzwischen jüngeren Firmware-Arbeitsbaum.
Vor Produktintegration aktuelle Quelle und kopierte Dateien abgleichen.
Verifiziertes Framing verleiht keine Identität, Rechte oder Ausführungsgarantie.

## Historische v0-Mockreferenz

`v0/` enthält device-status und gm-command-request/result für den
[Vite-Prototyp](../app/README.md). Felder wie bootId/runtimeGeneration/
configRevision/storageGeneration/requestId und die accepted-/Mockabläufe
sind keine kompatiblen Aliase für v2. Sie werden nicht als Android-Wirevertrag
weiterentwickelt oder ungeprüft übertragen.

```powershell
node companion/contract/scripts/validate-contract.js
```

Die v0-Fachtests bleiben für das Verständnis von Bedienung, Sperren und
Rückmeldungen erhalten. Produktanmeldung, Befehle und korrelierte Antworten
benötigen den kanonischen Firmwarepfad und dessen eigene Abnahme; offene
Abhängigkeiten stehen in [OPEN-DEPENDENCIES.md](../OPEN-DEPENDENCIES.md).