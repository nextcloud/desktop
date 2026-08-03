# Lokalisierung des HiDrive Next Clients auf Basis einer Diff-Datei

Diese Anleitung beschreibt den Prozess zur Lokalisierung des HiDrive Next Clients mithilfe einer von uns erstellten Diff-Datei. Die Lokalisierung erfolgt auf Basis von `.ts`-Dateien, welche die Ressourcen der Anwendung in XML-Form enthalten.

## Voraussetzungen

- Nextcloud Stable Client Quellcode
- Python-Script `merge_translation.py`
- Qt Linguist Tools (insbesondere `lupdate`)

# Vorgehen bei Release

Um in einem Release zu erstellen und einen valider PR zur Übersetzung zu haben ist folgendes Vorgehen notwendig:

0. (Optional) Einbeziehen unserer Änderungen aus Phrase. Dieser Schritt ist optional, da die Änderungen in der Regel schon in der Diff-Datei enthalten sind. Sollte es dennoch notwendig sein, können die Änderungen aus Phrase in die `.ts`-Diff-Dateien gemerged werden.
1. Es wird ein neuer `translations_<version source>` branch erstellt. Abgeleitet vom entsprechenden `develop_<version source>`. (z.b. translations_stable-3.16)
2. Durchlaufen der unter stehen den Schritte 1-6
3. Erstellen eines "approved" PRs von `translations_<version source>` nach `<version source>`, also in Richtung der eigentlichen Basisversion von nc (z.B. [stable-3.16] Translations)
4. Der PR wird dann vom Brander gemerged

Das Rebasedn der Translation-Branches lohnt sich eigentlich nicht, weil der nextcloud master sich relativ häufig ändert, was zu vielen Konflikten führen würde.

## Automatisierung: post-merge Hook

Beim Mergen eines `stable-x.y`-Branches (neue NC-Basisversion) in einen Feature-/Entwicklungs-Branch müssen die STRATO/IONOS-Übersetzungen erneut gegen die aktualisierte NC-Basis gemerged werden. Damit das nicht vergessen wird, gibt es den Hook `.githooks/post-merge`:

- Läuft automatisch nach jedem lokalen `git merge`/`git pull`.
- Erkennt anhand der Merge-Commit-Message, ob ein `stable-x.y`-Branch gemerged wurde (z.B. `Merge branch 'stable-33.0' into ...`).
- Führt in diesem Fall automatisch `merge_translation.py all <branch>` aus (**ohne** `--auto-commit`).
- Änderungen an `translations/client_*.ts` liegen danach ungestaged im Working Directory und müssen manuell geprüft und committet werden.

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
python3 merge_translation.py all stable-4.0 --auto-commit
```

### Einzelne Schritte

Die Schritte können auch einzeln ausgeführt werden. Mit `--auto-commit` entfällt das manuelle Committen:

```
python3 merge_translation.py 1 --auto-commit
```

### 1. Nextcloud-Grundstand aktualisieren

- Das Skript verwendet `git worktree`, um den unveränderten Nextcloud-Quellcode temporär verfügbar zu machen, ohne den aktuellen Branch zu wechseln.
- Der NC-Basisbranch wird als zweites Argument übergeben:

```
python3 merge_translation.py 0 stable-4.0
```

- Das Skript erstellt automatisch einen temporären Worktree, führt `lupdate` gegen den NC-Quellcode aus, sortiert die Dateien und räumt den Worktree wieder auf.
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