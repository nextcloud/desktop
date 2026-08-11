/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UISCREENSHOTMODE_H
#define UISCREENSHOTMODE_H

#include <QByteArrayView>
#include <QLoggingCategory>
#include <QString>

namespace OCC::UiScreenshots {

Q_DECLARE_LOGGING_CATEGORY(lcUiScreenshots)

/** @brief Identifies the requested standalone screenshot phase. */
enum class Phase {
    None,
    Qml,
    Native,
    Invalid,
};

/** @brief Contains a parsed screenshot phase and a diagnostic for invalid input. */
struct ParsedPhase
{
    Phase phase = Phase::None;
    QString error;
};

/**
 * @brief Parses the value of `NEXTCLOUD_UI_SCREENSHOTS`.
 * @param value Raw environment value. An empty value selects no phase.
 */
[[nodiscard]] ParsedPhase parsePhase(QByteArrayView value);

/** @brief Parses `NEXTCLOUD_UI_SCREENSHOTS` from the current process environment. */
[[nodiscard]] ParsedPhase phaseFromEnvironment();

/** @brief Returns a stable diagnostic name for @p phase. */
[[nodiscard]] QString phaseName(Phase phase);

}

#endif // UISCREENSHOTMODE_H
