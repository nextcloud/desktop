/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "capabilities.h"

#include <QVariantMap>
#include <QLoggingCategory>

#include <QDebug>

namespace OCC {

Q_LOGGING_CATEGORY(lcServerCapabilities, "nextcloud.sync.server.capabilities", QtInfoMsg)


Capabilities::Capabilities(const QVariantMap &capabilities)
    : _capabilities(capabilities)
{
}

bool Capabilities::shareAPI() const
{
    if (_capabilities["files_sharing"].toMap().contains("api_enabled")) {
        return _capabilities["files_sharing"].toMap()["api_enabled"].toBool();
    } else {
        // This was later added so if it is not present just assume the API is enabled.
        return true;
    }
}

bool Capabilities::sharePublicLink() const
{
    if (_capabilities["files_sharing"].toMap().contains("public")) {
        return shareAPI() && _capabilities["files_sharing"].toMap()["public"].toMap()["enabled"].toBool();
    } else {
        // This was later added so if it is not present just assume that link sharing is enabled.
        return true;
    }
}

bool Capabilities::sharePublicLinkAllowUpload() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["upload"].toBool();
}

bool Capabilities::sharePublicLinkSupportsUploadOnly() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["supports_upload_only"].toBool();
}

bool Capabilities::sharePublicLinkEnforcePassword() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["password"].toMap()["enforced"].toBool();
}

bool Capabilities::sharePublicLinkEnforceExpireDate() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["enforced"].toBool();
}

int Capabilities::sharePublicLinkExpireDateDays() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["days"].toInt();
}

bool Capabilities::sharePublicLinkMultiple() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["multiple"].toBool();
}

bool Capabilities::shareResharing() const
{
    return _capabilities["files_sharing"].toMap()["resharing"].toBool();
}

bool Capabilities::clientSideEncryptionAvaliable() const
{
    auto it = _capabilities.constFind(QStringLiteral("end-to-end-encryption"));
    if (it != _capabilities.constEnd())
        return (*it).toMap().value(QStringLiteral("enabled"), false).toBool();
    return false;
}

bool Capabilities::notificationsAvailable() const
{
    // We require the OCS style API in 9.x, can't deal with the REST one only found in 8.2
    return _capabilities.contains("notifications") && _capabilities["notifications"].toMap().contains("ocs-endpoints");
}

bool Capabilities::isValid() const
{
    return !_capabilities.isEmpty();
}

QList<QByteArray> Capabilities::supportedChecksumTypes() const
{
    QList<QByteArray> list;
    foreach (const auto &t, _capabilities["checksums"].toMap()["supportedTypes"].toList()) {
        list.push_back(t.toByteArray());
    }
    return list;
}

QByteArray Capabilities::preferredUploadChecksumType() const
{
    return _capabilities["checksums"].toMap()["preferredUploadType"].toByteArray();
}

QByteArray Capabilities::uploadChecksumType() const
{
    QByteArray preferred = preferredUploadChecksumType();
    if (!preferred.isEmpty())
        return preferred;
    QList<QByteArray> supported = supportedChecksumTypes();
    if (!supported.isEmpty())
        return supported.first();
    return QByteArray();
}

bool Capabilities::chunkingNg() const
{
    static const auto chunkng = qgetenv("OWNCLOUD_CHUNKING_NG");
    if (chunkng == "0")
        return false;
    if (chunkng == "1")
        return true;
    return _capabilities["dav"].toMap()["chunking"].toByteArray() >= "1.0";
}

bool Capabilities::chunkingParallelUploadDisabled() const
{
    return _capabilities["dav"].toMap()["chunkingParallelUploadDisabled"].toBool();
}

bool Capabilities::privateLinkPropertyAvailable() const
{
    return _capabilities["files"].toMap()["privateLinks"].toBool();
}

QList<int> Capabilities::httpErrorCodesThatResetFailingChunkedUploads() const
{
    QList<int> list;
    foreach (const auto &t, _capabilities["dav"].toMap()["httpErrorCodesThatResetFailingChunkedUploads"].toList()) {
        list.push_back(t.toInt());
    }
    return list;
}

QString Capabilities::invalidFilenameRegex() const
{
    return _capabilities["dav"].toMap()["invalidFilenameRegex"].toString();
}

bool Capabilities::uploadConflictFiles() const
{
    static auto envIsSet = !qEnvironmentVariableIsEmpty("OWNCLOUD_UPLOAD_CONFLICT_FILES");
    static int envValue = qEnvironmentVariableIntValue("OWNCLOUD_UPLOAD_CONFLICT_FILES");
    if (envIsSet)
        return envValue != 0;

    return _capabilities["uploadConflictFiles"].toBool();
}
}
