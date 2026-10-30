# SP-01 P0 v1: Paket- und Regelsprache

**Status: ausgearbeiteter technischer Vertragsvorschlag zu #93 P0.**
Stand 29.10.2026, geprüfte Ausgangsbasis `191d08164614bd6472db241336d263da77691f49`.
Auftrag: P0 bis zum prüfbaren PR ausarbeiten. Maintainer führt Plattform und
Systemintegration; ein anderer Bearbeiter übernimmt die Qualitätsgegenprüfung.
Betroffen sind der künftige allgemeine Regelkern, GM-Autorenschaft/Vorschau,
D1-B/F5a und Paketübernahme. Bestehende Firmware-, Dev-/GM- und Laborvarianten
werden hier nicht umgebaut. Neue Sprachfähigkeit ist noch nicht implementiert.

Maßgeblich bleiben [SP-01 und SP-T01–SP-T07](../spielpakete-architektur.md),
[D0](../veranstaltung/d0-vertrag-v1.md), [dessen Sicherheitsvertrag](../veranstaltung/d0-sicherheit-migration-v1.md),
[D1](../veranstaltung/d1-persistenz-v1.md) und [Profil v1](../configuration/profile-model.md).
MUSS bezeichnet die zu implementierende v1-Semantik, keine vorhandene Fähigkeit.
[Referenzspiele](p0-referenzspiele-v1.md), [Formate und Module](p0-formate-v1.md)
und [Arbeits-/Prüfbericht](../../handoff/SPIELPAKETE-P0-2026-10-29.md) gehören dazu.

## 1. Technische Auswahl und Abgrenzung

Vorgeschlagen wird ein **typisierter Dateninterpreter mit endlichem Regelplan**.
Er verarbeitet Werte, reine Ausdrucksbäume und begrenzte Aktionslisten; Module
werden vor Aktivierung statisch expandiert. Kein Spielname steuert Produktcode.

| Möglichkeit | Bewertung für den ersten Schnitt |
| --- | --- |
| Profil-v1 unverändert ausbauen | Rollen/Kristalle und ein Graphübergang je Tick reichen für allgemeine Werte, Module und vollständige D1-Pläne nicht. Bestehende Profile behalten ihre Bedeutung. |
| Neue typisierte IR neben Profil v1 | Gewählt als technischer Vorschlag: begrenzter Validator, nachvollziehbarer Plan, dieselbe Semantik für Vorschau und Box; Geräteports wiederverwenden. Neue Parser-/Laufzeitimplementierung und Paritätsbeleg erforderlich. |
| Lua-/Wasm-Laufzeit | Für die beschriebenen zwei Fälle kein zusätzlicher Sprachbedarf belegt; zusätzliche Laufzeit-, Sandbox-, Speicher-, Zeit- und Editorverträge wären nötig. Kein SDK-/Bibliothekswechsel in P0; späterer Vergleich möglich, falls ein belegter Bedarf die IR überschreitet. |

Die neue IR ist `larp.rules.v1`, kein zusätzlicher Enumwert in Profil-v1.
P1 soll einen hardwarefreien Kern hinter Validator/Planer entwickeln; GM kann
seinen identischen Kern nutzen oder eine getrennte Implementierung anhand der
gleichen vollständigen Konformitätstraces nachweisen. Das Rechner-Prüfmodell
in diesem PR ist nur eine Semantikprojektion, keine produktive Laufzeit.

## 2. Werte, Ereignisse und reine Ausdrücke

Eine Station hat typisierte benannte `bool`- oder `int32`-Werte; Integer sind
-2147483648..2147483647. Kein Float, Stringvergleich oder impliziter Cast.
Enumparameter werden vor Expansion in einen deklarierten int32-Wertebereich
aufgelöst. Jeder Wert hat einen Initialwert und gehört genau einer Station.
Geteilte veränderliche Variablen über Boxen gibt es nicht.

Eingänge sind `local` (gültige entprellte logische Gerätebeobachtung),
`fact` (von D0/D1 zugelassene aktuelle Erfolgstatsache), `timer` oder ein
berechtigter lokaler Verwaltungsauftrag. Jeder angenommene Eingang hat eine
stabile Vorgangskennung und wird mit dem vollständigen lokalen Plan korreliert.
Die Kennung eines Gerätevorgangs vergibt der Eingangsbesitzer als
(Station, dauerhaft vor Nutzung vergebene Bootgeneration, lokale Folge);
bei unklarer Generation oder Überlauf keine Annahme. Timer verwenden
(Timer-ID, dauerhaft gespeicherte Arm-Generation). ein Wiederholen
nach verlorener Commitantwort behält sie. Neustart erzeugt keinen Erfolg.
Unbekannt/fehlerhaft ist ein eigener Wert, niemals eine positive Beobachtung.

| Ausdruck | Typ / Bedeutung |
| --- | --- |
| `literal`, `value(name)`, `input(field)` | Typisierter Literalwert, Stationswert, typisiertes Feld des aktuellen Eingangs |
| `fact(subject)` | bool: true nur mit aktuellem zugelassenem Inhaltsbeleg; fehlend unknown |
| `eq`, `lt`, `le`, `gt`, `ge` | Zwei gleiche Typen, Ordnung nur int32; unknown propagiert |
| `all`, `any`, `not` | Dreistellig: false dominiert all, true dominiert any, not unknown bleibt unknown |
| `add`, `sub` | Zwei int32; Überlauf verwirft den ganzen Plan. Keine Sättigung oder Wrap. |

Nur **true** löst aus. `not fact(x)` darf fehlende Daten nicht als Erfolg
werten. Abwesenheit/Timeout wird ausschließlich durch einen ausdrücklich
lokalen Timer und einen benannten Ersatzweg behandelt. Ausdrücke führen kein
I/O aus. Geteilte Ausdrucksreferenzen sind erlaubt; Zyklen nicht. Aktionswerte
werden aus dem Eingangssnapshot berechnet, nicht aus vorherigen Schreibaktionen.
Division, Zufall, Systemuhr und dynamische Codeauswertung sind in v1 unzulässig.

## 3. Regeln, Reihenfolge und vollständiger Plan

Eine Regel benennt stabile `ruleId`, `generation`, Trigger, Guard, Modus,
Priorität, Offlinepolitik und geordnete Aktionen. IDs sind stationslokal und
bleiben bei Bearbeitung erhalten. `once` bezeichnet eine fachliche Freigabe
mit D0-Latch `(event,run,localStation,ruleId)`. `each` bezeichnet einen lokalen
wiederholbaren Schritt mit Vorgangsbeleg `(event,run,station,inputOperationId)`;
er verwendet nicht denselben Freigabe-Latch für jeden Zählschritt.
Externe fact-Trigger dürfen nur `once` sein. Nichtmonotone verteilte
Zähler-/Rücksetzregeln sind in dieser Version ausgeschlossen. Der Validator
lässt fact-getriggerte Guards ausschließlich als positive fact-Blätter mit
all/any zu. Deren Aktionen dürfen nur emit und effect enthalten, keine
veränderlichen Werte/Timer. Unterschiedliche externe Regeln müssen disjunkte
Ausgabesubjects und Geräteports besitzen; ein Zielsubject hat genau einen
produzierenden once-ruleId. Damit sind nachträgliche Remote-Writes und
reihenfolgeabhängige Ressourcenwahlen ausgeschlossen. Lokale Zusatzregeln
dürfen deren Subjects/Ports nicht konkurrierend produzieren.

1. Der Anwendungseigentümer ordnet Eingänge lokal serialisiert ein. Innerhalb
   eines Erfassungsdurchgangs: fällige Timer zuerst (Deadline, dann timerId),
   danach lokale Geräteeingänge nach Besitzerfolge, danach zugelassene Facts
   nach lokaler Annahmefolge. Gleichzeitigkeit über Boxen wird nicht behauptet.
   D0-Abhängigkeiten und Lückenregeln bleiben vorgeschaltet.
2. Ein Eingang plus Zeit und unveränderlicher bestätigter Zustand bilden einen
   Snapshot. Alle passenden Regeln werden darauf geprüft. Reihenfolge für
   Plan und Trace: aufsteigende Priorität, dann lexikografische ruleId.
   Gleiche Priorität ist somit erlaubt. Eine Regel wird je Eingang höchstens
   einmal berücksichtigt; eine schon gelatchte once-Regel bleibt ausgelassen.
3. Alle zutreffenden Regeln bilden **einen** Plan. Mehrere Schreibaktionen auf
   denselben Wert oder Timer, auch mit gleichem Wert, sind `write_conflict`.
   Mehrere Intents auf denselben exklusiven Geräteport sind `resource_conflict`.
   Keine stille Gewinnerregel durch Priorität. Mehrere Fakten dürfen nur
   unterschiedliche lokale Subjects erfüllen; Wiederholung bereits erfüllter
   Subjects erzeugt keinen neuen Erfolg oder Effekt.
4. `set`, `add`, `arm`, `cancel`, `emit` und `effect` sind die einzigen Aktionen.
   `add` ist atomar `set(old+operand)`. `arm` startet einen einmaligen Timer;
   aktiven Timer erneut armen ist Fehler, außer derselbe Plan konsumiert dessen
   fälligen timer-Eingang. `cancel` auf inaktivem Timer ist ein No-op.
   `emit(subject)` erzeugt ausschließlich D0 `task_solved`, true. Lokale
   Ersatzfreigaben verwenden den getrennten berechtigten local_override-Pfad.
   `effect(port,resource)` erzeugt einen begrenzten Intent, keine Wirkung.
5. Typen, alle Kapazitäten, erwartete Profil-/Rechte-/Fortschrittsrevisionen
   und der **gesamte** Ausgabebedarf werden vor Übernahme geprüft. Bei Fehler,
   vollem Journal/Outbox/Intentbereich oder ungültigem Teilplan bleibt der
   bestätigte Zustand unverändert. Kapazitätsmangel ist deferred, nicht applied.
6. D1 übernimmt Zustand, Werte, Timer, Latches, Vorgangsbeleg, zugeordnete
   Fact-IDs/Outbox und Intents atomar oder gleichwertig wiederaufnehmbar.
   Kein Effekt und keine Veröffentlichung vor Commit; keine Rekursion oder
   sofortige Neubewertung nach einem set. Folgeeingänge kommen später über
   denselben Besitzer. Eigene emit-Fakten erzeugen nach Commit einen späteren
   lokalen fact-Eingang mit derselben stabilen Fact-ID und demselben Digest;
   Kontext, aktuelle Rechte und Deduplizierung werden wie beim Fernempfang
   geprüft. Ihre lokalen Inhaltsbelege stehen fact(subject) zur Verfügung.
   So können Moduloutputs weitere lokale Module speisen, ohne Rekursion oder
   neue Kennungsvergabe. Die Queue hält bei voller Kapazität den dauerhaften
   Vorgang pending, statt ihn zu verlieren.
   Ein Plan mit null zutreffenden Regeln erhält ebenfalls
   einen Vorgangsbeleg, damit derselbe Eingang später nicht anders wirkt.

Einmalfreigaben dürfen nach Empfangsreihenfolge unterschiedlicher Ursprünge
nicht verschiedene fachliche Endstände produzieren. v1 verlangt monotone
externe Freigaben; lokale Reihenfolgespiele verwenden ausschließlich lokale
Eingänge. Eine externe AND-Regel speichert Fakten unabhängig und prüft bei
jedem passenden Eingang alle benannten Inhaltsbelege. Fehlt einer, wartet sie;
der letzte Fakt ersetzt nicht die Abhängigkeiten. Mehr als acht notwendige
FactRefs je neuem Fakt werden abgewiesen, nicht abgeschnitten.

## 4. Zeit, Stop und Wiederanlauf

Timer zählen **aktive lokale Spielzeit**, maximal 86400000 ms je Timer;
Dauer muss positiv sein. Keine gemeinsame Uhr und kein Ablauf durch bloße
Funkstille. Ein aktiver Timer darf genau einen späteren timer-Eingang liefern.
Der Timer wird erst im dauerhaften Eingangscommit konsumiert, auch wenn
kein Guard passt. Bei deferred/Planfehler bleibt derselbe Timer-Vorgang
wartend; kein wiederholtes Erzeugen neuer IDs. Impliziter Consume und ein
explizites arm desselben konsumierenden Plans sind ein Timerwrite.
Fälligkeit wird am Beginn eines Durchgangs erfasst; von einem Plan neu
gearmte Timer können frühestens im nächsten Durchgang auslösen.
Die erfasste Liste enthält Kandidaten, keine bereits angenommenen Eingänge.
Vor jedem Timer-Eingang muss derselbe Arm (Timer-ID und Arm-Generation)
noch aktiv und fällig sein. Ein vorher bestätigter Cancel verwirft dessen
Kandidaten ohne Vorgangsbeleg oder Regelwirkung; ein neuer Arm wird niemals
durch den alten Kandidaten konsumiert. Ein fehlgeschlagener Plan bestätigt
auch seinen Cancel nicht; die betroffenen Arme bleiben unverändert.

Pause friert Restzeit ein; Resume setzt sie fort. Stop verhindert weitere
Regelverarbeitung und beansprucht keine neuen Geräteintents. Timerrest und
Zustand werden beim bestätigten Pause/Stop konsistent gespeichert. Fortsetzen
ist bewusster Verwaltungsauftrag, Start eines neuen Runs gesondert.

Bei ungeplantem Neustart ist die Zeit seit letztem dauerhaftem Checkpoint
**unbekannt**. Die Station startet stopped/recovery_required; kein fälliger
Timer wird erfunden oder automatisch abgespielt. Berechtigte Entscheidung:
Checkpointrest fortsetzen (mögliche Verlängerung sichtbar), einen expliziten
Ersatzweg wählen oder neuen Run beginnen. Keine zugesagte exakte Deadline über
Stromausfall. Periodisches Checkpointing hat in P1 Verschleiß-/Latenzbudget.
Timerbesitzer verwendet monotone Differenzen mit explizitem Überlaufschutz;
keine uint32-Uhrdifferenz jenseits des halben Wertebereichs auswerten. Checkpoint
spätestens jede 1000 ms aktive Zeit ist ein **zu qualifizierendes Soll**;
bei Überschreitung friert Timerfortschritt mit fault ein.

Für physische Intents gelten D1 prepared/attempted/unknown/observed und erneute
lokale Zulässigkeitsprüfung. Crash nach attempted führt zu unknown, nie zu
automatischer Wiederholung. Ein Effektfehler setzt bestätigten Fortschritt nicht
zurück. Licht/Ton sind Rückmeldung, kein Beweis einer erfolgreichen Mechanik.

## 5. Offline und Quellenvertrauen

Jede fact-Regel deklariert `wait`, `local_continue` oder `local_fallback`.
Wait blockiert nur die benannte Abhängigkeit. Local_continue lässt andere
lokale Regeln weiterlaufen, erfüllt aber keinen fehlenden Fakt. Local_fallback
verweist auf eine explizite Ersatzregel und dieselbe fachliche once-Freigabe;
Original und Ersatz konkurrieren um denselben D0-Latch. Die Ersatzentscheidung
bleibt im Beleg sichtbar. Ein Timeout allein verleiht keine Verwaltungsrechte.

IR-Facts werden nur aus D0 task_solved/local_override derselben zugelassenen
Veranstaltung, Run, Deploymentmanifest und Stationsbindung gespeist.
unused_range ist ausschließlich Streamkontrolle. Beobachtung und receipt sind
keine Guard-Fakten; Sofortaufträge verwenden D0-Grants und nicht diese Outbox.
D0-G1–G5, echte Ursprungsverifikation und #27/#28 bleiben erforderlich.
Die Paketquelle darf keine Trustregister, privaten Schlüssel, Allow-all-Policy,
GPIO-Aufträge oder unverifizierte Remote-Eingänge installieren.

D1-B muss wiederholbare lokale Schritte, Werte/Timer und vollständige Pläne
ausdrücklich ergänzen. Das integrierte D1-A-Modell ist dafür keine fertige
Firmwarepersistenz. Ein Sensorereignis darf erst bestätigt werden, wenn sein
Vorgangsbeleg dauerhaft ist. Ist die Quelle nicht wiederholbar und geht vor
Commit verloren, bleibt die Beobachtung verloren/unknown; kein Exactly-once-
Versprechen für beliebige physische Eingänge.

## 6. Ressourcen und überprüfbare Startbudgets

Grenzen gelten je **expandierter Station**. Validator weist Überschreitungen
vor Aktivierung ab. Ziele dürfen kleinere beworbene Grenzen haben; Annahme
verlangt, dass der konkrete Plan hineinpasst. Dies sind Planungsgrenzen,
keine bereits gemessenen ESP32-Kapazitäten.

| Bereich | Vorgeschlagene harte v1-Obergrenze |
| --- | --- |
| Paket / Stationen / Module | 256 KiB kanonischer Inhalt, 8 Stationen, 16 verschiedene Moduldefinitionen |
| Werte / Regeln / Timer | 32 / 64 / 16 je Station |
| Ausdruck / Expansion | 512 Knoten insgesamt, Pfadtiefe 8; 32 Modulinstanzen, Expansionstiefe 4 |
| Aktionen / ein Plan | 8 je Regel, insgesamt 32 Schreib-/Timer-/Fact-/Intentaktionen je Eingang |
| Ein-/Ausgang | 16 wartende Eingänge, 8 neue Facts und 8 Geräteintents je Plan; keine Verdrängung |
| Rechenschritte | 4096 Ausdrucksknoten-/Aktionsbesuche je Eingang; bereits Validierung berechnet oberen Worst Case |
| Factabhängigkeiten | D0 maximal 8 je erzeugtem Fakt; Quellen-/Subjectliste vollständig deklariert |
| Text / Ressourcen | Bezeichner 1..48 ASCII-Bytes; Label 1..96 UTF-8-Bytes; 32 Ressourcen, Metadaten max. 32 KiB |

Erste überschlägige **explizite Layoutziele** für eine Station:
Werte 32×16=512 B, Regeln 64×32=2048 B, Knoten 512×16=8192 B,
Aktionen 512×16=8192 B, Timer 16×24=384 B, Eingänge 16×64=1024 B,
Plan 32×32=1024 B plus 8×128=1024 B Intents: **22400 B**.
Zwei Laufzustandskopien für atomare Planung, Stringtabellen, Modulcompiler,
Parser, D0-Frames, D1-Outbox/Journal und echte Geräte-/SDK-Stacks kommen hinzu.
Als erste Arbeitsreserve **64 KiB RAM zusätzlich** vorsehen, nicht als zugesagten
Messwert; ganze Quelldatei nicht gleichzeitig im ESP-Heap voraussetzen.
Profil-v1 benötigt bereits 28636 B je Profil; vorhandene zwei Slots werden
nicht gedankenlos parallel zum neuen IR-Doppelstand gehalten.

P1 misst sizeof/Linkermap, Heapminimum mit BLE/ESP-NOW/WLAN, Stackmaximum,
Planlatenz und Speicherworker-Rückstau am tatsächlichen SDK-Kandidaten.
Vorläufiges Planlatenzziel 2 ms ohne I/O ist zu messen, keine Deadlinegarantie.
Budgetfehler hält den betroffenen Plan mit Diagnose an; er macht nicht aus
Zeit-/Kapazitätsgründen ein anderes Spielergebnis. Journalvoll: keine neue
bestätigte Tatsache, lokale Status-/Stopbedienung muss weiter funktionieren.
Kein Implementierungsstart benötigt hierfür ein neues Produktmerkmal; eine
nicht erfüllbare Kapazität erfordert transparente Vertragsrevision.

## 7. Versionierung, Migration und nächste Schnitte

Sprachmajor exakt 1, keine stille Interpretation unbekannter Operationen oder
Versionen. Capability `rules_ir_v1` erst nach Validator, vollständigem Planer,
Persistenzanschluss und Konformitätsnachweis bewerben. `rules_preview_v1`,
`package_install_v1` und Geräteeigenschaften separat prüfen; ein Firmwarelabel
oder dieses Dokument schaltet keine Fähigkeit frei.

Profil-v1 wird weiter als v1 gelesen, aber nicht als allgemeines Regelpaket
ausgeführt. Automatische Konvertierung ist nicht Teil von P0: P1 braucht einen
expliziten verlustprüfenden Konverter mit Herkunft und Vergleichstraces; weder
Rollenfortschritt noch Gerätebelegung werden beim Import geraten.

Paketwechsel nur bei gestopptem Spiel und aktueller Berechtigung. Im selben
Run v1 nur bei identischen Werttypen/IDs, Timern, Subjectbezügen und ruleIds
mit explizitem Zustandsmapping erlauben; sonst neuer Run oder abweisen.
Latches und Vorgangsbelege bleiben erhalten, auch bei höherer generation.
Neue ruleId ist neue fachliche Regel und erfordert bewusste Migration, keine
Umbenennung zum Zurücksetzen. Neue Run-ID setzt keine D0-Quellsequenz zurück.
Downgrade/unklare Daten sperren; kein Formatieren oder automatischer Rollback.

| Nächster Schnitt | Abschlussnachweis |
| --- | --- |
| P1a Validator/Expandierer/Planer | Vollständige Formate und alle Operatoren inkl. Grenz-/Konfliktfälle, gleiche Traces beider Spiele, keine Spielnamen im Kern |
| P1b D1-B und #31 F5a | Vollständige Mehrregelpläne, lokale Vorgänge/Werte/Timer/Intents, Crashpunkte und Entzug; gemeinsame Ressourcen-/Latenzmessung |
| P1c GM-Autorenschaft/Vorschau | Alle Module/Parameter/Porttypen als Daten bearbeitbar; kein externer JSON-Edit nötig; eigener UI/UX-Auftrag mit Rollenvertrag |
| P2 #30/#37 | Autorisierte Übernahme, Ressourceninventar und bestätigter Speicher-/Aktivstand pro Box |
| P3 #31/#60/#61/#63 | Zwei echte kooperierende Referenzspiele samt Partition/Replay/Neustart und zwei Handys |
| P4 | Erst nach P3 Freeze und neuer Drittspielauftrag gemäß SP-T01–SP-T07 |

P0 liefert einen technischen Vorschlag mit Gegenbeispielen; es schließt weder
#93 noch SP-T01–SP-T07, D0-G1–G5 oder Geräteabnahmen. Es definiert ausdrücklich
kein drittes Referenzspiel vor dem Freeze.
