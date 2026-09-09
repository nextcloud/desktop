/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedsettingsschema.h"

namespace OCC::ManagedSettingsSchema {

const QList<SettingSpec> &all()
{
    static const QList<SettingSpec> specs = {
        {QStringLiteral("skipUpdateCheck"), false, true, SettingScope::User},
        {QStringLiteral("autoUpdateCheck"), true, true, SettingScope::User},
    };
    return specs;
}

std::optional<SettingSpec> find(const QString &key)
{
    for (const auto &spec : all()) {
        if (spec.key == key) {
            return spec;
        }
    }
    return std::nullopt;
}

}
