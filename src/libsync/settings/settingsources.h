/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSOURCES_H
#define SETTINGSOURCES_H

#include <QString>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT UserConfigSource : public SettingSource
{
public:
    explicit UserConfigSource(QString configFilePath);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceKind kind() const override;
    [[nodiscard]] LockState lockState() const override;
    [[nodiscard]] int priority() const override;

private:
    QString _configFilePath;
};

} // namespace OCC

#endif // SETTINGSOURCES_H
