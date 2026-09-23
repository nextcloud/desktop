/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "catalogue.h"
#include "config.h"
#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#endif

namespace OCC::DocAssets
{
QMap<QString, QString> statusIconSources()
{
    QMap<QString, QString> sources;
    const QStringList states{QStringLiteral("ok"),
                             QStringLiteral("sync"),
                             QStringLiteral("pause"),
                             QStringLiteral("warning"),
                             QStringLiteral("error"),
                             QStringLiteral("offline")};
    for (const auto &state : states) {
        sources.insert(QStringLiteral("icon-status-") + state, QStringLiteral(":/client/theme/colored/") + state + QStringLiteral(".svg"));
        sources.insert(QStringLiteral("icon-tray-") + state, QStringLiteral(":/client/theme/black/state-") + state + QStringLiteral(".svg"));
        sources.insert(QStringLiteral("icon-tray-colored-") + state, QStringLiteral(":/client/theme/colored/state-") + state + QStringLiteral(".svg"));
    }
    return sources;
}

QStringList captureScenarios()
{
    QStringList scenarios{
        QStringLiteral("wizard-server"),
        QStringLiteral("wizard-browser-auth"),
        QStringLiteral("wizard-sync-classic"),
        QStringLiteral("wizard-advanced-options"),
        QStringLiteral("wizard-proxy-settings"),
        QStringLiteral("wizard-client-certificate"),
        QStringLiteral("wizard-secure-connection"),
        QStringLiteral("search-results"),
        QStringLiteral("sharing-overview"),
        QStringLiteral("sharing-details"),
        QStringLiteral("sharing-advanced-settings"),
        QStringLiteral("user-status"),
        QStringLiteral("activities"),
        QStringLiteral("settings-account-classic"),
        QStringLiteral("settings-general"),
        QStringLiteral("settings-advanced"),
        QStringLiteral("settings-ignored-files"),
        QStringLiteral("settings-info"),
        QStringLiteral("assistant-chat"),
    };
#ifdef BUILD_FILE_PROVIDER_MODULE
    if (Mac::FileProvider::available()) {
        scenarios.insert(3, QStringLiteral("wizard-sync-file-provider"));
    }
#endif
    return scenarios;
}

QStringList captureOmissions()
{
    auto omissions = QStringList{
        QStringLiteral("File Provider Account Settings and activity file actions are not catalogued."),
        QStringLiteral("File Provider enable/disable confirmations are not yet catalogued."),
        QStringLiteral("Selective sync, per-folder ignored files, and folder setup are not catalogued."),
        QStringLiteral("Native tray window capture remains omitted."),
        QStringLiteral("Browser-hosted login pages, OS file pickers and OS-owned File Provider indicators are excluded."),
    };
    if (!captureScenarios().contains(QStringLiteral("wizard-sync-file-provider"))) {
        omissions.append(QStringLiteral("File Provider wizard: unavailable in this build or macOS version."));
    }
    return omissions;
}

QString captureFilename(const QString &scenario)
{
    return captureScenarios().contains(scenario) ? scenario + QStringLiteral(".png") : QString{};
}
}
