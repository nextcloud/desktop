/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "ocsynclib.h"
#include "result.h"

#include <QObject>

namespace OCC {

class OCSYNC_EXPORT PluginFactory
{
public:
    virtual ~PluginFactory();
    virtual QObject* create(QObject* parent) = 0;

    [[nodiscard]] virtual bool checkAvailability() const;

    /**
     * @param path The path for which the plugin should be prepared
     * @param accountUuid The UUID of the account for which the plugin should be prepared (might be null during account setup)
     * @return Nothing or an error string
     */
    [[nodiscard]] virtual Result<void, QString> prepare(const QString &path, const QUuid &accountUuid) const = 0;
};

template<class PluginClass>
class DefaultPluginFactory : public PluginFactory
{
public:
    QObject* create(QObject *parent) override
    {
        return new PluginClass(parent);
    }
};

/// Return the expected name of a plugin, for use with QPluginLoader
QString pluginFileName(const QString &type, const QString &name);

}

Q_DECLARE_INTERFACE(OCC::PluginFactory, "org.owncloud.PluginFactory")
