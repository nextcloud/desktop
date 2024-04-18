/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "remotepermissions.h"

#include <QLoggingCategory>

#include <cstring>

namespace OCC {

Q_LOGGING_CATEGORY(lcRemotePermissions, "nextcloud.sync.remotepermissions", QtInfoMsg)

static const char letters[] = " WDNVCKRSMm";


template <typename Char>
void RemotePermissions::fromArray(const Char *p)
{
    _value = notNullMask;
    if (!p)
        return;
    while (*p) {
        if (auto res = std::strchr(letters, static_cast<char>(*p)))
            _value |= (1 << (res - letters));
        ++p;
    }
}

QByteArray RemotePermissions::toDbValue() const
{
    QByteArray result;
    if (isNull())
        return result;
    result.reserve(PermissionsCount);
    for (uint i = 1; i <= PermissionsCount; ++i) {
        if (_value & (1 << i))
            result.append(letters[i]);
    }
    if (result.isEmpty()) {
        // Make sure it is not empty so we can differentiate null and empty permissions
        result.append(' ');
    }
    return result;
}

QString RemotePermissions::toString() const
{
    return QString::fromUtf8(toDbValue());
}

RemotePermissions RemotePermissions::fromDbValue(const QByteArray &value)
{
    if (value.isEmpty())
        return {};
    RemotePermissions perm;
    perm.fromArray(value.constData());
    return perm;
}

template <typename T>
RemotePermissions RemotePermissions::internalFromServerString(const QString &value,
                                                              const T&otherProperties,
                                                              MountedPermissionAlgorithm algorithm)
{
    RemotePermissions perm;
    perm.fromArray(value.utf16());

    if (algorithm == MountedPermissionAlgorithm::WildGuessMountedSubProperty) {
        return perm;
    }

    if ((otherProperties.contains(QStringLiteral("is-mount-root")) && otherProperties.value(QStringLiteral("is-mount-root")) == QStringLiteral("false") && perm.hasPermission(RemotePermissions::IsMounted)) ||
        (!otherProperties.contains(QStringLiteral("is-mount-root")) && perm.hasPermission(RemotePermissions::IsMounted))) {
        /* All the entries in a external storage have 'M' in their permission. However, for all
           purposes in the desktop client, we only need to know about the mount points.
           So replace the 'M' by a 'm' for every sub entries in an external storage */
        perm.unsetPermission(RemotePermissions::IsMounted);
        perm.setPermission(RemotePermissions::IsMountedSub);
        qCInfo(lcRemotePermissions()) << otherProperties.value(QStringLiteral("permissions")) << "replacing M permissions by m for subfolders inside a group folder";
    }

    return perm;
}

RemotePermissions RemotePermissions::fromServerString(const QString &value,
                                                      MountedPermissionAlgorithm algorithm,
                                                      const QMap<QString, QString> &otherProperties)
{
    return internalFromServerString(value, otherProperties, algorithm);
}

RemotePermissions RemotePermissions::fromServerString(const QString &value,
                                                      MountedPermissionAlgorithm algorithm,
                                                      const QVariantMap &otherProperties)
{
    return internalFromServerString(value, otherProperties, algorithm);
}

} // namespace OCC
