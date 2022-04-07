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
#include <QUrl>

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

bool Capabilities::shareEmailPasswordEnabled() const
{
    return _capabilities["files_sharing"].toMap()["sharebymail"].toMap()["password"].toMap()["enabled"].toBool();
}

bool Capabilities::shareEmailPasswordEnforced() const
{
    return _capabilities["files_sharing"].toMap()["sharebymail"].toMap()["password"].toMap()["enforced"].toBool();
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

bool Capabilities::sharePublicLinkAskOptionalPassword() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["password"].toMap()["askForOptionalPassword"].toBool();
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

bool Capabilities::shareInternalEnforceExpireDate() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date_internal"].toMap()["enforced"].toBool();
}

int Capabilities::shareInternalExpireDateDays() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date_internal"].toMap()["days"].toInt();
}

bool Capabilities::shareRemoteEnforceExpireDate() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date_remote"].toMap()["enforced"].toBool();
}

int Capabilities::shareRemoteExpireDateDays() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date_remote"].toMap()["days"].toInt();
}

bool Capabilities::sharePublicLinkMultiple() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["multiple"].toBool();
}

bool Capabilities::shareResharing() const
{
    return _capabilities["files_sharing"].toMap()["resharing"].toBool();
}

int Capabilities::shareDefaultPermissions() const
{
    if(_capabilities["files_sharing"].toMap().contains("default_permissions")) {
        return _capabilities["files_sharing"].toMap()["default_permissions"].toInt();
    }
    
    return {};
}

bool Capabilities::clientSideEncryptionAvailable() const
{
    auto it = _capabilities.constFind(QStringLiteral("end-to-end-encryption"));
    if (it == _capabilities.constEnd()) {
        return false;
    }

    const auto properties = (*it).toMap();
    const auto enabled = properties.value(QStringLiteral("enabled"), false).toBool();
    if (!enabled) {
        return false;
    }

    const auto version = properties.value(QStringLiteral("api-version"), "1.0").toByteArray();
    qCInfo(lcServerCapabilities) << "E2EE API version:" << version;
    const auto splittedVersion = version.split('.');

    bool ok = false;
    const auto major = !splittedVersion.isEmpty() ? splittedVersion.at(0).toInt(&ok) : 0;
    if (!ok) {
        qCWarning(lcServerCapabilities) << "Didn't understand version scheme (major), E2EE disabled";
        return false;
    }

    ok = false;
    const auto minor = splittedVersion.size() > 1 ? splittedVersion.at(1).toInt(&ok) : 0;
    if (!ok) {
        qCWarning(lcServerCapabilities) << "Didn't understand version scheme (minor), E2EE disabled";
        return false;
    }

    return major == 1 && minor >= 1;
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

bool Capabilities::hasActivities() const
{
    return _capabilities.contains("activity");
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
    static const auto chunkng = qgetenv("OWNCLOUD_CHUNKING_NG");
    if (chunkng == "0")
        return false;
    if (chunkng == "1")
        return true;
    return _capabilities["dav"].toMap()["chunking"].toByteArray() >= "1.0";
}

bool Capabilities::bulkUpload() const
{
    return _capabilities["dav"].toMap()["bulkupload"].toByteArray() >= "1.0";
}

bool Capabilities::filesLockAvailable() const
{
    return _capabilities["files"].toMap()["locking"].toByteArray() >= "1.0";
}

bool Capabilities::userStatus() const
{
    if (!_capabilities.contains("user_status")) {
        return false;
    }
    const auto userStatusMap = _capabilities["user_status"].toMap();
    return userStatusMap.value("enabled", false).toBool();
}

bool Capabilities::userStatusSupportsEmoji() const
{
    if (!userStatus()) {
        return false;
    }
    const auto userStatusMap = _capabilities["user_status"].toMap();
    return userStatusMap.value("supports_emoji", false).toBool();
}

QColor Capabilities::serverColor() const
{
    const auto themingMap = serverThemingMap();
    return themingMap.contains("color") ? QColor(themingMap["color"].toString()) : QColor();
}

QColor Capabilities::serverTextColor() const
{
    const auto themingMap = serverThemingMap();
    return themingMap.contains("color-text") ? QColor(themingMap["color-text"].toString()) : QColor();
}

QMap<QString, QVariant> Capabilities::serverThemingMap() const
{
    if (!_capabilities.contains("theming")) {
        return {};
    }

    return _capabilities["theming"].toMap();
}



PushNotificationTypes Capabilities::availablePushNotifications() const
{
    if (!_capabilities.contains("notify_push")) {
        return PushNotificationType::None;
    }

    const auto types = _capabilities["notify_push"].toMap()["type"].toStringList();
    PushNotificationTypes pushNotificationTypes;

    if (types.contains("files")) {
        pushNotificationTypes.setFlag(PushNotificationType::Files);
    }

    if (types.contains("activities")) {
        pushNotificationTypes.setFlag(PushNotificationType::Activities);
    }

    if (types.contains("notifications")) {
        pushNotificationTypes.setFlag(PushNotificationType::Notifications);
    }

    return pushNotificationTypes;
}

QUrl Capabilities::pushNotificationsWebSocketUrl() const
{
    const auto websocket = _capabilities["notify_push"].toMap()["endpoints"].toMap()["websocket"].toString();
    return QUrl(websocket);
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

QStringList Capabilities::blacklistedFiles() const
{
    return _capabilities["files"].toMap()["blacklisted_files"].toStringList();
}

/*-------------------------------------------------------------------------------------*/

// Direct Editing
void Capabilities::addDirectEditor(DirectEditor* directEditor)
{
    if(directEditor)
        _directEditors.append(directEditor);
}

DirectEditor* Capabilities::getDirectEditorForMimetype(const QMimeType &mimeType)
{
    foreach(DirectEditor* editor, _directEditors) {
        if(editor->hasMimetype(mimeType))
            return editor;
    }

    return nullptr;
}

DirectEditor* Capabilities::getDirectEditorForOptionalMimetype(const QMimeType &mimeType)
{
    foreach(DirectEditor* editor, _directEditors) {
        if(editor->hasOptionalMimetype(mimeType))
            return editor;
    }

    return nullptr;
}

/*-------------------------------------------------------------------------------------*/

DirectEditor::DirectEditor(const QString &id, const QString &name, QObject* parent)
    : QObject(parent)
    , _id(id)
    , _name(name)
{
}

QString DirectEditor::id() const
{
    return _id;
}

QString DirectEditor::name() const
{
    return _name;
}

void DirectEditor::addMimetype(const QByteArray &mimeType)
{
    _mimeTypes.append(mimeType);
}

void DirectEditor::addOptionalMimetype(const QByteArray &mimeType)
{
    _optionalMimeTypes.append(mimeType);
}

QList<QByteArray> DirectEditor::mimeTypes() const
{
    return _mimeTypes;
}

QList<QByteArray> DirectEditor::optionalMimeTypes() const
{
    return _optionalMimeTypes;
}

bool DirectEditor::hasMimetype(const QMimeType &mimeType)
{
    return _mimeTypes.contains(mimeType.name().toLatin1());
}

bool DirectEditor::hasOptionalMimetype(const QMimeType &mimeType)
{
    return _optionalMimeTypes.contains(mimeType.name().toLatin1());
}

/*-------------------------------------------------------------------------------------*/

}
