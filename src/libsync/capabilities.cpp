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

using namespace std::chrono;

namespace OCC {


Capabilities::Capabilities(const QVariantMap &capabilities)
    : _capabilities(capabilities)
    , _fileSharingCapabilities(_capabilities.value(QStringLiteral("files_sharing")).toMap())
    , _fileSharingPublicCapabilities(_fileSharingCapabilities.value(QStringLiteral("public"), {}).toMap())
    , _tusSupport(_capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("tus_support")).toMap())
    , _spaces(_capabilities.value(QStringLiteral("spaces")).toMap())
    , _status(_capabilities.value(QStringLiteral("core")).toMap().value(QStringLiteral("status")).toMap())
{
}

QVariantMap Capabilities::raw() const
{
    return _capabilities;
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

static bool getEnforcePasswordCapability(const QVariantMap &fileSharingPublicCapabilities, const QString &name)
{
    auto value = fileSharingPublicCapabilities[QStringLiteral("password")].toMap()[QStringLiteral("enforced_for")].toMap()[name];
    if (!value.isValid())
        return fileSharingPublicCapabilities[QStringLiteral("password")].toMap()[QStringLiteral("enforced")].toBool();
    return value.toBool();
}

bool Capabilities::sharePublicLinkEnforcePasswordForReadOnly() const
{
    return getEnforcePasswordCapability(_fileSharingPublicCapabilities, QStringLiteral("read_only"));
}

bool Capabilities::sharePublicLinkEnforcePasswordForReadWrite() const
{
    return getEnforcePasswordCapability(_fileSharingPublicCapabilities, QStringLiteral("read_write"));
}

bool Capabilities::sharePublicLinkEnforcePasswordForUploadOnly() const
{
    return getEnforcePasswordCapability(_fileSharingPublicCapabilities, QStringLiteral("upload_only"));
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

std::chrono::seconds Capabilities::remotePollInterval() const
{
    return duration_cast<seconds>(milliseconds(_capabilities.value(QStringLiteral("core")).toMap().value(QStringLiteral("pollinterval")).toInt()));
}

bool Capabilities::notificationsAvailable() const
{
    // We require the OCS style API in 9.x, can't deal with the REST one only found in 8.2
    return _capabilities.contains(QStringLiteral("notifications")) && _capabilities.value(QStringLiteral("notifications")).toMap().contains(QStringLiteral("ocs-endpoints"));
}

bool Capabilities::isValid() const
{
    return !_capabilities.isEmpty();
}

QList<QByteArray> Capabilities::supportedChecksumTypes() const
{
    QList<QByteArray> list;
    const auto &supportedTypes = _capabilities.value(QStringLiteral("checksums")).toMap().value(QStringLiteral("supportedTypes")).toList();
    for (const auto &t : supportedTypes) {
        list.push_back(t.toByteArray());
    }
    return list;
}

QByteArray Capabilities::preferredUploadChecksumType() const
{
    return qEnvironmentVariable("OWNCLOUD_CONTENT_CHECKSUM_TYPE",
                                _capabilities.value(QStringLiteral("checksums")).toMap()
                                .value(QStringLiteral("preferredUploadType"), QStringLiteral("SHA1")).toString()).toUtf8();
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
    if (!bigfilechunkingEnabled())
    {
        return false;
    }
    static const auto chunkng = qgetenv("OWNCLOUD_CHUNKING_NG");
    if (chunkng == "0")
        return false;
    if (chunkng == "1")
        return true;
    return _capabilities.value(QStringLiteral("dav")).toMap().value(QStringLiteral("chunking")).toByteArray() >= "1.0";
}

bool Capabilities::bigfilechunkingEnabled() const
{
    bool ok;
    const int chunkSize = qEnvironmentVariableIntValue("OWNCLOUD_CHUNK_SIZE", &ok);
    if (ok && chunkSize == 0)
    {
        return false;
    }
    return _capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("bigfilechunking"), true).toBool();
}

const Status &Capabilities::status() const
{
    return _status;
}

const TusSupport &Capabilities::tusSupport() const
{
    return _tusSupport;
}

const SpaceSupport &Capabilities::spacesSupport() const
{
    return _spaces;
}

bool Capabilities::chunkingParallelUploadDisabled() const
{
    return _capabilities.value(QStringLiteral("dav")).toMap().value(QStringLiteral("chunkingParallelUploadDisabled")).toBool();
}

bool Capabilities::privateLinkPropertyAvailable() const
{
    return _capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("privateLinks")).toBool();
}

bool Capabilities::privateLinkDetailsParamAvailable() const
{
    return _capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("privateLinksDetailsParam")).toBool();
}

QList<int> Capabilities::httpErrorCodesThatResetFailingChunkedUploads() const
{
    QList<int> list;
    const auto &errorCodes = _capabilities.value(QStringLiteral("dav")).toMap().value(QStringLiteral("httpErrorCodesThatResetFailingChunkedUploads")).toList();
    for (const auto &t : errorCodes) {
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
    return _capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("versioning")).toBool();
}

bool Capabilities::avatarsAvailable() const
{
    auto userCaps = _fileSharingCapabilities.value(QStringLiteral("user")).toMap();
    // true by default for older servers, because turning off profile pictures was introduced in later versions
    return userCaps.value(QStringLiteral("profile_picture"), true).toBool();
}

QStringList Capabilities::blacklistedFiles() const
{
    return _capabilities.value(QStringLiteral("files")).toMap().value(QStringLiteral("blacklisted_files")).toStringList();
}

Status::Status(const QVariantMap &status)
{
    legacyVersion = QVersionNumber::fromString(status.value(QStringLiteral("version")).toString());
    legacyVersionString = status.value(QStringLiteral("versionstring")).toString();
    edition = status.value(QStringLiteral("edition")).toString();
    productname = status.value(QStringLiteral("productname")).toString();
    product = status.value(QStringLiteral("product")).toString();
    productversion = status.value(QStringLiteral("productversion")).toString();
}

QVersionNumber Status::version() const
{
    return productversion.isEmpty() ? legacyVersion : QVersionNumber::fromString(productversion);
}

QString Status::versionString() const
{
    return productversion.isEmpty() ? legacyVersionString : productversion;
}

TusSupport::TusSupport(const QVariantMap &tus_support)
{
    if (tus_support.isEmpty() || qEnvironmentVariableIsSet("OWNCLOUD_NO_TUS")) {
        return;
    }
    version = QVersionNumber::fromString(tus_support.value(QStringLiteral("version")).toString());
    resumable = QVersionNumber::fromString(tus_support.value(QStringLiteral("resumable")).toString());

    extensions = tus_support.value(QStringLiteral("extension")).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    max_chunk_size = tus_support.value(QStringLiteral("max_chunk_size")).value<quint64>();
    http_method_override = tus_support.value(QStringLiteral("http_method_override")).toString();
}

bool TusSupport::isValid() const
{
    return !version.isNull();
}

SpaceSupport::SpaceSupport(const QVariantMap &spaces_support)
{
    if (spaces_support.isEmpty()) {
        return;
    }
    enabled = spaces_support.value(QStringLiteral("enabled")).toBool();
    version = QVersionNumber::fromString(spaces_support.value(QStringLiteral("version")).toString());
}

bool SpaceSupport::isValid() const
{
    return !version.isNull();
}


} // namespace OCC
