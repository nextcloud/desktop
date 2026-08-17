# Decision Log

Chronologisches Protokoll nennenswerter Implementierungs-Entscheidungen und bewusst verworfener Alternativen. Format und Ablauf siehe [SKILL.md](SKILL.md).

## 2026-08-17 — Kontrast-Fix für Account-Icon im Settings-Dialog (SES-576)

**Kontext:** Account-Button in `settingsdialog.cpp`s Toolbar hatte zu wenig Kontrast — sowohl das SVG-Fallback-Glyph (Marken-Navy) als auch das nachgeladene Server-Avatar/Initialen-Bild.

**Entscheidung:** Avatar-spezifischer Fix in `settingsdialog.cpp`: Kreis/Rand in `palette(WindowText)` um Glyph bzw. Server-Avatar, nur im normalen Zustand (im `QIcon::On`/ausgewählten Zustand sorgt `palette(highlight)` schon für Kontrast). Committed als `ef168b21d`.

**Verworfene Alternative:** Invertierungs-Logik aus stable-33.0s `Theme::createColorAwareIcon` (`src/libsync/theme.cpp`) wiederherstellen — generische, für alle Icons geltende Lösung. Verworfen: unsere Marken-SVGs sind Navy statt Schwarz/Weiß, RGB-Invertierung ergibt nur einen unbefriedigenden Khakiton statt Kontrast. Von Boris nach Test verworfen.

**Offener Punkt:** `Theme::createColorAwareIcon` ignoriert `palette` fork-weit komplett (nicht nur fürs Avatar-Icon) — nicht weiter untersucht.

**Status:** aktiv.
