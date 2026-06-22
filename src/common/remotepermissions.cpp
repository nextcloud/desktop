/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "remotepermissions.h"

#include <QVariant>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>

#include <cstring>
#include <type_traits>

namespace OCC {

Q_LOGGING_CATEGORY(lcRemotePermissions, "nextcloud.sync.remotepermissions", QtInfoMsg)

static const char letters[] = " GWDNVCKRSMm";


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
    constexpr auto shareAttributesDecoder = [] (const auto &shareAttributes, RemotePermissions &perm) -> void {
        auto missingDownloadPermission = false;
        const auto &jsonArray = shareAttributes.array();
        for (const auto &oneEntry : jsonArray) {
            const auto &jsonObject = oneEntry.toObject();

            if (jsonObject.contains(u"scope") && jsonObject.value(u"scope").toString() == u"permissions") {
                if (jsonObject.contains(u"key") && jsonObject.value(u"key").toString() == u"download") {
                    if (jsonObject.contains(u"value") && !jsonObject.value(u"value").toBool()) {
                        missingDownloadPermission = true;
                    }
                    break;
                }
            }
        }

        if (missingDownloadPermission) {
            perm.unsetPermission(RemotePermissions::CanRead);
        }
    };

    RemotePermissions perm;
    perm.fromArray(value.utf16());
    Q_ASSERT(perm.hasPermission(RemotePermissions::CanRead));

    if (otherProperties.contains(QStringLiteral("share-attributes"))) {
        const auto shareAttributesRawValue = otherProperties.value(QStringLiteral("share-attributes"));

        if constexpr (std::is_same<T, QMap<QString, QString>>::value) {
            const auto &shareAttributes = QJsonDocument::fromJson(otherProperties.value(QStringLiteral("share-attributes")).toUtf8());
            shareAttributesDecoder(shareAttributes, perm);
        } else if constexpr (std::is_same<T, QVariantMap>::value) {
            const auto &shareAttributes = QJsonDocument::fromJson(otherProperties.value(QStringLiteral("share-attributes")).toString().toUtf8());
            shareAttributesDecoder(shareAttributes, perm);
        }
    }

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
