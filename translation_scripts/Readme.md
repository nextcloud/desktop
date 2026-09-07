# Lokalisierung des HiDrive Next Clients auf Basis einer Diff-Datei

Diese Anleitung beschreibt den Prozess zur Lokalisierung des HiDrive Next Clients mithilfe einer von uns erstellten Diff-Datei. Die Lokalisierung erfolgt auf Basis von `.ts`-Dateien, welche die Ressourcen der Anwendung in XML-Form enthalten.

## Voraussetzungen

- Nextcloud Stable Client Quellcode
- Python-Script `merge_translation.py`
- Qt Linguist Tools (insbesondere `lupdate`)

# Vorgehen bei Release (obsolet)

> **Obsolet, siehe [Automatisierung: Merge-Treiber + post-merge Hook](#automatisierung-merge-treiber--post-merge-hook).** Der separate `translations_<version source>`-Branch samt PR zurück in Richtung NC-Basisversion wird nicht mehr gebraucht: der Abgleich mit dem jeweils aktuellen NC-Basisstand passiert seitdem inplace bei jedem Merge eines `stable-x.y`-Branches, automatisch über den post-merge-Hook. Der folgende Ablauf bleibt hier nur als Referenz/Historie stehen.

Um in einem Release zu erstellen und einen valider PR zur Übersetzung zu haben ist folgendes Vorgehen notwendig:

0. (Optional) Einbeziehen unserer Änderungen aus Phrase. Dieser Schritt ist optional, da die Änderungen in der Regel schon in der Diff-Datei enthalten sind. Sollte es dennoch notwendig sein, können die Änderungen aus Phrase in die `.ts`-Diff-Dateien gemerged werden.
1. Es wird ein neuer `translations_<version source>` branch erstellt. Abgeleitet vom entsprechenden `develop_<version source>`. (z.b. translations_stable-3.16)
2. Durchlaufen der unter stehen den Schritte 1-6
3. Erstellen eines "approved" PRs von `translations_<version source>` nach `<version source>`, also in Richtung der eigentlichen Basisversion von nc (z.B. [stable-3.16] Translations)
4. Der PR wird dann vom Brander gemerged

Das Rebasedn der Translation-Branches lohnt sich eigentlich nicht, weil der nextcloud master sich relativ häufig ändert, was zu vielen Konflikten führen würde.

## Automatisierung: Merge-Treiber + post-merge Hook

Beim Mergen eines `stable-x.y`-Branches (neue NC-Basisversion) in einen Feature-/Entwicklungs-Branch (oder z.B. auch `develop` in einen Feature-Branch) müssen die `client_*.ts`-Dateien gegen die neue Basis abgeglichen werden. Das übernehmen zwei zusammenspielende Mechanismen:

### 1. Merge-Treiber für `translations/client_*.ts`

Diese Dateien sind maschinell generierte, stark umsortierte XML-Dateien. Ein normaler zeilenbasierter 3-way-Merge (Git-Standardverhalten) kann Message-Blöcke falsch ausrichten und Konflikte erzeugen, deren Auflösung die Datei inhaltlich beschädigt, ohne dass es auffällt (die Validierung im Skript prüft nur Struktur/Konsistenz, nicht ob eine Übersetzung noch zum richtigen Source-Text gehört). Deshalb wird für diese Dateien gar kein inhaltlicher Merge mehr versucht: `.gitattributes` markiert sie mit einem eigenen Treiber, der bei jedem Merge **immer die eingehende Seite** (das, was gerade reingemerged wird) 1:1 übernimmt – unabhängig von der Richtung. Das garantiert z.B. auch, dass ein Merge von `develop` in einen Feature-Branch dort den korrekten, aktuellen Stand ankommen lässt.

**Einmalige Einrichtung pro Clone** (der Treiber-Name in `.gitattributes` ist bereits eingetragen, nur das Kommando dahinter muss lokal registriert werden – analog zu `core.hooksPath`):

```
git config merge.nc-take-incoming.driver "cp -- '%B' '%A'"
```

### 2. `post-merge`/`post-commit` Hook

- Läuft automatisch nach jedem lokalen `git merge`/`git pull` (bzw. nach dem manuellen Abschluss eines Merges, der wegen Konflikten in *anderen* Dateien als den Übersetzungen manuell committet werden musste).
- Erkennt anhand der Merge-Commit-Message, ob ein `stable-x.y`-Branch gemerged wurde (z.B. `Merge branch 'stable-33.0' into ...`).
- Führt in diesem Fall automatisch `merge_translation.py auto` aus (**ohne** `--auto-commit`, **ohne** Branch-Argument).
- Änderungen an `translations/client_*.ts` liegen danach ungestaged im Working Directory und müssen manuell geprüft und committet werden.

`auto` durchläuft dabei Schritt 0 (jetzt nur noch Normalisieren/Sortieren der frisch eingemergten Datei, siehe unten) und Schritte 1–5 wie gewohnt.

**Einmalige Aktivierung pro Clone:**

```
git config core.hooksPath .githooks
```

## Allgemeines

Die Lokalisierung erfolgt in mehreren Schritten. 
Die Qt-Translation Files (`.ts`-Dateien) enthalten zu jeder Resource die entsprechnde Datei und die Zeilennummer. Diese Informationen entfernen wir für eine bessere Vergleichabrkeit.
*Die `.ts`-Dateien müssen vor jedem Merge-Schritt sortiert werden.* Dies geschieht in der Regel durch das Skript selbst.

## Schritte

### Automatischer Durchlauf

Alle Schritte (0–5) können mit einem einzigen Befehl ausgeführt werden. Mit `--auto-commit` wird nach jedem Schritt automatisch committet:

```
python3 merge_translation.py all --auto-commit
```

### Einzelne Schritte

Die Schritte können auch einzeln ausgeführt werden. Mit `--auto-commit` entfällt das manuelle Committen:

```
python3 merge_translation.py 1 --auto-commit
```

### 1. Eingemergte Übersetzungen normalisieren

- Dank des Merge-Treibers (siehe oben) enthält `translations/client_*.ts` an dieser Stelle bereits 1:1 den Stand der Seite, die gerade eingemergt wurde – es muss also nichts mehr rekonstruiert werden, kein Branch-Argument nötig.
- Das Skript sortiert die Dateien nur in unsere kanonische Reihenfolge/Formatierung, damit der Diff von Schritt 1 nachher nur echte `lupdate`-Änderungen zeigt statt Sortier-Rauschen:

```
python3 merge_translation.py 0
```

- Committen (STEP 0) — oder `--auto-commit` verwenden.

### 2. Merge-Schritt 1

- Verwenden des Python-Skripts `merge_translation.py` mit Parameter `1`:

```
python3 merge_translation.py 1
```

- Führt ein `lupdate` auf dem **HiDrive Next Client** aus.
- Obsolete Einträge werden **nicht gelöscht**.
- Neue Keys werden hinzugefügt.
- Obsolete-Markierungen werden entfernt und die Datei wird sortiert.
- Committen (STEP 1) — oder `--auto-commit` verwenden.

### 3. Merge-Schritt 2

- Verwenden des Skripts mit Parameter `2`:

```
python3 merge_translation.py 2
```

- Führt ein `lupdate` aus.
- **Obsolete Keys werden entfernt**.
- Die Datei enthält jetzt nur die aktuellen Keys (unsere Keys ohne Übersetzungen).
- Committen (STEP 2) — oder `--auto-commit` verwenden.

### 4. Merge-Schritt 3

- Verwenden des Skripts mit Parameter `3`:

```
python3 merge_translation.py 3
```

- Sprachabhängige Diff-Dateien werden in die `.ts`Dateien gemergt.
- Leere Keys werden mit Übersetzungen gefüllt.
- Obsolete Keys aus der Diff-Datei werden gegebenenfalls eingefügt.
- Committen (STEP 3) — oder `--auto-commit` verwenden.

### 5. Merge-Schritt 4

- Verwenden des Skripts mit Parameter `4`:

```
python3 merge_translation.py 4
```

- Ein weiteres `lupdate` wird ausgeführt.
- Heuristische Füllung von doppelten Keys mit unseren Übersetzungen.
- Committen (STEP 4) — oder `--auto-commit` verwenden.

### 6. Merge-Schritt 5

- Verwenden des Skripts mit Parameter `5`:

```
python3 merge_translation.py 5
```

- **Obsolete Keys werden endgültig entfernt**.
- Committen (STEP 5) — oder `--auto-commit` verwenden.

## Abschluss

Nach dem letzten Schritt sind die `.ts`-Dateien vollständig lokalisiert, enthalten unsere Keys und Übersetzungen und sind frei von obsolete Keys.

## Historie: entfernte Mechanismen

Zwei frühere Mechanismen zum Abgleich mit dem NC-Basisstand wurden entfernt, weil der neue Merge-Treiber (siehe oben) das eigentliche Problem, das sie lösen sollten, gar nicht mehr entstehen lässt:

- **Ursprünglich (manuell):** den `stable-x`-Branch auschecken, den `lupdate`-Befehl von Hand gegen den dort ausgecheckten Quellcode laufen lassen, die Änderung stashen, zurück auf den eigenen Branch wechseln und den Stash anwenden.
- **Danach (automatisiert, `git worktree`):** Step 0 hat denselben Ablauf automatisiert – über ein temporäres `git worktree`-Checkout von `nc_branch` wurde `lupdate` gegen dessen Quellcode ausgeführt, mit `-ts` aber direkt auf die eigenen `client_*.ts`-Dateien zeigend.

Beide Varianten haben ausschließlich die *Menge* der Source-Strings synchronisiert (neu/entfernt/Location) – `lupdate` liest nie den Übersetzungstext einer anderen `.ts`-Datei, auch nicht den von `stable-x`. Der eigentliche NC-Übersetzungsstand kam bisher nur über den normalen Git-Merge von `client_*.ts` selbst ins Repo – und genau der war durch Zeilen-basiertes 3-way-Merging auf dieser stark umsortierten, maschinengenerierten XML-Struktur nicht robust (siehe Merge-Treiber-Abschnitt oben). Mit dem Merge-Treiber übernimmt der Git-Merge selbst zuverlässig den kompletten, korrekten Stand der eingehenden Seite; Step 0 muss diesen Stand daher nicht mehr aus dem Quellcode rekonstruieren und macht seitdem nur noch die Normalisierung/Sortierung.