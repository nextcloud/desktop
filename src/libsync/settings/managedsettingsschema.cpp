/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedsettingsschema.h"

#include <QNetworkProxy>

#include <limits>

namespace OCC::ManagedSettingsSchema {

namespace
{

constexpr auto minimumProxyPort = 1;

bool isValidProxyType(const QVariant &value)
{
    auto isNumber = false;
    const auto proxyType = value.toInt(&isNumber);
    return isNumber && proxyType >= QNetworkProxy::DefaultProxy && proxyType <= QNetworkProxy::HttpProxy;
}

bool isValidProxyPort(const QVariant &value)
{
    auto isNumber = false;
    const auto proxyPort = value.toInt(&isNumber);
    return isNumber && proxyPort >= minimumProxyPort && proxyPort <= std::numeric_limits<quint16>::max();
}

}

const QList<SettingDefinition> &all()
{
    static const QList<SettingDefinition> specs = {
        {QStringLiteral("skipUpdateCheck"), false, true},
        {QStringLiteral("autoUpdateCheck"), true, true},
        {QStringLiteral("confirmExternalStorage"), true, true},
        {QStringLiteral("useNewBigFolderSizeLimit"), true, true},
        {QStringLiteral("notifyExistingFoldersOverLimit"), false, true},
        {QStringLiteral("virtualFilesMode"), QStringLiteral("off"), true},
        {QStringLiteral("newBigFolderSizeLimit"), 0, true},
        {QStringLiteral("stopSyncingExistingFoldersOverLimit"), false, true},
        {QStringLiteral("proxyType"), 0, true, isValidProxyType},
        {QStringLiteral("proxyHost"), QString(), true},
        {QStringLiteral("proxyPort"), 0, true, isValidProxyPort},
    };
    return specs;
}

std::optional<SettingDefinition> find(const QString &key)
{
    for (const auto &definition : all()) {
        if (definition.key == key) {
            return definition;
        }
    }
    return std::nullopt;
}

}
