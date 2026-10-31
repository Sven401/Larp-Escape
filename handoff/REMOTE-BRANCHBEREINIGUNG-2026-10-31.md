# Remote-Branchbereinigung · 31. Oktober 2026

**Status: umgesetzt und nachgeprüft.** Quelle: ausdrücklicher Nutzerauftrag
„lösche die 49 vollständig integrierten Remote-Branches und dokumentiere den
geprüften Bestand im Repository. Lokale Branches und Arbeitskopien bitte
vorerst unverändert lassen.“ Codex-Sitzung `01a192cd-8d52-7d30-afe5-4b2f96426c86`.

## Basis und Verfahren

Repository: [Sven401/Larp-Escape](https://github.com/Sven401/Larp-Escape).
Frisch abgerufene Zielbasis: `plan/projektuebergabe-kuratierter-stand`
bei `2db141294c18678c46435a400f23bb5c54c4d0cc` (Merge von PR #117).
Vorher 69 Remote-Branches, davon 52 mit vollständig in der Zielbasis
erreichbarer Commit-Historie. Davon blieben `main`, `SvensBranch` und
der Integrationsbranch ausdrücklich erhalten.

Die 49 autorisierten Namen wurden mit dem frischen Bestand und
`git for-each-ref --merged=origin/plan/projektuebergabe-kuratierter-stand`
abgeglichen. Die Löschung erfolgte in einem erfolgreichen atomaren Push,
für jeden Ref mit `--force-with-lease=refs/heads/<name>:<geprüfte SHA>`.
Zwischenzeitlich geänderte Spitzen hätten die gesamte Transaktion abgewiesen.

**Nachkontrolle:** `git ls-remote --heads origin` liefert genau die
20 erwarteten verbleibenden Branches mit identischen SHA-Werten.
Alle 49 Löschkandidaten fehlen. Lokale Branch-Refs, registrierte Worktrees
einschließlich ihrer HEADs und der Dateistatus wurden vor/nach der Löschung
verglichen und sind unverändert. Remote-Tracking-Refs wurden beim Fetch/Push
aktualisiert; lokale Arbeitsbranches wurden weder gelöscht noch verschoben.

Der Dokumentationsbranch `codex/remote-branchbereinigung-2026-10-31`
kommt nach dieser Bestandsmessung hinzu: mit ihm 21 Remote-Branches.
Der Bericht wird als eigener PR gegen den Integrationsbranch vorgelegt;
keine Integration nach main und kein Merge als Teil dieses Auftrags.

## Gelöschte Remote-Branches

Alle nachstehenden Spitzen sind über die oben genannte Zielbasis weiterhin
vollständig erreichbar. Die Tabelle bewahrt Namen und vollständige SHA-Werte
für eine gezielte spätere Wiederherstellung eines Branch-Refs.

| Remote-Branch | Geprüfte und gelöschte Spitze |
| --- | --- |
| `codex/bereinigung-abschluss-2026-10-20` | `95112188afdece35f11a4d196f8158473988b325` |
| `codex/ble-fortsetzungsbasis` | `2f0b03f925cb5d54fc72ce0059d97815632652a4` |
| `codex/ble-rollen-profile` | `2728220166997248be7a0860d466dd49cd687cdb` |
| `codex/d0-veranstaltungsvertraege` | `d5ef3b663dfceb65861de7c4ec9e023b7f530879` |
| `codex/d1-fortschritt-journal` | `5462301aa83a52bc1962fd114979f3f06f7a6bb2` |
| `codex/dezentrale-veranstaltungsarchitektur` | `0b58f359d6137bc5e33a99d1a5a30a35ac490422` |
| `codex/entwicklungsstart-audit` | `776a15d351ba043d8781a8de1f963ee3ce2bf224` |
| `codex/esp-idf-migration` | `085788e67dfbdc4d9b6f468e1b68f0f1f796e64e` |
| `codex/espnow-app-bedienung` | `1dbe2d1ccd4eef73cbb21a1df31ae79da53d4177` |
| `codex/espnow-baugruppentest` | `49cb1d01abce027222717857f423c55daa366fc6` |
| `codex/fachaudit-aktivierung-pruefstand` | `9fbee0d073a464fd0957027687aff778e78007e4` |
| `codex/fachaudit-entwurf` | `d17e699e548346051f0777a022171be545e76b9b` |
| `codex/fachaudit-erstlaufkontrolle-doku` | `689da76c6d9b1212db2fee9996ec5f3c3c50d2ec` |
| `codex/fachaudit-integration` | `12cd5c5239ae9c9e6ad0ddf46fb4b33eba97937b` |
| `codex/fachaudit-probelauf-2` | `a8f5ec45dccf6a2d83347bb503c70e8e9255a755` |
| `codex/github-automatisierung` | `c4c5a2a366b6d2c9ed956efa07ea3b7cccc2be14` |
| `codex/hardwareplanung-testergebnisse` | `a332bb58de1e2e19c7b6a60231d6d2e71dab239e` |
| `codex/integration-offene-prs-2026-10-18` | `4e93343367e7fa5e8e456ce7e5cce645f6af38ff` |
| `codex/integrationsabschluss-69` | `a586b75c9756b14cce9ca6d6764e3594f312128a` |
| `codex/led-architekt-integrationsstand` | `d509f7eb85146356398da24760edfd95ce668ead` |
| `codex/led-camera-measurement` | `1066b1878acaa01c3be3c0ced84c864dfce15b72` |
| `codex/led-effekt-architekt` | `40d962188635f6a7fc2d766523c4a714fd20eaf3` |
| `codex/led-kristallsiegel-entwurf` | `900db936643dd5c049ab8df3b1c92810fe8027fc` |
| `codex/led-resonanzkreis` | `edb36b3282813e1ce78ac302e2ed80446da79954` |
| `codex/maintainer-integration-191d081` | `b1d7fcfd8e82700fde843757689c67a79eb84f2a` |
| `codex/nachladbare-spielpakete` | `40149be3499712b325b02fc4376215c0ba8d4ed3` |
| `codex/naeheglut-ci-abschluss` | `0fb6ab096ebf0858121875b1e028222bc60f35d1` |
| `codex/naeheglut-espnow-test` | `8e4c9af78dee1c4aa325ff18697a131d08e5bb9d` |
| `codex/naeheglut-integrationsstand` | `da1369dedfa90d74fec1491fc9c66fbbbb22776e` |
| `codex/naeheglut-stop-integrationsstand` | `edde13f6c373225764bcb4a046cee250403fe6ec` |
| `codex/naeheglut-stop-spielanschluss` | `9114ff89863501809e06f65892ec6948342eecb6` |
| `codex/p1a1-paketreader` | `d75742b46b5f110e25ddad0eb3cc1056255fa642` |
| `codex/p1a2-schema-typen` | `98638eef25ba221329d81cb576a2b1625c8e53e0` |
| `codex/pm-p1-2026-10-31` | `c1b214ab553720423225fac4fa4d43953ec3dc91` |
| `codex/pr79-integrationsabschluss` | `be2ad1a211742b6d4980696fe3ecd275389d12a4` |
| `codex/pr83-merge-dokumentation` | `0a8bb416a86e433214de4a38aba1d50462273c77` |
| `codex/pr84-merge-dokumentation` | `823e3c7b35ac1be6f0c437a1a819ee9696a92b5b` |
| `codex/projektkonfiguration-abnahme` | `9f1eb56b3c7a1434a1926a49bb0058a9f867e4c8` |
| `codex/rollen-einfuehrungsabschluss` | `6a5715a4a591d1a8482c595d18afc546bdb9882d` |
| `codex/rollenarchitektur` | `3d8b6d2c57f7cd1bbe75d1d7d01bfff685086457` |
| `codex/rollenpilot-a-einstieg` | `32a6ba793cf8b4e21c2b9a320b75bec715442950` |
| `codex/spielpakete-p0` | `957aaa792ef7786541438edf6a68bebb6d0118cf` |
| `codex/svensbranch-regression` | `9af36a00df6760223db2900658c17bdb82f48871` |
| `codex/ui-ux-integrationsstand` | `c20712571f12bca3bb22cea2f8993e85dd0e5128` |
| `codex/ui-ux-projektreferenz` | `98488ea61117cba88b87803eb4711add3490edcc` |
| `codex/ui-ux-role-pilot` | `3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4` |
| `codex/veranstaltungszugang-konzept` | `2f5fba6c574716b0764f680a0e4c06839dc973e3` |
| `codex/wartung-pruefstand-2026-10-20` | `44e7098b99f2eb2dc308e8f41eaa6a60af09448e` |
| `codex/werkbank-projektsetup` | `a23ecd1315068f01091643e42cb52d03d952d371` |

## Erhaltener Bestand unmittelbar nach Löschung

| Remote-Branch | Spitze | Grund |
| --- | --- | --- |
| `Starwarscon` | `540aeec1362c2d29a3f28267a0abf3b794cf8c1b` | eigene, nicht vollständig integrierte Historie; erhalten |
| `SvensBranch` | `a40365d715b2055ee23f767fa53b55ac25243437` | vollständig integriert; historischer Branch ausdrücklich erhalten |
| `codex/ble-softwaretore` | `7a6e5ce394333d1ba07b464eafc396c794b46030` | offener PR #105 |
| `codex/esp32-s3-proximity-konzept` | `1a42a8305142683041c2850a157a3d196c073493` | offener PR #116 |
| `codex/esp32-s3-waveshare-tests` | `c4cbe28fcf95057841f8f61d5abead2d7019d2c3` | offener PR #115 |
| `codex/ki-projekteinstieg` | `96a75cca2e2ae15f8cebc382ae81727d6c13ac6b` | offener PR #106 |
| `codex/p1a3-regelpruefung` | `1d44904602ac2aec666efe30cb56db165879ff82` | offener PR #121 |
| `codex/p1a3-speicher` | `43bd65454bad2cca51a7c6222096db608dfb63e0` | offener PR #122 |
| `codex/pcb-konzept-112` | `1334118011d3cbd5b64eb541d67e6e96816d49e6` | offener PR #113 |
| `codex/produktstand-2026-10-31` | `be51d708a4bb037246b47c6a0fe1e49b13d388b6` | offener PR #120 |
| `codex/s3-nearglow-android-live` | `31d6ccabe38e719ab4b04f57f35b489351f20b1d` | offener PR #119 |
| `codex/s3-nearglow-autonomous` | `b3e3693dc3904877c5e508273247ab495534f5d8` | offener PR #118 |
| `main` | `29da937c4309beddfd45c5dd0fbf84fdf853d8e4` | historischer Default-Branch; erhalten |
| `plan/projektuebergabe-android-entwuerfe-archiv` | `b13aa57ff9b4ab0171b5f608c985a8eb5926edf3` | historisches Quellarchiv; erhalten |
| `plan/projektuebergabe-diagnose-historie-archiv` | `8e1fc9abf57d88e98a56fd8ef1f42028eb78ed7b` | historisches Quellarchiv; erhalten |
| `plan/projektuebergabe-experimente-archiv` | `d4993a6442ff64a22a108bb3de3879a3a57f76ac` | historisches Quellarchiv; erhalten |
| `plan/projektuebergabe-firmware-f1-archiv` | `7617db248fe06bc4968152210b3a6015f455c7f6` | historisches Quellarchiv; erhalten |
| `plan/projektuebergabe-hardwarequellen-archiv` | `a52ce791ac7b0a25352ba142cba33cfafb438b34` | historisches Quellarchiv; erhalten |
| `plan/projektuebergabe-kuratierter-stand` | `2db141294c18678c46435a400f23bb5c54c4d0cc` | vereinbarter Integrationsbranch; erhalten |
| `plan/projektuebergabe-led-entwurf-archiv` | `918d418ece097ecaa1e730130b0fc22a352c4dda` | historisches Quellarchiv; erhalten |

## Lokale Grenzen und Nachweise

Der Hauptcheckout bleibt auf `codex/led-resonanzkreis` bei
`f1113b199b8e80b7edd922f70716219532d66999`.
Seine lokale Spitze unterscheidet sich vom gelöschten Remote-Ref und bleibt
erhalten; ebenso der lokale Branch `codex/esp32-s3-led-stations` auf derselben
Spitze. Die vorher ermittelten lokalen Entwürfe
`codex/audio-test-alternative-local` und `codex/ble-led-versuch` bleiben
ebenfalls unangetastet. Lokale und entfernte gleichnamige Branches dürfen
daher bei einer späteren Bereinigung nicht gleichgesetzt werden.

Die vorhandene Änderung an `.vscode/extensions.json` und das ungetrackte
Verzeichnis `.tmp/` bleiben unverändert. Private/ignorierte Artefakte wurden
nicht auf Entbehrlichkeit geprüft; es wurde keine Arbeitskopie entfernt.
Keine neue Aussage über Sicherung privater Flashabzüge oder Buildartefakte.

Passende historische Belege:
[Repository-Hygiene vom 18.10.](REPOSITORY-HYGIENE-2026-10-18.md) und
[technische Übergabe: Quellarchive](README.md#quellarchiv-und-wiederaufnahme).
Die sechs Quellarchive bleiben Sicherungsquellen; ihre Existenz ist keine
fachliche Freigabe verworfener Entwürfe.

Geprüft wurden Git-Erreichbarkeit, explizite Namensliste, Transaktionserfolg,
verbleibende Remote-Refs und unveränderter lokaler Zustand.
Keine Produktquellen geändert; keine Builds, Softwaretests, Geräteuploads
oder neue Geräteabnahme erforderlich bzw. durchgeführt.
Die eigene Bestandsprüfung ersetzt keine unabhängige Qualitätsgegenprüfung
des Dokumentations-PRs; dessen Review und CI bleiben gesonderte Nachweise.
