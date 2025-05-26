/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <QString>
#include <QMetaType>
#include "ocsynclib.h"
#include <QDebug>

namespace OCC {

/**
 * Class that store in a memory efficient way the remote permission
 */
class OCSYNC_EXPORT RemotePermissions
{
private:
    // The first bit tells if the value is set or not
    // The remaining bits correspond to know if the value is set
    quint16 _value = 0;
    static constexpr int notNullMask = 0x1;

    template <typename Char> // can be 'char' or 'ushort' if conversion from QString
    void fromArray(const Char *p);

public:
    enum Permissions {
        CanRead = 1,              // G
        CanWrite,                 // W
        CanDelete,                // D
        CanRename,                // N
        CanMove,                  // V
        CanAddFile,               // C
        CanAddSubDirectories,     // K
        CanReshare,               // R
        // Note: on the server, this means SharedWithMe, but in discoveryphase.cpp we also set
        // this permission when the server reports the any "share-types"
        IsShared,                 // S
        IsMounted,                // M
        IsMountedSub,             // m (internal: set if the parent dir has IsMounted)

        // Note: when adding support for more permissions, we need to invalid the cache in the database.
        // (by setting forceRemoteDiscovery in SyncJournalDb::checkConnect)
        PermissionsCount = IsMountedSub
    };

    enum class MountedPermissionAlgorithm {
        UseMountRootProperty,
        WildGuessMountedSubProperty,
    };

    /// null permissions
    RemotePermissions() = default;

    /// array with one character per permission, "" is null, " " is non-null but empty
    [[nodiscard]] QByteArray toDbValue() const;

    /// output for display purposes, no defined format (same as toDbValue in practice)
    [[nodiscard]] QString toString() const;

    /// read value that was written with toDbValue()
    static RemotePermissions fromDbValue(const QByteArray &);

    /// read a permissions string received from the server, never null
    static RemotePermissions fromServerString(const QString &value,
                                              MountedPermissionAlgorithm algorithm = MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                              const QMap<QString, QString> &otherProperties = {});

    /// read a permissions string received from the server, never null
    static RemotePermissions fromServerString(const QString &value,
                                              MountedPermissionAlgorithm algorithm,
                                              const QVariantMap &otherProperties = {});

    [[nodiscard]] bool hasPermission(Permissions p) const
    {
        return _value & (1 << static_cast<int>(p));
    }
    void setPermission(Permissions p)
    {
        _value |= (1 << static_cast<int>(p)) | notNullMask;
    }
    void unsetPermission(Permissions p)
    {
        _value &= ~(1 << static_cast<int>(p));
    }

    [[nodiscard]] bool isNull() const { return !(_value & notNullMask); }
    friend bool operator==(RemotePermissions a, RemotePermissions b)
    {
        return a._value == b._value;
    }
    friend bool operator!=(RemotePermissions a, RemotePermissions b)
    {
        return !(a == b);
    }

    friend QDebug operator<<(QDebug &dbg, RemotePermissions p)
    {
        return dbg << p.toString();
    }

private:

    template <typename T>
    static RemotePermissions internalFromServerString(const QString &value,
                                                      const T&otherProperties,
                                                      MountedPermissionAlgorithm algorithm);
};


} // namespace OCC

Q_DECLARE_METATYPE(OCC::RemotePermissions)
