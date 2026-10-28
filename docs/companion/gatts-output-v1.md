# Originalgebundener BLE-Rückkanal

Stand: **28.10.2026**. Status: optionaler Firmwareadapter mit Host- und
SDK-Buildnachweis; noch kein produktiv gestarteter Dienst und kein Funknachweis.
Arbeitsauftrag und Prüfstand: [BLE-/Rollen-/Profilintegration](../../handoff/BLE-ROLLEN-PROFILE-2026-10-28.md).

## Dienst und Bytes

Der [allgemeine Peer-Service](gatts-service-sdk-v1.md) kann nach dem
[Authentisierungsergebnis](peer-auth-result-v1.md) ein zusätzliches Attribut
registrieren: `407e6a1d-1cc2-43fd-856b-4aff85a20009`, READ,
`ESP_GATT_PERM_READ_ENC_MITM`, `ESP_GATT_RSP_BY_APP`, maximal **20 Byte**.
`PeerChallengeReadSetup.output=true` verlangt die vollständige Challenge-/Issue-/
Proof-/Result-Kette. Diese Variante reserviert 15 Handles und installiert den
Outputhandle auf dem Originalaktor, bevor der Dienst gestartet werden darf.
Die bisherigen Varianten mit 5/7/9/13 Handles behalten ihre Auswahl und Bytes.

Ein Lesevorgang liefert genau einen vollständigen Wert. Das sind unveränderte
[Spielantwortframes](reply-framing-v1.md), Statusframes oder eine vollständige
[Profilantwort vom Typ 8](profile-controls-v1.md). Der Transport sendet die
vorhandenen Formate, keine zusätzliche Fragmenthülle. Alle Sender müssen
`maximumFrameBytes=20` verwenden. Ein größerer Wert wird nicht gekürzt, sondern
als `Unavailable` abgewiesen. Dadurch passt jeder Wert auch bei ATT-MTU 23 in
eine einzelne Read Response. Ein größerer ausgehandelter MTU hebt diese
Outputgrenze nicht an; Profileingang und Proof behalten ihre eigenen Grenzen.

Es gibt keine Notify-/Indicate-Subscription und keinen CCC. Android liest auf
derselben bereits geprüften Verbindung. Offset ungleich null oder ReadBlob
(`isLong`) werden mit ATT-Status 7 abgewiesen und verbrauchen den Wert nicht.
Fehlende Antwortanforderung, falscher SDK-Aktor oder fremder Verbindungshandle
erzeugen keine Antwort auf diesem Pfad. Ein fehlendes Outputattribut ist eine
nicht unterstützte Fähigkeit, kein Anlass zur Protokollherabstufung.

## Eigentümer, Rechte und Ablauf

`Esp32GattsSource` implementiert `IReplyFrameBearer`. Genau ein separater
Transportworker kopiert höchstens 20 eigene Bytes in einen festen Mailboxplatz.
Vor jedem externen Port werden Callerbytes eingefroren. Der Platz hält zusätzlich
die vollständige ursprüngliche Proofbindung, Registrierung, Lifecycle,
GM-Authority, Kopierzeit und einen nicht umlaufenden lokalen Slottoken.
Bei belegtem aktuellem Platz bleibt der neue Senderwert mit `Busy` beim Sender;
Polling und Busy erneuern die Kopierzeit nicht. Überholte oder abgelaufene
Plätze dürfen durch einen frischen, aktuell autorisierten Wert ersetzt werden.

Nur der originale SDK-Aktor verarbeitet den Read. Er friert den ausgewählten
Platz ein und prüft aktuelle GM-Zuordnung, vollständige Authority und Lifecycle
vor und nach den echten SDK-Sicherheitsabfragen. SC/MITM/Bond und der aktuelle
16-Byte-Linkschlüssel bleiben erforderlich. Die öffentliche Attributpermission
allein reicht nicht, weil der Originalwrapper den Read selbst verarbeitet.
Ein Handshake ohne ausdrückliche GM-Zuordnung erhält keine private Ausgabe.
Verbindungsnummer, Name oder Baugruppen-HMAC begründen keine solche Rolle.

Kopierter Wert und ursprünglicher Read müssen beim tatsächlichen `GATTS_SendRsp`
jeweils jünger als **1000 ms** sein. Die letzte Altersprüfung folgt auf die
externen SDK-Sicherheitsports; danach folgt die letzte Authorityprüfung.
Unsigned Differenzen behandeln Wrap und zukünftige Zeiten geschlossen.
Keine SDK-/RTOS-Aufrufe unter den eigenen Spinlocks. Ein Guard verhindert
rekursive Doppelausgabe auf dem Originalaktor.

| Ergebnis | Bedeutung |
| --- | --- |
| Status 0, 1..20 Byte | Ein ausgewählter Wert wurde lokal an das SDK übergeben. |
| Status 0, 0 Byte | Für diesen ursprünglichen Read war keine Ausgabe vorhanden. Keine Auftragsbestätigung und keine Erneuerung eines Statussamples. |
| Status 7, 0 Byte | Offset/ReadBlob nicht unterstützt; der ausgewählte Platz bleibt erhalten. |
| Status 8, 0 Byte | Kein aktuelles Recht oder Sicherheitsnachweis; keine privaten Bytes. |
| Status 14, 0 Byte | Ausgewählter Wert oder ursprünglicher Read abgelaufen; kein alter Erfolg. |

Der SDK-Aufruf verbraucht nur den exakt ausgewählten Slottoken. Auch ein
SDK-Fehler kann eine verlorene Antwort bedeuten; es gibt keine automatische
Wiederholung. Ein Read, der eine leere Mailbox ausgewählt hat, entfernt keinen
zwischenzeitlich neu kopierten Wert. Ein neuer Link erhält keine alte Antwort.
`Copied` und erfolgreicher SDK-Aufruf sind weder Peerzustellung noch
Speicher-/Spielwirkung. Android muss Antworten weiterhin an Sitzung,
Transfer/Auftrag/Operation und erwartete Revisionen binden. Unklarer Apply-Ausgang
verlangt einen frischen Zustandsabgleich statt eines blinden zweiten Apply.

## Nachweisgrenzen

`GattsOutputCases.h` prüft den tatsächlichen Adapter mit modellierten SDK- und
RTOS-Ports: GM/kein GM, Wertebesitz, volle/leere Mailbox, Fristen einschließlich
Wrap, falsche Worker, Handlewechsel, Entzug an Beobachtungspunkten,
Sicherheitsverlust, spätes Kopieren bei leerem Read, rekursiver Read,
SDK-Sendefehler und optionale Dienstinstallation. Das sind kontrollierte
Interleavings, kein ESP32-SMP-/Funk-/Latenz- oder Stackreservennachweis.
Der getrennte SDK-Build kompiliert und linkt den echten Rückkanal gegen die
festgelegte Plattform. Produktstart, gegenseitiger Servernachweis,
Bond-Wiederverbindung, Android-Polling und gemeinsame Geräteabnahme bleiben
eigene offene Anschlüsse.
