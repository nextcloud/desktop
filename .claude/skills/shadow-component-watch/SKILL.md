---
name: shadow-component-watch
description: Überwacht Fork-eigene UI-Komponenten, die eine Upstream-Komponente funktional ersetzt haben (Paare in registry.md, z. B. TrayWindowHeader.qml → SesTrayHeader.qml), auf funktionale Änderungen im upstream stable-Branch an der verwaisten Originaldatei und pflegt ein Änderungsprotokoll (CHANGELOG.md) über bewusste vs. offene Abweichungen. Nur manuell auslösen. Trigger — "prüfe die shadow components", "sind unsere Ersatzkomponenten noch aktuell", "wurden funktionale Änderungen aus stable übernommen", "shadow component check", nach einem größeren stable-Merge, wenn im Gespräch eine weitere "durch eigene Entwicklung ersetzte" Komponente identifiziert wird.
---

# Shadow-Component-Watch

Manche Fork-Bereiche haben eine Upstream-Komponente nicht angepasst, sondern komplett durch eine eigene Entwicklung ersetzt (Beispiel: `TrayWindowHeader.qml`/`CurrentAccountHeaderButton.qml` → `SesTrayHeader.qml`/`TrayWindowAccountMenu.qml`). Die alten Upstream-Dateien bleiben oft unbenutzt im Baum liegen (kompiliert, aber nicht mehr aus dem Render-Baum erreichbar) und werden bei jedem stable-Merge weiter von Nextcloud gepflegt — funktionale Fixes/Verbesserungen dort (nicht nur Styling) laufen sonst ins Leere, weil niemand hinschaut.

Dieser Skill hält eine Registry solcher Paare, prüft inkrementell (nur neue Commits seit der letzten Prüfung) auf funktionale Änderungen, und protokolliert jede Prüfung inkl. Entscheidung in einem Changelog — damit bewusste Abweichungen nachvollziehbar bleiben und nicht bei jeder Anfrage neu diskutiert werden müssen.

## Dateien in diesem Skill

- [registry.md](registry.md) — Liste der bekannten Paare (Upstream-Datei ↔ Fork-Ersatz), inkl. Notizen zu bekannten bewussten Abweichungen und dem zuletzt geprüften Upstream-Commit je Datei.
- [CHANGELOG.md](CHANGELOG.md) — chronologisches Protokoll jeder Prüfung: welche Commits geprüft wurden, und ob die Änderung "bereits portiert", "bewusst nicht portiert" (mit Grund/Ticket) oder "offener Portierungs-Kandidat" ist.

## Referenz-Branch

Dieser Skill nutzt denselben Referenz-Branch wie [stable-merge-check](../stable-merge-check/SKILL.md) — dort in `reference-branch.txt` nachsehen, nicht dupliziert pflegen. Vor der Analyse `git fetch origin <branch>` ausführen.

## Ablauf

1. **Referenz-Branch** aus `../stable-merge-check/reference-branch.txt` lesen und dem Nutzer nennen.
2. **`registry.md` einlesen** — für jedes Paar: Upstream-Datei(en), Fork-Ersatz-Datei(en), zuletzt geprüfter Commit.
3. **Pro Paar, neue Commits seit letzter Prüfung ermitteln** (rein inkrementell, kein Vollscan der Historie):
   - `git log --oneline <letzter-geprüfter-commit>..origin/<branch> -- <upstream-datei>`
   - Keine neuen Commits → Paar überspringen, nichts anfassen.
4. **Jeden neuen Commit einordnen:**
   - `git show <commit> -- <upstream-datei>` lesen.
   - **Rein kosmetisch/i18n/Refactor ohne Verhaltensänderung** (Property-Umbenennung ohne Semantikänderung, Textänderung, Formatierung) → kurz vermerken, kein Handlungsbedarf.
   - **Funktionale Änderung** (neue Bedingung/Guard, neues Signal, geändertes Verhalten, neue sichtbare Property): prüfen, ob das Verhalten im Fork-Ersatz bereits existiert — gezielt nach dem betroffenen Property-/Funktionsnamen in der Fork-Ersatzdatei suchen (Grep), ggf. auch in mitbenutzten geteilten Komponenten (z. B. `UserLine.qml`, das von mehreren Menü-Varianten genutzt wird).
     - **Bereits vorhanden** → "bereits portiert" vermerken, fertig.
     - **Fehlt, aber erkennbar bewusst** (Fork-Code hat an der fachlich passenden Stelle einen Kommentar mit Ticket-Referenz wie `//SES-4 removed`, `//SES-50`, oder das ganze Feature ist nachweislich per Produktentscheidung nicht vorhanden) → als "bewusst nicht portiert" mit Begründung/Ticket vermerken.
     - **Fehlt ohne erkennbaren Grund** → dem Nutzer als offenen Portierungs-Kandidaten vorschlagen, mit kurzer Einschätzung des Aufwands. Nicht automatisch patchen — erst nach Bestätigung umsetzen (wie bei Workflow A in `stable-merge-check`).
5. **Ergebnis mit dem Nutzer besprechen**, bestätigte Portierungen umsetzen.
6. **`CHANGELOG.md` ergänzen**: neuen Abschnitt mit Datum anlegen, pro Paar die geprüften Commits + Verdict auflisten (siehe Format in der Datei). Auch "bewusst nicht portiert"-Fälle explizit eintragen, nicht nur die nachgezogenen — genau das macht das Changelog wertvoll für spätere Nachvollziehbarkeit.
7. **`registry.md` aktualisieren**: "zuletzt geprüfter Commit" je Upstream-Datei auf den neuesten tatsächlich geprüften Commit setzen.
8. **Neue Paare aufnehmen**: Wenn der Nutzer im Gespräch eine weitere "durch eigene Entwicklung ersetzte" Komponente identifiziert, die noch nicht in `registry.md` steht, anbieten sie dort mit Startzustand (aktueller HEAD-Commit der Upstream-Datei als "zuletzt geprüft") einzutragen.

## Nicht in diesem Skill enthalten

- Keine automatische/periodische Ausführung — bewusst nutzergetriggert, typischerweise nach einem stable-Merge oder wenn der Verdacht aufkommt, dass eine ersetzte Komponente hinterherhinkt.
- Kein automatisches Patchen ohne Bestätigung.
- Prüft nicht, ob ein Paar selbst noch aktuell/korrekt ist (z. B. ob der Fork-Ersatz zwischenzeitlich erneut umbenannt wurde) — dafür `registry.md` bei Bedarf manuell korrigieren oder [gui-context-refresh](../gui-context-refresh/SKILL.md) für die generelle Strukturdoku nutzen.
