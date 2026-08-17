---
name: decision-log
description: Hält nennenswerte Implementierungs-Entscheidungen und bewusst verworfene Alternativen in DECISIONS.md fest, inkl. Begründung — nicht nur stable-Vergleiche (dafür siehe stable-merge-check), sondern jede Design-/Architektur-/UI-Entscheidung, bei der mehrere echte Optionen abgewogen wurden. Proaktiv nutzen: wenn eine Alternative nach Diskussion oder Test verworfen wird ("nicht zufriedenstellend", "das passt nicht", "lass uns bei X bleiben"), einen Eintrag vorschlagen bzw. anlegen, ohne dass explizit danach gefragt werden muss. Auch manuell auslösbar. Trigger — "halte das im Entscheidungs-Log fest", "log diese Entscheidung", "warum haben wir uns für X entschieden", "haben wir das schon mal probiert", nach einem verworfenen Lösungsansatz, vor einem Commit der eine von mehreren erwogenen Lösungen umsetzt.
---

# Decision Log

Bei Implementierungsarbeit fallen laufend Entscheidungen zwischen mehreren echten Alternativen — manche werden getestet und wieder verworfen (z. B. "Option 2 sieht doch nicht gut aus, zurück zu Option 1"). Ohne Protokoll geht die Begründung verloren, und ein späteres Gespräch (auch mit anderem Kontext-Fenster oder anderer Person) probiert die bereits verworfene Idee erneut aus, ohne zu wissen, dass sie schon geprüft und aus einem konkreten Grund abgelehnt wurde.

Dieser Skill pflegt dafür ein einziges, chronologisches Protokoll in [DECISIONS.md](DECISIONS.md).

## Wann ein Eintrag sinnvoll ist

- Es gab **mindestens zwei echte Alternativen**, und eine wurde bewusst gewählt oder verworfen.
- Eine bereits umgesetzte/getestete Lösung wird **verworfen** ("nicht zufriedenstellend", "das brauchen wir nicht", "das passt nicht") — der Grund ist der wertvollste Teil des Eintrags.
- Eine Abweichung von stable-x.y wird bewusst in Kauf genommen (Überschneidung mit [stable-merge-check](../stable-merge-check/SKILL.md) — dort ggf. kurz verweisen statt inhaltlich zu duplizieren).
- Eine überraschende Erkenntnis während der Bewertung fällt an (z. B. "Funktion X ignoriert Parameter Y komplett"), die spätere Arbeit beeinflusst.

**Nicht** eintragen: triviale Ein-Weg-Fixes, mechanische Refactorings, reine Tippfehler-Korrekturen, Entscheidungen ohne echte Alternative.

## Knapp halten (Token-Budget)

Diese Datei wird potenziell bei jedem neuen Eintrag gelesen — sie soll nicht zur Kopie des Gesprächsverlaufs werden.

- Jedes Feld (`Kontext`/`Entscheidung`/`Verworfene Alternative(n)`) **1–3 Sätze**, kein Fließtext-Absatz. Wenn es mehr braucht, gehört das Detail in den Commit/PR, nicht ins Log — hier nur die Kurzfassung + Referenz.
- Keine Code-Blöcke, keine Diffs, keine kompletten Funktionskörper zitieren. Stattdessen Datei:Zeile oder Commit-Hash referenzieren.
- Aufzählungen statt Prosa, wo es passt.
- Nicht dieselbe Begründung in mehreren Einträgen wiederholen — auf den früheren Eintrag verlinken (`siehe Eintrag vom YYYY-MM-DD`) statt sie erneut auszuformulieren.

## Ablauf

1. **Proaktiv erkennen**: Wenn im Gespräch eine Alternative verworfen wird oder eine von mehreren Optionen bestätigt wird, kurz anbieten oder direkt einen Eintrag ergänzen ("Ich halte das im Entscheidungs-Log fest.") — nicht erst warten, bis explizit danach gefragt wird.
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
- Dieser Skill dupliziert deren Inhalte nicht, sondern verlinkt bei Bedarf darauf.
