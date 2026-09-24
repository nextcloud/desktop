/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDSETTINGSTESTUTILS_H
#define MANAGEDSETTINGSTESTUTILS_H

#include <QVariantMap>

#include <optional>

#include "settings/managedproxysettings.h"
#include "settings/managedsettings.h"

class MapSource : public OCC::SettingSource
{
public:
    MapSource(OCC::SettingSourceType kind, OCC::EnforcementState enforcement, int priority, QVariantMap values)
        : _kind(kind)
        , _enforcement(enforcement)
        , _priority(priority)
        , _values(std::move(values))
    {
    }

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &) const override
    {
        if (!_values.contains(key)) {
            return std::nullopt;
        }
        return _values.value(key);
    }
    [[nodiscard]] OCC::SettingSourceType type() const override
    {
        return _kind;
    }
    [[nodiscard]] OCC::EnforcementState enforcement() const override
    {
        return _enforcement;
    }
    [[nodiscard]] int priority() const override
    {
        return _priority;
    }

private:
    OCC::SettingSourceType _kind;
    OCC::EnforcementState _enforcement;
    int _priority;
    QVariantMap _values;
};

inline OCC::ManagedProxySettings
managedProxyFields(bool enforced, std::optional<int> proxyType, std::optional<QString> proxyHostName, std::optional<int> proxyPort)
{
    OCC::ManagedProxySettings managedProxy;
    if (proxyType) {
        managedProxy.typeManaged = true;
        managedProxy.typeEnforced = enforced;
        managedProxy.proxyType = *proxyType;
    }
    if (proxyHostName) {
        managedProxy.hostManaged = true;
        managedProxy.hostEnforced = enforced;
        managedProxy.proxyHostName = *proxyHostName;
    }
    if (proxyPort) {
        managedProxy.portManaged = true;
        managedProxy.portEnforced = enforced;
        managedProxy.proxyPort = *proxyPort;
    }
    managedProxy.isManaged = managedProxy.typeManaged || managedProxy.hostManaged || managedProxy.portManaged;
    managedProxy.isEnforced = managedProxy.typeEnforced || managedProxy.hostEnforced || managedProxy.portEnforced;
    return managedProxy;
}

#endif // MANAGEDSETTINGSTESTUTILS_H
