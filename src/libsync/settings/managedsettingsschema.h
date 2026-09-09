/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDSETTINGSSCHEMA_H
#define MANAGEDSETTINGSSCHEMA_H

#include <QList>
#include <optional>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC::ManagedSettingsSchema {

[[nodiscard]] OWNCLOUDSYNC_EXPORT const QList<SettingSpec> &all();
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::optional<SettingSpec> find(const QString &key);

}

#endif // MANAGEDSETTINGSSCHEMA_H
