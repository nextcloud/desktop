---
name: merge-drift-map
description: Pflegt eine Landkarte in DRIFT_MAP.md, die pro Datei/Klasse zeigt, wie stark sie gegenüber stable-33.0 abweicht — als zwei getrennte Werte: Diff-Anteil (wie oft ein Merge hier vermutlich kollidiert) und qualitative Risiko-Einstufung aus stable-merge-check (wie gefährlich eine Kollision wäre). Wird bewusst Datei für Datei/Komponente für Komponente befüllt, kein Vollscan. Nur manuell auslösen. Trigger — "wie stark weicht <Datei/Komponente> von stable ab", "Drift-Map aktualisieren", "Merge-Risiko-Landkarte", "wo sind Brüche zu stable-33.0 zu erwarten".
---

# Merge-Drift-Map

`stable-merge-check` beantwortet die Frage "wie riskant ist diese eine Datei" sehr gut, aber nur als einmalige Antwort im Gespräch — sie geht danach verloren. Dieser Skill hält das Ergebnis dauerhaft fest, damit sich über die Zeit eine Übersicht aufbaut, welche Dateien wie stark umgebaut sind und wo bei künftigen `stable-x.y`-Merges am ehesten Ärger zu erwarten ist.

**Zwei getrennte Werte, bewusst nicht zu einer Kennzahl verschmolzen** (Diskussion dazu: reine Diff-Größe allein ist irreführend — `folderwizard.cpp` hat einen sehr hohen Diff-Anteil und ist trotzdem unkritisch, während die frühere `isWindows11OrGreater()`-Verlagerung nur 3 Zeilen Diff war, aber ein echter struktureller Rückschritt gegenüber stable):

- **Diff-Anteil** (geänderte Zeilen ÷ Zeilen auf `stable-33.0`): wie *oft* ein Merge hier vermutlich einen Textkonflikt auslöst — sagt nichts darüber, wie gefährlich der wäre.
- **Qualitative Einstufung** (niedrig/mittel/hoch, aus [stable-merge-check](../stable-merge-check/SKILL.md)s Workflow A): wie *gefährlich* eine Abweichung tatsächlich ist — additive Whitelabel-Ergänzungen sind unkritisch, umbenannte/umstrukturierte Stellen können hart kollidieren oder sich sogar unbemerkt falsch zusammenführen.

Kombiniert: niedrig+niedrig = unauffällig, hoch+niedrig = viele harmlose Konflikte, niedrig+hoch = seltene, aber gefährliche Stellen, hoch+hoch = besondere Vorsicht.

**Bewusst inkrementell, Datei/Komponente für Komponente** — kein Vollscan über das ganze Repo in einem Lauf.

## Datei

- [DRIFT_MAP.md](DRIFT_MAP.md) — Legende (die Erläuterung oben) einmal am Anfang, danach pro Komponente eine Tabelle: Datei | Auf stable? | Diff-Anteil | Qualitative Einstufung | Bewusst/Drift | Bemerkung, plus "zuletzt geprüfter Commit" (eigener und stable-Seite).

## Referenz-Branch

Wie [stable-merge-check](../stable-merge-check/SKILL.md): Branch aus dessen `reference-branch.txt` lesen, dem Nutzer nennen, vorher `git fetch origin <branch>` (rein lesend).

## Ablauf

1. **Datei(en)/Komponente identifizieren.** Vom Nutzer übernehmen, bei vager Beschreibung wie bei [component-context](../component-context/SKILL.md) über `.claude/context/gui/**/CLAUDE.md`/`COMPONENTS.md` auflösen.

2. **Prüfen, ob schon erfasst** (`Grep <Datei> DRIFT_MAP.md`):
   - Vorhanden mit "zuletzt geprüfter Commit" (eigene Seite und stable-Seite) → nur inkrementell nachziehen: `git log <eigener-commit>..HEAD -- <datei>` und `git log <stable-commit>..origin/<branch> -- <datei>`. Keine neuen Commits auf beiden Seiten → nichts tun. Neue Commits → nur die betroffene Zeile neu bewerten, nicht die ganze Datei neu rechnen.
   - Noch nicht erfasst → Vollerfassung (Schritte 3–5).

3. **Existenz prüfen:** `git cat-file -e origin/<branch>:<datei>`. Existiert die Datei auf stable nicht → Diff-Anteil ist **n/a (fork-only)**, qualitative Einstufung **keins von stable-Seite**, fertig — kein Konflikt möglich, den es nicht schon durch die reine Existenz der Datei gäbe.

4. **Diff-Anteil berechnen** (existiert die Datei):
   - Zeilenzahl auf stable: `git show origin/<branch>:<datei> | wc -l`
   - Änderungsumfang: `git diff --shortstat origin/<branch> HEAD -- <datei>`
   - Anteil = (insertions + deletions) ÷ Zeilenzahl auf stable, grob gerundet in Prozent.

5. **Qualitative Einstufung ermitteln:** Workflow A aus [stable-merge-check](../stable-merge-check/SKILL.md) anwenden (nicht hier duplizieren) — `git diff origin/<branch> -- <datei>` lesen, niedrig/mittel/hoch einordnen, bewusst vs. Drift vermerken. Bei Unsicherheit oder wenn nur ein Teil der Datei bisher inhaltlich geprüft wurde, ehrlich **"nicht vollständig geprüft"** statt eine Einstufung zu raten.

6. **`DRIFT_MAP.md` schreiben/ergänzen:** Zeile/Abschnitt für die Datei/Komponente anlegen, beide "zuletzt geprüft"-Commits (eigener HEAD, stable-Seite) setzen.

7. **Auffällige Kombinationen hervorheben.** Am Ende dem Nutzer explizit die hoch+hoch-Fälle nennen (besondere Vorsicht) sowie neue niedrig+hoch-Fälle (selten, aber gefährlich) — das sind die eigentlich interessanten Ergebnisse, nicht die Tabelle an sich.

## Abgrenzung

- **stable-merge-check**: liefert die qualitative Methodik und macht die tiefergehende Post-Merge-Diagnose (Workflow B) — dieser Skill übernimmt nur deren Ergebnis in die Landkarte, dupliziert die Methode nicht.
- **color-scheme-map**: gleiches Muster (inkrementelle, dauerhafte Landkarte), aber für Farb-Properties statt Merge-Drift — unabhängige Dimension, kein Zusammenhang zwischen den beiden Karten.
- **gui-context-refresh**: Struktur/Zweck einer Komponente — nicht deren Abweichung von stable.

## Nicht in diesem Skill enthalten

- Keine eigene Risikomethodik — reine Diff-Größe wird nie allein als Risikoaussage verwendet, immer nur neben der qualitativen Einstufung aus `stable-merge-check`.
- Kein Vollscan aller Dateien in einem Lauf.
- Kein automatisches Fixen gefundener Risiken.
- Kein periodischer/automatischer Trigger.
