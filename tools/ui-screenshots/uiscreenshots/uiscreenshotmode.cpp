/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "uiscreenshotmode.h"

#include <QByteArray>

namespace OCC::UiScreenshots {

Q_LOGGING_CATEGORY(lcUiScreenshots, "nextcloud.gui.uiscreenshots", QtInfoMsg)

ParsedPhase parsePhase(const QByteArrayView value)
{
    if (value.isEmpty()) {
        return {Phase::None, {}};
    }
    if (value == "qml" || value == "1") {
        return {Phase::Qml, {}};
    }
    if (value == "native") {
        return {Phase::Native, {}};
    }

    return {
        Phase::Invalid,
        QStringLiteral("Invalid NEXTCLOUD_UI_SCREENSHOTS value '%1'; expected 'qml', '1', or 'native'.")
            .arg(QString::fromUtf8(value)),
    };
}

ParsedPhase phaseFromEnvironment()
{
    return parsePhase(qgetenv("NEXTCLOUD_UI_SCREENSHOTS"));
}

QString phaseName(const Phase phase)
{
    switch (phase) {
    case Phase::None:
        return QStringLiteral("none");
    case Phase::Qml:
        return QStringLiteral("qml");
    case Phase::Native:
        return QStringLiteral("native");
    case Phase::Invalid:
        return QStringLiteral("invalid");
    }
    Q_UNREACHABLE();
    return {};
}

}
