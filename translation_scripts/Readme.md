# Lokalisierung des HiDrive Next Clients auf Basis einer Diff-Datei

Diese Anleitung beschreibt den Prozess zur Lokalisierung des HiDrive Next Clients mithilfe einer von uns erstellten Diff-Datei. Die Lokalisierung erfolgt auf Basis von `.ts`-Dateien, welche die Ressourcen der Anwendung in XML-Form enthalten.

## Voraussetzungen

- Nextcloud Stable Client Quellcode
- Python-Script `merge_translation.py`
- Qt Linguist Tools (insbesondere `lupdate`)

# Vorgehen bei Release

Um in einem Release zu erstellen und einen valider PR zur übersetuen zu haben ist folgendes Vorgehen notwendig:

0. (Optional) Einbeziehen unserer Änderungen aus Phrase. Dieser Schritt ist optional, da die Änderungen in der Regel schon in der Diff-Datei enthalten sind. Sollte es dennoch notwendig sein, können die Änderungen aus Phrase in die `.ts`-Diff-Dateien gemerged werden.
1. Es wird ein neuer `treanslations_<version source>` branch erstellt. Abgeleitet vom entsprechenden `develop_<version source>`. (z.b. translations_stable-3.16)
2. Durchlaufen der unter stehen den Schritte 1-6
3. Erstellen eines "approved" PRs von `translations_<version source>` nach `<version source>`, also in Richtung der eigentlichen Basisversion von nc (z.B. [stable-3.16] Translations)
4. Der PR wird dann hoffentlich vom Brander gemerged

Das Rebasedn der translations branches lohnt sich eigentlich nicht, weil der nextcloud master sich relativ jäufig Ändert, was zu vielen Konflikten führen würde.

## Allgemeines

Die Lokalisierung erfolgt in mehreren Schritten. Für eine bessere Vergleichbarkeit werden der Dateiname (Location) und die Zeilennummer in den `.ts`-Dateien entfernt. Die `.ts`-Dateien müssen vor jedem Merge-Schritt sortiert werden.

## Schritte

### 1. Nextcloud-Grundstand aktualisieren

- Den **unveränderten Nextcloud Stable Client** verwenden.
- Am Einfachsten ist es den branch auschecken, den befehl laufen lassen, stashen und dann wieder auf den alten branch wechseln. Stash poppen.
- Mit folgendem Befehl die `.ts`Dateien auf den neuesten Stand bringen:

```
C:\Craft64\bin\lupdate.exe -locations none -no-obsolete -no-ui-lines .\src\libsync\ .\src\gui\ .\src\csync\ .\src\common\ .\src\cmd\ -ts .\translations\client_de.ts .\translations\client_en.ts .\translations\client_en_GB.ts .\translations\client_fr.ts .\translations\client_es.ts .\translations\client_nl.ts
```

Danach die .ts Dateien mit folgendem Befehl sortieren:

```
python3 merge_translation.py 0
```

- Wichtig: **Obsolete Keys entfernen**.
- Die `.ts`Datei manuell per Skript sortieren.
- Datei in den **Translation Branch** legen und committen.

### 2. Merge-Schritt 1

- Verwenden des Python-Skripts `merge_translation.py` mit Parameter `1`:

```
python3 merge_translation.py 1
```

- Führt ein `lupdate` auf dem **HiDrive Next Client** aus.
- Obsolete Einträge werden **nicht gelöscht**.
- Neue Keys werden hinzugefügt.
- Obsolete-Markierungen werden entfernt und die Datei wird sortiert.
- Commit durchführen.

### 3. Merge-Schritt 2

- Verwenden des Skripts mit Parameter `2`:

```
python3 merge_translation.py 2
```

- Führt ein `lupdate` aus.
- **Obsolete Keys werden entfernt**.
- Die Datei enthält jetzt nur die aktuellen Keys (unsere Keys ohne Übersetzungen).
- Commit durchführen.

### 4. Merge-Schritt 3

- Verwenden des Skripts mit Parameter `3`:

```
python3 merge_translation.py 3
```

- Sprachabhängige Diff-Dateien werden in die `.ts`Dateien gemergt.
- Leere Keys werden mit Übersetzungen gefüllt.
- Obsolete Keys aus der Diff-Datei werden gegebenenfalls eingefügt.
- Commit durchführen.

### 5. Merge-Schritt 4

- Verwenden des Skripts mit Parameter `4`:

```
python3 merge_translation.py 4
```

- Ein weiteres `lupdate` wird ausgeführt.
- Heuristische Füllung von doppelten Keys mit unseren Übersetzungen.

### 6. Merge-Schritt 5

- Verwenden des Skripts mit Parameter `5`:

```
python3 merge_translation.py 5
```

- **Obsolete Keys werden endgültig entfernt**.
- Commit durchführen.

## Abschluss

Nach dem letzten Schritt sind die `.ts`-Dateien vollständig lokalisiert, enthalten unsere Keys und Übersetzungen und sind frei von obsolete Keys.