---
name: component-context
description: Sammelt auf Zuruf vorhandenen Kontext zu EINER konkreten GUI-Komponente, bevor sie ausgeblendet oder in einem Subdialog geändert wird — aus DECISIONS.md, .claude/context/gui/, shadow-component-watch/registry.md, Git-Historie und einem per Code-Analyse ermittelten Klickpfad, wo die Komponente in der laufenden App zu finden ist. Schreibt nichts, liest nur und fasst zusammen. Nur manuell auslösen. Trigger — "gib mir Kontext zu <Komponente>", "was wissen wir schon über <Komponente>", "bevor ich <Komponente> anfasse", "History zu <Komponente>", "wurde <Komponente> schon mal ausgeblendet/geändert", "wo finde ich <Komponente> in der App", "wie komme ich zu <Komponente>", "Klickpfad zu <Komponente>".
---

# Component-Context

Beim UI-Theming (Ausblenden von Komponenten, Änderungen in Subdialogen) mit viel KI-Unterstützung geht schnell die Übersicht verloren, was an einer Komponente schon passiert ist — und wo sie in der laufenden App überhaupt zu finden ist. Dieser Skill sammelt vorhandenen Kontext zu **genau einer** Komponente aus bereits existierenden Quellen und ermittelt zusätzlich einen Klickpfad zur Komponente in der App, bevor du dort weiterarbeitest.

**Wichtig:** Dieser Skill schreibt selbst nichts fest und pflegt kein eigenes Registry-File. Er liest ausschließlich aus den Quellen, die [decision-log](../decision-log/SKILL.md), [gui-context-refresh](../gui-context-refresh/SKILL.md) und [shadow-component-watch](../shadow-component-watch/SKILL.md) ohnehin schon pflegen, plus Git-Historie live zusammen.

## Ablauf

1. **Komponente identifizieren.** Name/Klasse/Datei vom Nutzer übernehmen. Bei vager Beschreibung ("das Tray-Kontextmenü") zunächst per Grep in `.claude/context/gui/**/CLAUDE.md` und `COMPONENTS.md` nach passenden Zeilen suchen, um Datei(en) und Klassenname aufzulösen. Bei mehreren Kandidaten kurz nachfragen statt zu raten.

2. **Decision-Log durchsuchen** (nur gezielt, nicht komplett lesen): `Grep <Komponentenname/Klassenname> .claude/skills/decision-log/DECISIONS.md`. Bei Treffer(n) den jeweiligen Abschnitt lesen — liefert das "Warum" zu bewussten Ausblend-/Entfernungs-/Abweichungsentscheidungen.

3. **Kontext-Doku nachschlagen**: passenden Abschnitt in `.claude/context/gui/<ordner>/CLAUDE.md` oder die zugehörige Zeile in `.claude/context/gui/COMPONENTS.md` lesen (nicht die ganze Datei). Liefert Zweck, Zusammenspiel mit anderen Klassen, "Fork-spezifisch vs. Upstream"-Einordnung, ggf. Notes zu Dead Code. Auf das dort vermerkte "Stand"-Datum achten und im Ergebnis mit angeben, damit klar ist, wie aktuell die Doku ist.

4. **Shadow-Component-Check**: `Grep <Komponentenname> .claude/skills/shadow-component-watch/registry.md`. Falls die Komponente ein Fork-Replacement für eine Upstream-Komponente ist, kurz auf offene Einträge in `CHANGELOG.md` desselben Skills hinweisen.

5. **Git-Historie der Datei(en)**: `git log --oneline -n 10 -- <datei>` für einen schnellen Überblick, wer/wann zuletzt daran gearbeitet hat. Bei Bedarf (Nutzer fragt "wann wurde das ausgeblendet") gezielt `git log -S"<Suchbegriff, z.B. setVisible(false)>" -- <datei>` nachschieben.

6. **Nicht selbst tief in Merge-Risiko einsteigen.** Falls die Frage in Richtung "wie riskant ist das für den nächsten stable-Merge" geht, das kurz benennen und auf [stable-merge-check](../stable-merge-check/SKILL.md) verweisen statt die Analyse hier zu duplizieren.

7. **Klickpfad in der laufenden App ermitteln** (rein statisch per Code-Analyse, kein App-Start nötig):
   - **Aufrufstelle(n) finden**: repo-weit grep nach Instanziierung/Anzeige der Komponente — `new <Klasse>(`, `<Klasse>::` gefolgt von `show()`/`exec()`/`open()`, sowie in QML `<Komponente> {`, `Loader { source: ".../<Komponente>.qml" }`, `sourceComponent:`.
   - **Pro Aufrufstelle den Auslöser identifizieren**: die umgebende Funktion/den Slot ansehen — meist ein `connect(<QAction/Button>, &...::triggered/clicked, ...)` in C++ oder ein `onClicked:`/`onTriggered:` in QML.
   - **Sichtbares Label des Auslösers extrahieren**: in der Nähe nach `tr("...")`/`i18n(...)` (C++) bzw. der `text:`-Property (QML) suchen — das ist der Text, den der Nutzer tatsächlich anklickt.
   - **Nach oben verfolgen** (max. 3–4 Ebenen): denselben Schritt für den umgebenden Dialog/das umgebende Fenster wiederholen, bis ein bekannter Top-Level-Einstieg erreicht ist (Tray-Icon-Menü, `MainWindow.qml`/Tray-Popup, Settings-Dialog-Tab, Setup-Wizard-Schritt, Explorer/Finder-Kontextmenü). Die Ordnerzuordnung aus `.claude/context/gui/CLAUDE.md` (welcher Unterordner/Cluster) als Abkürzung/Plausibilitätscheck nutzen, ersetzt aber nicht das Nachverfolgen der konkreten Aufrufkette.
   - **Mehrere Einstiegspunkte** (Komponente von mehreren Stellen erreichbar, z. B. Dialog sowohl aus Tray als auch aus Kontextmenü öffenbar): alle auflisten, nicht nur einen auswählen.
   - **Plattform-Unterschiede** (macOS/Windows/Linux-Guards in der Aufrufkette) kurz vermerken, falls vorhanden.
   - **Ehrlich bleiben bei Unsicherheit**: lässt sich der Pfad nicht eindeutig statisch ermitteln (z. B. dynamisch aus Server-Capabilities generierte Menüeinträge, stark verschachtelte Delegates), das offen so benennen ("Klickpfad nicht eindeutig ermittelbar, vermutete Region: ...") statt einen Pfad zu erfinden — Datei/Klasse zum manuellen Nachschlagen trotzdem angeben.
   - Kein automatischer App-Start, kein Screenshot — das Ergebnis ist ein Text-Breadcrumb zum manuellen Nachklicken.

8. **Zusammenfassen**, kompakt und mit Quellenangabe pro Punkt (Datei:Zeile, DECISIONS.md-Datum, Commit-Hash), Format z. B.:

   ```markdown
   ## Kontext: <Komponente>

   **Datei(en):** ...
   **Status:** unangetastet | ausgeblendet seit ... | zuletzt geändert am ...

   **Navigation in der App:** Tray-Icon (Rechtsklick) > Konto-Menü > Einstellungen (Zahnrad) > Tab "Allgemein" > ...
   (bei mehreren Einstiegspunkten alle auflisten; bei Unsicherheit klar als "vermutet" kennzeichnen)

   **Bisherige Entscheidungen:** (aus DECISIONS.md, oder "keine Einträge gefunden")
   **Doku:** (Zweck/Zusammenspiel aus context/gui, Stand-Datum)
   **Shadow-Component:** (falls zutreffend, sonst weglassen)
   **Letzte Änderungen:** (2–3 relevante Commits)

   **Hinweis:** (z. B. "keine Vorgeschichte gefunden — Neuland" oder "Merge-Risiko nicht geprüft, siehe stable-merge-check" oder "nach dieser Änderung ggf. decision-log-Eintrag sinnvoll")
   ```

9. Wenn zu keiner Quelle etwas gefunden wird, das explizit sagen ("keine Vorgeschichte gefunden") statt die Abwesenheit von Treffern zu verschweigen — das ist selbst eine nützliche Information (Neuland, kein bekanntes Risiko). Gleiches gilt für den Klickpfad: "nicht ermittelbar" ist eine gültige und nützliche Antwort.

## Abgrenzung

- **decision-log**: hält das "Warum" von Ausblend-/Entfernungs-/Abweichungsentscheidungen fest. Dieser Skill liest daraus, schreibt aber nichts hinein — wird nach einer Änderung ein Eintrag fällig, decision-log dafür vorschlagen, nicht hier nachbilden.
- **gui-context-refresh**: pflegt die Strukturdoku unter `.claude/context/gui/`. Dieser Skill liest nur daraus und aktualisiert sie nicht.
- **stable-merge-check**: für Merge-Risiko-Analyse im Detail und Post-Merge-Diagnose. Bei Bedarf verlinken statt inhaltlich duplizieren.
- **shadow-component-watch**: für laufende Überwachung von Fork-Ersatzkomponenten gegen Upstream. Dieser Skill prüft nur kurz, ob die Komponente dort registriert ist.
- **color-scheme-map**: für die Farb-Property-Landkarte (welches Theme-Property, Light-/Dark-Wert, Bruch oder nicht) einer Komponente — dieser Skill liest davon nichts mit, bei Bedarf explizit verweisen.

## Nicht in diesem Skill enthalten

- Kein automatisches Loggen von Entscheidungen (macht decision-log).
- Keine Aktualisierung der Kontext-Doku (macht gui-context-refresh).
- Kein eigenes Registry-/Cache-File — jede Abfrage sammelt live aus den vorhandenen Quellen, es gibt nichts, das veralten kann außer den Quellen selbst (das gilt auch für den Klickpfad: wird bei jeder Abfrage neu aus dem Code hergeleitet, nicht zwischengespeichert).
- Kein automatisiertes Starten/Steuern/Screenshotten der laufenden App — dafür gibt es in diesem Projekt noch keine belastbare Grundlage (native Qt/QML-App, kein etabliertes UI-Automation-Setup). Der Klickpfad ist ein textueller Wegweiser zum manuellen Nachklicken, kein Beweis, dass er exakt stimmt — bei Unsicherheit im Ergebnis klar kennzeichnen.
- Keine Sammel-Tour über mehrere Komponenten (z. B. "alle in diesem Branch geänderten Stellen") — bewusst auf eine Komponente pro Abfrage begrenzt.
- Keine proaktive Auslösung — der Nutzer entscheidet bewusst, wann er Kontext abruft.
