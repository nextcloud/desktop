/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>
#include <QUrl>
#include <optional>

namespace OCC
{
class Account;
}

namespace OCC::DocAssets
{
struct LiveProfile {
    QString directory;
    QUrl serverUrl;
    QString user;
    QString searchTerm;
    QString sharePath;
};

[[nodiscard]] std::optional<LiveProfile> savedDevProfile(QString *error);
[[nodiscard]] std::optional<LiveProfile> liveProfileFromConfig(const QString &configPath, QString *error);
[[nodiscard]] bool accountMatchesProfile(const Account *account, const LiveProfile &profile, QString *error);
[[nodiscard]] bool scenarioUsesLiveProfile(const QString &scenario);
}
