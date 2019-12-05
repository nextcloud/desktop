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
#include <QDebug>

namespace OCC {


Capabilities::Capabilities(const QVariantMap &capabilities)
    : _capabilities(capabilities)
    , _fileSharingCapabilities(_capabilities.value(QStringLiteral("files_sharing")).toMap())
    , _fileSharingPublicCapabilities(_fileSharingCapabilities.value(QStringLiteral("public"), {}).toMap())
{
}

bool Capabilities::shareAPI() const
{
    // This was later added so if it is not present just assume the API is enabled.
    return _fileSharingCapabilities.value(QStringLiteral("api_enabled"), true).toBool();
}

bool Capabilities::sharePublicLink() const
{
    // This was later added so if it is not present just assume that link sharing is enabled.
    return shareAPI() && _fileSharingPublicCapabilities.value(QStringLiteral("enabled"), true).toBool();
}

bool Capabilities::sharePublicLinkAllowUpload() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("upload")).toBool();
}

bool Capabilities::sharePublicLinkSupportsUploadOnly() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("supports_upload_only")).toBool();
}

bool Capabilities::sharePublicLinkEnforcePassword() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("password")).toMap().value(QStringLiteral("enforced")).toBool();
}

bool Capabilities::sharePublicLinkDefaultExpire() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("expire_date")).toMap().value(QStringLiteral("enabled")).toBool();
}

int Capabilities::sharePublicLinkDefaultExpireDateDays() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("expire_date")).toMap().value(QStringLiteral("days")).toInt();
}

bool Capabilities::sharePublicLinkEnforceExpireDate() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("expire_date")).toMap().value(QStringLiteral("enforced")).toBool();
}

bool Capabilities::sharePublicLinkMultiple() const
{
    return _fileSharingPublicCapabilities.value(QStringLiteral("multiple")).toBool();
}

bool Capabilities::shareResharing() const
{
    return _fileSharingCapabilities.value(QStringLiteral("resharing")).toBool();
}

int Capabilities::defaultPermissions() const
{
    return _fileSharingCapabilities.value(QStringLiteral("default_permissions"), 1).toInt();
}

bool Capabilities::notificationsAvailable() const
{
    // We require the OCS style API in 9.x, can't deal with the REST one only found in 8.2
    return _capabilities.contains("notifications") && _capabilities.value("notifications").toMap().contains("ocs-endpoints");
}

bool Capabilities::isValid() const
{
    return !_capabilities.isEmpty();
}

QList<QByteArray> Capabilities::supportedChecksumTypes() const
{
    QList<QByteArray> list;
    foreach (const auto &t, _capabilities.value("checksums").toMap().value("supportedTypes").toList()) {
        list.push_back(t.toByteArray());
    }
    return list;
}

QByteArray Capabilities::preferredUploadChecksumType() const
{
    return _capabilities.value("checksums").toMap().value("preferredUploadType").toByteArray();
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
    return _capabilities.value("dav").toMap().value("chunking").toByteArray() >= "1.0";
}

bool Capabilities::chunkingParallelUploadDisabled() const
{
    return _capabilities.value("dav").toMap().value("chunkingParallelUploadDisabled").toBool();
}

bool Capabilities::privateLinkPropertyAvailable() const
{
    return _capabilities.value("files").toMap().value("privateLinks").toBool();
}

bool Capabilities::privateLinkDetailsParamAvailable() const
{
    return _capabilities.value("files").toMap().value("privateLinksDetailsParam").toBool();
}

QList<int> Capabilities::httpErrorCodesThatResetFailingChunkedUploads() const
{
    QList<int> list;
    foreach (const auto &t, _capabilities.value("dav").toMap().value("httpErrorCodesThatResetFailingChunkedUploads").toList()) {
        list.push_back(t.toInt());
    }
    return list;
}

QString Capabilities::invalidFilenameRegex() const
{
    return _capabilities[QStringLiteral("dav")].toMap()[QStringLiteral("invalidFilenameRegex")].toString();
}

bool Capabilities::uploadConflictFiles() const
{
    static auto envIsSet = !qEnvironmentVariableIsEmpty("OWNCLOUD_UPLOAD_CONFLICT_FILES");
    static int envValue = qEnvironmentVariableIntValue("OWNCLOUD_UPLOAD_CONFLICT_FILES");
    if (envIsSet)
        return envValue != 0;

    return _capabilities[QStringLiteral("uploadConflictFiles")].toBool();
}

bool Capabilities::versioningEnabled() const
{
    return _capabilities.value("files").toMap().value("versioning").toBool();
}

QString Capabilities::zsyncSupportedVersion() const
{
    return _capabilities[QStringLiteral("dav")].toMap()[QStringLiteral("zsync")].toString();
}

QStringList Capabilities::blacklistedFiles() const
{
    return _capabilities.value("files").toMap().value("blacklisted_files").toStringList();
}
}
