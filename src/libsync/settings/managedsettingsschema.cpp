/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedsettingsschema.h"

namespace OCC::ManagedSettingsSchema {

const QList<SettingDefinition> &all()
{
    // Keys with a runtime default are resolved at the call site, not listed here.
    static const QList<SettingDefinition> specs = {
        {QStringLiteral("skipUpdateCheck"), false, true, SettingScope::User},
        {QStringLiteral("autoUpdateCheck"), true, true, SettingScope::User},
        {QStringLiteral("confirmExternalStorage"), true, true, SettingScope::User},
        {QStringLiteral("useNewBigFolderSizeLimit"), true, true, SettingScope::User},
        {QStringLiteral("notifyExistingFoldersOverLimit"), false, true, SettingScope::User},
        {QStringLiteral("virtualFilesMode"), QStringLiteral("off"), true, SettingScope::User},
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
