/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "liveprofile.h"

#include "account.h"
#include "creds/abstractcredentials.h"
#include "theme.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace OCC::DocAssets
{
namespace
{
QString normalizedServer(const QUrl &url)
{
    auto normalized = url.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash);
    normalized.setUserName({});
    normalized.setPassword({});
    return normalized.toString(QUrl::FullyEncoded);
}
}

std::optional<LiveProfile> liveProfileFromConfig(const QString &configPath, QString *error)
{
    error->clear();
    if (!QFileInfo(configPath).isFile()) {
        *error = QStringLiteral("No NextcloudDev account configuration found. Sign in to the test server in NextcloudDev first.");
        return std::nullopt;
    }
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Accounts"));
    const auto accounts = settings.childGroups();
    if (accounts.size() != 1) {
        *error = QStringLiteral("NextcloudDev must have exactly one configured test account for capture.");
        return std::nullopt;
    }
    settings.beginGroup(accounts.constFirst());
    auto profile = LiveProfile{
        QFileInfo(configPath).absolutePath(),
        settings.value(QStringLiteral("url")).toUrl(),
        settings.value(QStringLiteral("dav_user"), settings.value(QStringLiteral("user"))).toString(),
        QStringLiteral("Project"),
        {},
    };
    if (!profile.serverUrl.isValid() || profile.serverUrl.host().isEmpty()
        || (profile.serverUrl.scheme() != QStringLiteral("https") && profile.serverUrl.scheme() != QStringLiteral("http")) || profile.user.isEmpty()) {
        *error = QStringLiteral("The NextcloudDev account configuration has no valid server URL or user ID.");
        return std::nullopt;
    }
    return profile;
}

std::optional<LiveProfile> savedDevProfile(QString *error)
{
    if (Theme::instance()->configFileName() != QStringLiteral("nextclouddev.cfg")) {
        *error = QStringLiteral("Build the capture target with the NextcloudDev branding to reuse its configuration and Keychain login.");
        return std::nullopt;
    }
    const auto sandboxConfig =
        QDir::home().filePath(QStringLiteral("Library/Containers/com.nextcloud.desktopclient/Data/Library/Preferences/NextcloudDev/nextclouddev.cfg"));
    const auto nativeConfig = QDir::home().filePath(QStringLiteral("Library/Preferences/NextcloudDev/nextclouddev.cfg"));
    // Never silently choose between two saved profiles.
    if (QFileInfo::exists(sandboxConfig) && QFileInfo::exists(nativeConfig)) {
        *error = QStringLiteral("Two NextcloudDev configurations exist. Resolve the duplicate profiles before capturing.");
        return std::nullopt;
    }
    return liveProfileFromConfig(QFileInfo::exists(sandboxConfig) ? sandboxConfig : nativeConfig, error);
}

bool accountMatchesProfile(const Account *account, const LiveProfile &profile, QString *error)
{
    error->clear();
    if (!account) {
        *error = QStringLiteral("The capture profile did not restore an account");
        return false;
    }
    if (normalizedServer(account->url()) != normalizedServer(profile.serverUrl)) {
        *error =
            QStringLiteral("Capture account server mismatch: expected %1, got %2").arg(normalizedServer(profile.serverUrl), normalizedServer(account->url()));
        return false;
    }
    const auto credentialsUser = account->credentials() ? account->credentials()->user() : QString{};
    if (account->davUser() != profile.user && credentialsUser != profile.user) {
        *error = QStringLiteral("Capture account user mismatch: expected %1, got %2")
                     .arg(profile.user, credentialsUser.isEmpty() ? account->davUser() : credentialsUser);
        return false;
    }
    return true;
}

bool scenarioUsesLiveProfile(const QString &scenario)
{
    return !scenario.startsWith(QStringLiteral("wizard-"));
}
}
