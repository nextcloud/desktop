---
name: color-scheme-map
description: Pflegt eine Farbschema-Landkarte für src/gui/-Komponenten in COLOR_MAP.md — pro Komponente, welches Farb-Property sie verwendet, mit Light-/Dark-Wert und Quelle (themedColor() vs. hartcodiert vs. QPalette-Rolle) — um gezielt nach Dark-Mode-Brüchen suchen zu können. Wird bewusst Komponente für Komponente befüllt, kein Vollscan in einem Lauf. Nur manuell auslösen. Trigger — "erfasse die Farben von <Komponente>", "Farbschema-Map aktualisieren", "welche Farb-Properties nutzt <Komponente>", "such nach Dark-Mode-Brüchen in <Komponente>", nach Abschluss von Theming-Arbeiten (z. B. SES-578-artig) an einer einzelnen Komponente.
---

# Color-Scheme-Map

Beim Dark-Mode-Umbau (SES-578 u. ä.) taucht dieselbe Frage ständig neu auf: welches `WLTheme`/`BaseTheme`-Property färbt dieses Widget, hat es überhaupt einen Dark-Wert, oder hängt die Farbe an einer nicht mehr propagierten `QPalette`-Rolle? Bisher wurde das jedes Mal per Ad-hoc-Grep/Agent neu ermittelt. Dieser Skill baut dafür eine dauerhafte Landkarte auf: Komponente → verwendete Farb-Properties → Light-/Dark-Wert → Quelle → Status.

**Bewusst inkrementell, eine Komponente pro Aufruf** — keine Vollscan-Aktion über alle ~450 Dateien in `src/gui/`. Die Karte wächst mit jeder Komponente, die tatsächlich bearbeitet/geprüft wurde, nicht spekulativ im Voraus.

## Datei

- [COLOR_MAP.md](COLOR_MAP.md) — ein Abschnitt pro Komponente: Tabelle (Property | Datei:Zeile | Light-Wert | Dark-Wert | Quelle | Status) plus eine Zeile "zuletzt geprüfter Commit", analog zum `registry.md`-Muster von [shadow-component-watch](../shadow-component-watch/SKILL.md).

## Ablauf

1. **Komponente identifizieren.** Name/Klasse/Datei(en) vom Nutzer übernehmen. Bei vager Beschreibung wie bei [component-context](../component-context/SKILL.md) zunächst per Grep in `.claude/context/gui/**/CLAUDE.md`/`COMPONENTS.md` auflösen. Bei mehreren Kandidaten nachfragen statt zu raten.

2. **Prüfen, ob schon ein Abschnitt existiert** (`Grep <Komponente> COLOR_MAP.md`):
   - Vorhanden, mit "zuletzt geprüfter Commit" → nur inkrementell nachziehen: `git log <commit>..HEAD -- <Komponentendateien>`. Keine neuen Commits → nichts tun, das dem Nutzer sagen. Neue Commits → nur die dort geänderten Zeilen neu bewerten (Schritt 4), nicht die ganze Komponente neu scannen.
   - Noch nicht vorhanden → Vollerfassung dieser Komponente (Schritte 3–5).

3. **Scannen.** In den Komponentendateien (`.cpp`/`.h`/`.ui`/`.qml`) nach Farb-relevanten Mustern grep'en:
   - `WLTheme\.\w+\(` / `WLTheme\.\w+` (C++ und QML)
   - `themedColor\(` (direkter Aufruf, selten außerhalb von basetheme.h/stratotheme.h/ionostheme.h)
   - `QColor\(` / `#[0-9a-fA-F]{6}` (hartcodierte Hex-Werte)
   - `palette\(` (Qt-Stylesheet-CSS-Funktion) / `QPalette::\w+` (C++ Palette-Rolle)
   - `Style\.\w+` (QML-Singleton) / `Qt\.rgba\(` / `Qt\.lighter\(` / `Qt\.darker\(` / `Qt\.tint\(`
   - `setStyleSheet\(` -Aufrufe, die `color:`/`background-color:`/`border-color:` enthalten

4. **Jeden Treffer auflösen und einordnen:**
   - Ruft einen `WLTheme`-Getter, der intern `themedColor(light, dark)` nutzt → beide Hex-Werte aus `basetheme.h`/`stratotheme.h`/`ionostheme.h` übernehmen, Status **✅ theme-aware**.
   - Hartcodierter Einzelwert (Hex/`QColor(...)`/`Qt.rgba(...)` ohne `Theme.darkMode`/`Style.darkMode`-Verzweigung) → Status **⚠️ Bruch: kein Dark-Wert**, außer ein Kommentar begründet das explizit als bewusst (z. B. "fixed contrast pair, deliberately not themed" wie bei den Error-Styles) → dann **ℹ️ bewusst fix**, mit Begründung in der Notiz-Spalte.
   - `palette(...)`/`QPalette::\w+` ohne eigene Farbwerte → Status **ℹ️ ambient**, mit Vermerk, ob das Widget nachweislich eine aktuelle Palette bekommt (kurzer Blick auf `customizeStyle()`/`changeEvent()` der Komponente bzw. ihres Parents) oder ob das ungeprüft ist.
   - Aufruf eines anderen `WLTheme`-Getters ohne erkennbares `themedColor` (z. B. weil der Getter selbst noch nicht auf `themedColor` umgestellt wurde) → Status **⚠️ Bruch: Getter selbst hat keinen Dark-Wert**, Fundstelle in `basetheme.h`/`stratotheme.h` mit angeben.

5. **`COLOR_MAP.md` schreiben/ergänzen:** neuen Abschnitt (oder aktualisierte Zeilen im bestehenden) für die Komponente anlegen, "zuletzt geprüfter Commit" auf den aktuellen `HEAD` der geprüften Dateien setzen.

6. **Brüche hervorheben.** Am Ende dem Nutzer explizit alle **⚠️**-Zeilen dieser Komponente nennen — das ist der eigentliche Zweck der Karte, nicht nur die Tabelle selbst.

7. **Nicht automatisch fixen.** Gefundene Brüche nur melden und in der Karte vermerken; ein Fix läuft wie gewohnt erst nach Bestätigung durch den Nutzer als eigener Schritt.

## Abgrenzung

- **gui-context-refresh**: Zweck/Struktur/Zusammenspiel einer Komponente — dieser Skill befasst sich ausschließlich mit ihren Farb-Properties, nicht mit ihrer sonstigen Doku.
- **stable-merge-check**: ob eine Farb-/Style-Änderung strukturell von `stable-x.y` abweicht — dieser Skill bewertet nur, ob eine Farbe dark-mode-tauglich ist, nicht das Merge-Risiko.
- **component-context**: bündelt vorhandenen Kontext vor einer Änderung, liest aber nur — dieser Skill ist die einzige Quelle, die die Farb-Landkarte selbst schreibt/pflegt.
- **merge-drift-map**: gleiches Muster (inkrementelle, dauerhafte Landkarte), aber für Abweichung von `stable-x.y` statt Farb-Properties — unabhängige Dimension, kein Zusammenhang zwischen den beiden Karten.

## Nicht in diesem Skill enthalten

- Kein Vollscan aller Komponenten in einem Lauf — bewusst Komponente für Komponente, wie vom Nutzer festgelegt.
- Kein automatisches Fixen gefundener Brüche.
- Keine Bewertung, ob eine Farbwahl *gestalterisch* richtig ist (Kontrast/Barrierefreiheit) — nur, ob überhaupt ein Dark-Wert existiert und woher er kommt.
- Kein periodischer/automatischer Trigger — der Nutzer entscheidet, wann eine Komponente erfasst/aktualisiert wird.
