# Registry: Upstream-Datei ↔ Fork-Ersatz

Jede Zeile ist ein Paar: eine Upstream-Komponente, die im Fork durch eine eigene Entwicklung ersetzt wurde und selbst nicht mehr aus dem aktiven Render-Baum erreichbar ist, plus die Fork-Datei(en), die sie funktional ersetzen. "Zuletzt geprüft" ist der Commit-Hash **auf dem Referenz-Branch** (siehe `../stable-merge-check/reference-branch.txt`), bis zu dem die Historie der Upstream-Datei bereits durchgesehen wurde — beim nächsten Lauf wird nur `<hash>..origin/<branch>` betrachtet.

## src/gui/tray

| Upstream-Datei | Fork-Ersatz | Bekannte bewusste Abweichungen | Zuletzt geprüft (Upstream-Commit) |
|---|---|---|---|
| `src/gui/tray/TrayWindowHeader.qml` | `src/gui/SesComponents/SesTrayHeader.qml` | "More apps"-Menü (mehrere externe Apps aus `UserAppsModel`, je eigenem Icon) entfernt, ersetzt durch einzelnen statischen "Website"-Button — Ticket `SES-4` | `568cbe171` (2026-04-19, "chore(i18n): Change group folder to team folder") |
| `src/gui/tray/CurrentAccountHeaderButton.qml` | `src/gui/tray/TrayWindowAccountMenu.qml` (+ `src/gui/tray/UserLine.qml`, geteilt mit anderen Aufrufern, selbst aber schon lange vom Upstream-Pendant abgedriftet — kein reines Duplikat) | Gesamtes User-Status-Feature (Presence-Picker + Status-Message) bewusst nicht vorhanden — Produkt hat keine Collaboration-Features, Status ergibt daher keinen Sinn (bestätigt von Boris am 2026-08-17). Betrifft: Status-Indikator auf dem Avatar (Ticket `SES-50`, Code-Kommentar vorhanden) UND die beiden Menüeinträge "Set status"/"Status message" in `UserLine.qml` (entfernt im Zuge von `SES-457`, kein expliziter Ticket-Kommentar im Code, aber dieselbe Produktentscheidung) — beide beim Prüfen als eine zusammengehörige Abweichung behandeln, nicht als zwei getrennte. Dynamische Menübreite (breitester Eintrag) durch feste `Style.sesAccountMenuWidth` ersetzt; `parentBackgroundColor`-Weiterreichung für kontrastabhängige Hintergründe entfällt (Fork nutzt statisches `Style.ses*`-Farbsystem statt QPalette) | `60e0cb196` (2026-02-11, "color fix" — kompletter Rückstand bis `de066e6b9` am 2026-08-17 nachgeholt geprüft) |

## Neues Paar eintragen

Beim Auffinden eines weiteren "durch eigene Entwicklung ersetzten" Paars: Zeile mit Upstream-Datei, Fork-Ersatz und dem aktuellen HEAD-Commit der Upstream-Datei auf dem Referenz-Branch (`git log -1 --format=%h origin/<branch> -- <upstream-datei>`) als Startwert für "Zuletzt geprüft" ergänzen.
