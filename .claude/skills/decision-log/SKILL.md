---
name: decision-log
description: Hält Entscheidungen zum Ausblenden/Entfernen ganzer Komponenten sowie bewusste Abweichungen von stable-x.y in DECISIONS.md fest, inkl. Begründung — bewusst eng gefasst, siehe stable-merge-check für die Detail-Merge-Risiko-Analyse. Kein allgemeines Änderungsprotokoll: Bugfixes, Kompilierbarkeits-Fixes und reine Vereinheitlichungs-/Aufräum-Refactorings gehören NICHT hinein. Proaktiv nutzen: wenn eine Komponente bewusst ausgeblendet statt entfernt wird (oder umgekehrt), oder eine Struktur bewusst nah an/abweichend von stable-x.y gehalten wird, einen Eintrag vorschlagen bzw. anlegen, ohne dass explizit danach gefragt werden muss. Auch manuell auslösbar. Trigger — "halte das im Entscheidungs-Log fest", "log diese Entscheidung", "warum haben wir X ausgeblendet/entfernt", "warum weichen wir hier von stable ab".
---

# Decision Log

Ziel ist **nicht** ein allgemeines Änderungsprotokoll, sondern gezielt zwei Dinge nachvollziehbar zu machen:

1. Warum eine ganze Komponente/ein ganzes Feature **ausgeblendet statt entfernt** wurde (oder umgekehrt entfernt statt nur ausgeblendet) — und was das für Nähe/Abstand zu stable-x.y bedeutet.
2. Warum eine Struktur bewusst **nah an stable-x.y gehalten** oder bewusst **davon abgewichen** wurde — relevant für spätere Merge-Konflikte oder Diagnose "was hat der Merge kaputt gemacht".

Ohne Protokoll geht diese Begründung verloren, und ein späteres Gespräch riskiert, eine bereits geprüfte Abweichung erneut aufzurollen oder eine bewusst ausgeblendete Komponente versehentlich zu entfernen (oder umgekehrt).

Dieser Skill pflegt dafür ein einziges, chronologisches Protokoll in [DECISIONS.md](DECISIONS.md).

## Wann ein Eintrag sinnvoll ist

- Eine **ganze Komponente/ein ganzes Feature** wird ausgeblendet statt entfernt (Code bleibt bestehen, nur `setVisible(false)`/Guard) — oder umgekehrt tatsächlich entfernt/gelöscht.
- Eine Struktur wird bewusst **nah an stable-x.y gehalten oder bewusst davon abgewichen**, mit Auswirkung auf künftige Merge-Konflikte (Überschneidung mit [stable-merge-check](../stable-merge-check/SKILL.md) — dort ggf. kurz verweisen statt inhaltlich zu duplizieren).
- Toter Code/eine Komponente wird bewusst **erhalten**, weil stable-x.y sie noch führt (Merge-Kompatibilität), obwohl sie im Fork ungenutzt wirkt.

**Nicht** eintragen (auch wenn eine Alternative erwogen wurde):
- Bugfixes jeder Art, auch wenn dabei eine Alternative verworfen wurde.
- Anpassungen, die nur die Kompilierbarkeit/den Build wiederherstellen (Includes, Case-Sensitivity, fehlende Pakete, Toolchain-Probleme).
- Rein mechanische Aufräumarbeiten (verwaiste Einzeldeklarationen, tote Funktionen ohne Komponentencharakter) — auch wenn "entfernt statt reaktiviert" wie eine Entscheidung aussieht, zählt das hier nicht, solange kein ganzes Feature/keine ganze Komponente betroffen ist.
- Reine Vereinheitlichungs-/Konsistenz-Refactorings ohne Bezug zu stable-x.y-Abweichung.
- Dev-Tooling/Build-Umgebung (Linux-Setup, CraftRoot, IDE-Konfiguration) — nichts davon betrifft Produktcode oder stable-Nähe.
- Triviale Ein-Weg-Fixes, reine Tippfehler-Korrekturen, Entscheidungen ohne echte Alternative.

**Faustregel bei Unsicherheit:** Würde ein späterer stable-x.y-Merge oder eine "warum ist Komponente X weg/da" Frage von diesem Eintrag profitieren? Wenn nein, gehört es nicht ins Log.

## Knapp halten (Token-Budget)

Diese Datei wird potenziell bei jedem neuen Eintrag gelesen — sie soll nicht zur Kopie des Gesprächsverlaufs werden.

- Jedes Feld (`Kontext`/`Entscheidung`/`Verworfene Alternative(n)`) **1–3 Sätze**, kein Fließtext-Absatz. Wenn es mehr braucht, gehört das Detail in den Commit/PR, nicht ins Log — hier nur die Kurzfassung + Referenz.
- Keine Code-Blöcke, keine Diffs, keine kompletten Funktionskörper zitieren. Stattdessen Datei:Zeile oder Commit-Hash referenzieren.
- Aufzählungen statt Prosa, wo es passt.
- Nicht dieselbe Begründung in mehreren Einträgen wiederholen — auf den früheren Eintrag verlinken (`siehe Eintrag vom YYYY-MM-DD`) statt sie erneut auszuformulieren.

## Ablauf

1. **Proaktiv erkennen**: Wenn im Gespräch eine Komponente ausgeblendet statt entfernt wird (oder umgekehrt), oder eine Struktur bewusst nah an/abweichend von stable-x.y gehalten wird, kurz anbieten oder direkt einen Eintrag ergänzen ("Ich halte das im Entscheidungs-Log fest.") — nicht erst warten, bis explizit danach gefragt wird. Bei allem anderen (Bugfixes, Build-Fixes, Aufräumarbeiten) nicht proaktiv anbieten, siehe "Wann ein Eintrag sinnvoll ist" oben.
2. **Vor einem neuen Eintrag gezielt prüfen, ob es schon einen zum selben Thema gibt** — nicht die komplette Datei lesen, sondern per Grep nach dem Thema/Dateinamen/Funktionsnamen suchen (z. B. `Grep "avatar" DECISIONS.md`). Nur bei echtem Treffer den betroffenen Abschnitt gezielt lesen. Das hält den Kontextverbrauch auch bei einer langen Datei klein.
3. **Neuen Abschnitt anhängen** (chronologisch, ans Ende) im Format:

   ```markdown
   ## YYYY-MM-DD — Kurzer Titel (Ticket-Referenz falls vorhanden)

   **Kontext:** 1–3 Sätze: Problem/Symptom, Auslöser.

   **Entscheidung:** 1–3 Sätze: was umgesetzt/gewählt wurde, ggf. Commit-Hash.

   **Verworfene Alternative(n):** 1–3 Sätze pro Alternative: was geprüft wurde und der konkrete Grund der Ablehnung.

   **Status:** aktiv | überholt durch [Datum/Titel] | offen
   ```

   Nicht jedes Feld ist immer nötig — "Verworfene Alternative(n)" nur, wenn es welche gab.
4. **Bei späterer Revision eines alten Eintrags**: nicht überschreiben, sondern neuen Abschnitt anhängen und im alten Eintrag `Status: überholt durch [Datum/Titel]` ergänzen, damit die Historie nachvollziehbar bleibt (wie ein Git-Log, nicht wie eine editierbare Wahrheit).
5. Kurz mit dem Nutzer bestätigen, dass der Eintrag so passt, besonders bei der Formulierung des "Warum" — das ist der Teil, der später zählt.

## Archivierung

Wenn `DECISIONS.md` über ca. 25 Einträge oder 300 Zeilen wächst: die ältesten Einträge mit `Status: überholt durch ...` (deren Begründung durch einen neueren Eintrag ohnehin abgedeckt ist) nach `DECISIONS-archive.md` verschieben, im Hauptfile nur einen Ein-Zeiler-Verweis lassen (`## YYYY-MM-DD — Titel → siehe DECISIONS-archive.md`). Aktive/offene Einträge bleiben immer im Hauptfile. Nur nach kurzer Rücksprache mit dem Nutzer durchführen, nicht automatisch.

## Abgrenzung

- Für stable-x.y-Vergleichs-/Merge-Risiko-Entscheidungen im Detail: [stable-merge-check](../stable-merge-check/SKILL.md).
- Für Protokoll zu Fork-Ersatzkomponenten vs. Upstream-Original: [shadow-component-watch](../shadow-component-watch/CHANGELOG.md).
- Für gebündelten Kontextabruf zu einer einzelnen Komponente (inkl. Auszug aus diesem Log): [component-context](../component-context/SKILL.md) — liest hier nur, schreibt nichts.
- Dieser Skill dupliziert deren Inhalte nicht, sondern verlinkt bei Bedarf darauf.
