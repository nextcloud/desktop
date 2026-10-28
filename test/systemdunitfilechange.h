/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDBusArgument>
#include <QString>

struct SystemdUnitFileChange {
    QString type;
    QString path;
    QString source;

    friend QDBusArgument &operator<<(QDBusArgument &argument, const SystemdUnitFileChange &change)
    {
        argument.beginStructure();
        argument << change.type << change.path << change.source;
        argument.endStructure();
        return argument;
    }

    friend const QDBusArgument &operator>>(const QDBusArgument &argument, SystemdUnitFileChange &change)
    {
        argument.beginStructure();
        argument >> change.type >> change.path >> change.source;
        argument.endStructure();
        return argument;
    }
};

Q_DECLARE_METATYPE(SystemdUnitFileChange)
