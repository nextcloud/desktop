---
name: gui-context-refresh
description: Aktualisiert die AI-lesbare Dokumentation von src/gui unter .claude/context/gui/ (CLAUDE.md je Unterordner + COMPONENTS.md für den flachen Wurzelbereich), indem nur die seit dem letzten Scan tatsächlich geänderten Bereiche neu erfasst werden. Nur manuell auslösen. Trigger — "aktualisiere die gui-doku", "gui context map ist veraltet", "gui-Doku auf den neuesten Stand bringen", nach größeren Umbauten/Merges in src/gui.
---

# GUI-Context-Refresh

Hält `.claude/context/gui/` (siehe dessen [CLAUDE.md](../../context/gui/CLAUDE.md)) aktuell, ohne bei jedem Aufruf alle ~450 Dateien in `src/gui/` neu zu scannen. Wird **nur manuell** aufgerufen, nicht automatisch bei jeder Session.

## Ablauf

1. **Referenzstruktur einlesen**: Liste alle Dateien unter `.claude/context/gui/` (`CLAUDE.md` je Unterordner + `COMPONENTS.md`). Jede trägt am Ende eine Zeile `*Quelle: src/gui/<bereich> — Stand YYYY-MM-DD, ...*`. Dieses Datum ist der Ausgangspunkt.

2. **Strukturabgleich zuerst** (billig, immer machen):
   - `find src/gui -maxdepth 1 -type d` gegen die Liste der dokumentierten Unterordner abgleichen.
   - Neuer Unterordner ohne `CLAUDE.md`-Gegenstück? → Dem Nutzer melden, danach wie in Schritt 4 (Vollscan) behandeln.
   - Dokumentierter Unterordner, der nicht mehr existiert? → Nutzer fragen, ob die verwaiste Doku-Datei gelöscht werden soll (nicht automatisch löschen).

3. **Änderungs-Check pro Bereich** (git-basiert, kein Neuscan):
   - Pro Unterordner: `git log --oneline --since=<Stand-Datum> -- src/gui/<ordner>`
   - Für den flachen Wurzelbereich (COMPONENTS.md): `git log --oneline --since=<Stand-Datum> -- src/gui/*.cpp src/gui/*.h src/gui/*.ui src/gui/*.qml src/gui/*.mm` (Shell-Glob, matcht nur Dateien direkt in `src/gui/`, nicht in Unterordnern — im Bash-Tool ausführen, nicht PowerShell, damit die Glob-Expansion greift).
   - Keine Treffer → Bereich überspringen, nichts anfassen.

4. **Bei Treffern: Umfang der Änderung einschätzen, dann passend reagieren**:
   - **Kleine Änderung** (grob: ≤ 3 betroffene Dateien, keine neuen/gelöschten Dateien, keine neuen Top-Level-Klassen): gezielt nachziehen — `git diff <Stand-Commit-oder-Datum> -- <geänderte Datei(en)>` lesen und nur die betroffenen Zeilen/Bullet-Points im bestehenden `CLAUDE.md`/`COMPONENTS.md`-Abschnitt per Edit anpassen. Kein Agent nötig für triviale Fälle.
   - **Größere/strukturelle Änderung** (neue Dateien, gelöschte Dateien, umbenannte Kernklassen, oder viele betroffene Dateien): den betroffenen Bereich komplett neu scannen wie beim Erstaufbau — ein Explore-Agent mit demselben Muster wie in der ursprünglichen Erstellung (Zweck, Kernklassen, Zusammenspiel, Fork-spezifisch vs. Upstream) über den Ordner laufen lassen und die zugehörige Datei komplett neu schreiben. Für den Wurzelbereich: nur den/die betroffenen Cluster in `COMPONENTS.md` neu scannen, nicht die ganze Tabelle.
   - Bei Unsicherheit, welche Kategorie zutrifft: lieber der größeren Kategorie zuordnen (Vollscan des Bereichs) statt eine unvollständige gezielte Anpassung zu riskieren.

5. **Datum aktualisieren**: Nach jedem tatsächlich geänderten Bereich die `*Quelle: ... Stand YYYY-MM-DD*`-Zeile auf das heutige Datum setzen. Unveränderte Bereiche behalten ihr altes Datum (zeigt ehrlich, wie alt die Doku dort wirklich ist).

6. **Zusammenfassung geben**: Am Ende auflisten, welche Bereiche unverändert übersprungen, welche gezielt nachgezogen und welche komplett neu gescannt wurden. Neue/verwaiste Unterordner aus Schritt 2 erneut hervorheben, falls der Nutzer sie noch nicht behandelt hat.

## Nicht in diesem Skill enthalten

- Kein automatisches Löschen verwaister Dateien (immer erst fragen).
- Kein periodischer/automatischer Trigger — der Nutzer entscheidet bewusst, wann aktualisiert wird.
- Prüft nicht inhaltlich, ob die *Fork-spezifisch vs. Upstream*-Einordnungen noch stimmen, wenn sich am Referenz-Branch selbst etwas geändert hat (dafür ist [stable-merge-check](../stable-merge-check/SKILL.md) zuständig).
