# Merge-Drift-Landkarte

Wird vom [merge-drift-map](SKILL.md)-Skill Datei für Datei/Komponente für Komponente befüllt — kein Vollscan, sondern nur tatsächlich geprüfte Stellen.

## Legende

**Diff-Anteil** (geänderte Zeilen ÷ Zeilen auf `stable-33.0`): wie *oft* ein Merge hier vermutlich einen Textkonflikt auslöst — sagt nichts darüber, wie gefährlich der wäre.

**Qualitative Einstufung** (niedrig/mittel/hoch, aus [stable-merge-check](../stable-merge-check/SKILL.md)s Workflow A): wie *gefährlich* eine Abweichung tatsächlich ist — additive Whitelabel-Ergänzungen sind unkritisch, umbenannte/umstrukturierte Stellen können hart kollidieren oder sich sogar unbemerkt falsch zusammenführen.

Kombiniert: niedrig+niedrig = unauffällig, hoch+niedrig = viele harmlose Konflikte, niedrig+hoch = seltene, aber gefährliche Stellen, hoch+hoch = besondere Vorsicht.

## SES-578-Dateien (Erstbefüllung)

*Eigener Commit: `9d3af4e33` (2026-08-20) · stable-Seite: `9e65fef46`*

| Datei | Auf stable? | Diff-Anteil | Qualitative Einstufung | Bewusst/Drift | Bemerkung |
|---|---|---|---|---|---|
| `src/gui/settingsdialog.cpp` | ja | ~49% (298/607) | **hoch** | gemischt | bekannter Merge-Hotspot, `IONOS_BUILD`-Verzweigungen |
| `src/gui/folderstatusdelegate.cpp` | ja | ~72% (349/482) | **nicht vollständig geprüft** | — | nur SES-578-Teiländerung heute inhaltlich angesehen, nicht volle Historie |
| `theme/Style/Style.qml` | ja | ~60% (130/216) | niedrig | bewusst (Whitelabel) | fast nur zusätzliche `ses*`-Properties, Kernfunktionen unverändert |
| `src/gui/folderwizard.cpp` | ja | ~63% (467/736) | niedrig | bewusst | additive Whitelabel-Styles + eine bewusste Bugfix-Zeile (`ModernStyle`) |
| `src/gui/accountsettings.cpp` | ja | ~30% (586/1927) | niedrig | bewusst | Struktur deckungsgleich, nur Branding-Werte |
| `src/gui/selectivesyncdialog.cpp` | ja | ~19% (106/553) | niedrig | bewusst | fast rein additiv |
| `theme.cpp` (`src/libsync/theme.cpp`) | ja | ~17% (195/1156) | niedrig | bewusst | heute vollständig gegen stable durchgegangen |
| `src/gui/owncloudgui.cpp` | ja | ~15% (115/760) | niedrig | bewusst | |
| `src/libsync/theme.h` | ja | ~3% (23/682) | niedrig | bewusst | |
| `src/common/utility.h` | ja | **0%** | keins | — | seit heutigem Revert exakt identisch zu stable |
| `src/common/utility_win.cpp` | ja | **0%** | keins | — | dito |
| `src/gui/buttonstyle.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/stratotheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/basetheme.h` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |
| `src/gui/sessnackbar.cpp` | **nein** | n/a (fork-only) | keins von stable-Seite | bewusst | |

**Auffällige Kombinationen:**
- **hoch+hoch:** keine in dieser ersten Runde.
- **hoch Diff-Anteil + niedriges Risiko** (häufige, aber harmlose Konflikte zu erwarten): `folderstatusdelegate.cpp` (72%, aber noch nicht vollständig qualitativ geprüft — Kandidat für eine vertiefte `stable-merge-check`-Runde), `folderwizard.cpp` (63%), `Style.qml` (60%).
- **niedrig Diff-Anteil + hoch** (selten, aber gefährlich): keine aktuell in der Karte — die frühere `isWindows11OrGreater()`-Verlagerung wäre ein Beispiel gewesen, ist aber bereits zurückgerollt (daher jetzt 0% bei `utility.h`/`utility_win.cpp`).
- `settingsdialog.cpp` bleibt die Datei mit der ungünstigsten Kombination (hoher Diff-Anteil, hohes Risiko) — verdient vor jedem Merge besondere Aufmerksamkeit.
