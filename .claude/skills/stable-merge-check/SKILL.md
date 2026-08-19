---
name: stable-merge-check
description: Vergleicht GUI-/Code-Änderungen in diesem Fork mit dem konfigurierten upstream stable-x.y-Branch, um Merge-Konflikt-Risiken durch Strukturabweichungen zu erkennen, merge-robuste Lösungsvorschläge zu machen, und um nach einem Merge kaputte Stellen (doppelte Layouts, verlorenes/zerschossenes Styling, kaputte Features) durch Vergleich mit beiden Merge-Elternversionen zu diagnostizieren. Trigger — "vergleiche mit stable", "Merge-Konflikt-Risiko", "nach dem Merge kaputt", "Struktur-Analyse gegen stable-33.0", "was hat der Merge kaputt gemacht", "merge-robuste Lösung", UI sieht nach einem Merge falsch/zerschossen aus, doppelte setLayout()-Warnungen, fehlende Elemente nach Merge (Search Bar, Quota-Anzeige, Avatar/Initialen).
---

# Stable-Merge-Check

Dieses Repo ist ein Whitelabel-Fork von Nextcloud Desktop (u.a. für IONOS/HiDrive Next, BRICKMAKERS-Kontext). Es wird regelmäßig gegen den upstream `stable-x.y`-Branch von Nextcloud gemerged. Zwei wiederkehrende Probleme:

1. Eigene GUI-Anpassungen weichen strukturell so stark vom stable-Branch ab, dass künftige Merges unnötig konfliktträchtig werden.
2. Nach einem Merge ist etwas kaputt (doppelte Widgets/Layouts, verlorenes Styling, fehlende Features), weil der Merge beide Seiten unglücklich zusammengeführt hat.

Dieser Skill deckt beide Fälle ab.

## Referenz-Branch

Der Vergleichs-Branch steht in [reference-branch.txt](reference-branch.txt) (aktuell `stable-33.0`). Er gilt für einen ganzen Entwicklungszyklus und wird bewusst nicht automatisch ermittelt.

- **Vor jeder Analyse**: Datei lesen und dem Nutzer explizit mitteilen, gegen welchen Branch gerade verglichen wird (z. B. "Ich vergleiche gegen `origin/stable-33.0`."), damit das nie stillschweigend passiert.
- **Wenn der Nutzer signalisiert, dass sich der relevante stable-Branch geändert hat** (neuer Release-Branch geschnitten, z. B. "wir sind jetzt auf stable-34.0"): kurz bestätigen lassen, dann `reference-branch.txt` aktualisieren. Das ist eine geteilte, eingecheckte Einstellung fürs ganze Team — nicht ungefragt ändern.
- Vor dem eigentlichen Vergleich `git fetch origin <branch>` ausführen (rein lesend, unkritisch), damit nicht gegen einen veralteten lokalen Stand verglichen wird. Falls `origin/<branch>` lokal nicht existiert, das dem Nutzer melden statt zu raten.

## Workflow A — Vorab-Struktur-/Merge-Risiko-Analyse

Einsatz: Der Nutzer arbeitet an einer GUI-Datei/Komponente (typischerweise unter `src/gui/`, oft `.ui`, `.qml`, `.cpp`) und will wissen, wie riskant aktuelle/geplante Änderungen für künftige Merges sind, oder will die Struktur bewusst näher an stable heranziehen.

1. Referenz-Branch ansagen (s.o.).
2. Betroffene Datei(en)/Klasse identifizieren — aus IDE-Selection/offener Datei übernehmen, sonst nachfragen.
3. Vergleichen:
   - `git diff origin/<branch> -- <file>` für den reinen Inhalt.
   - `git log --oneline origin/<branch> -- <file>` bzw. `git log --oneline -- <file>` um zu verstehen, wann/warum divergiert wurde.
4. Abweichungen einordnen, nicht nur auflisten:
   - **Niedriges Risiko**: reine Ergänzungen (neue Property, neues optionales Element) ohne Umbau bestehender Struktur.
   - **Mittleres Risiko**: umbenannte/verschobene Bezeichner, die an anderer Stelle im Code referenziert werden (vorher per Grep prüfen, ob es weitere Verwendungsstellen gibt).
   - **Hohes Risiko**: Umstrukturierung bestehender Layout-Hierarchien/Reihenfolgen (z. B. andere Gruppierung von Widgets, dynamisches statt fixes Spacing, verschobene Sections) — das sind erfahrungsgemäß die Stellen, an denen künftige stable-Merges kollidieren.
   - Für jede Abweichung kurz einordnen, ob sie **bewusst** ist (Branding/Feature-Unterschied dieses Forks, z. B. entfernter "Neues Konto"-Button, andere Datenschutz-Formulierung) oder **zufällige Drift**.
   - Wenn eine Abweichung darauf hindeutet, dass eine Datei/Komponente durch eine neuere ersetzt wurde und ungenutzt wirkt (z. B. altes Widget nicht mehr im aktiven Render-Baum): **vor einer Entfernungsempfehlung reposweit per Grep** (Dateiname, Klassenname, QML-Typname) auf verbleibende Referenzen prüfen — nicht allein aus dem Struktur-Diff auf "toter Code" schließen.
   - **Entscheidend zusätzlich: prüfen, ob `origin/<branch>` dieselbe Datei/Klasse noch führt und aktiv nutzt** (`git show origin/<branch>:<file>`, ggf. dort ebenfalls auf Referenzen grep'en). Ziel ist maximale Nähe zu stable, nicht minimaler eigener Code:
     - Führt stable die Datei noch aktiv → **nicht löschen**, auch wenn sie im Fork ungenutzt wirkt. Entfernen würde den Diff zu stable vergrößern statt verkleinern. Stattdessen nur als "im Fork ungenutzt, aber bewusst erhalten für Merge-Kompatibilität mit stable" dokumentieren.
     - Hat stable die Datei ebenfalls entfernt/nicht mehr aktiv verdrahtet → Entfernung bringt uns näher an stable; nur dann Löschung als merge-robust vorschlagen, weiterhin nur nach Bestätigung durch den Nutzer.
     - Bei Unsicherheit: nicht löschen. Nähe zu stable schlägt Code-Hygiene.
5. Höchstens 2–3 Lösungsvorschläge machen, nicht mehr. Jeder Vorschlag mit einer knappen Aufwand-vs-Konfliktrisiko-Einschätzung.
   - Standard-Bias: Die Option bevorzugen, die die Struktur so nah wie möglich an stable hält, auch wenn sie gerade etwas mehr Aufwand bedeutet — das zahlt sich bei jedem künftigen Merge aus. Optionen, die zwar einfach umzusetzen sind, aber die Struktur dauerhaft von stable wegbewegen, explizit als "hoher Wartungsaufwand bei künftigen Merges" kennzeichnen statt sie gleichwertig zu präsentieren.
   - Erst nach Bestätigung durch den Nutzer implementieren, nicht direkt lospatchen.
6. Bei größeren strukturellen Umbauten: nach jedem stabilen Zwischenschritt einen Checkpoint-Commit vorschlagen ("soll ich das erstmal committen, bevor wir weitermachen?"), damit Zwischenstände nicht verloren gehen.

## Workflow B — Post-Merge-Schadensdiagnose

Einsatz: Nach einem Merge von `origin/<branch>` funktioniert/sieht etwas nicht mehr richtig aus (Qt-Warnung zu doppeltem Layout, zerschossenes Styling, fehlende Suchleiste/Quota-Anzeige, kaputtes Feature).

1. Referenz-Branch ansagen (s.o.).
2. Merge-Commit identifizieren: `git log --merges -1 -- <file>` (oder den Commit-Hash vom Nutzer erfragen, falls nicht der letzte).
3. Beide Merge-Elternversionen der betroffenen Datei/Stelle gegenüberstellen:
   - `git show <merge-commit>^1:<file>`
   - `git show <merge-commit>^2:<file>`
   - `git show <merge-commit>:<file>` (das tatsächliche Merge-Ergebnis)
   - Klassisches Muster: beide Elternversionen haben unabhängig voneinander dieselbe Art Änderung eingebaut (z. B. je ein `setLayout()`-Aufruf), und der Merge hat beide behalten statt nur einen.
4. Mit der konkreten Fehlerbeschreibung des Nutzers abgleichen (Fehlermeldung, Screenshot-Beschreibung, betroffenes UI-Element) — nicht raten, sondern die exakte Stelle über `git blame` auf der aktuellen Datei lokalisieren, um zu sehen, welche Zeile von welcher Seite stammt.
5. Den konkreten Fix vorschlagen (i. d. R. die überzählige/widersprüchliche Zeile entfernen) und kurz erklären, welches Merge-Muster dazu geführt hat, damit der Nutzer es beim nächsten Mal selbst erkennt.
6. Nach dem Fix: kurz gegen `origin/<branch>` und den vorherigen (funktionierenden) Stand gegenchecken, dass keine weiteren Stellen betroffen sind.
