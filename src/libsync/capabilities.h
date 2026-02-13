/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include "owncloudlib.h"

#include <QVariantMap>
#include <QStringList>
#include <QMimeDatabase>
#include <QColor>

namespace OCC {

class DirectEditor;

enum PushNotificationType {
    None = 0,
    Files = 1,
    Activities = 2,
    Notifications = 4
};
Q_DECLARE_FLAGS(PushNotificationTypes, PushNotificationType)
Q_DECLARE_OPERATORS_FOR_FLAGS(PushNotificationTypes)

/**
 * @brief The Capabilities class represents the capabilities of an ownCloud
 * server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Capabilities
{
public:
    Capabilities(const QVariantMap &capabilities);

    [[nodiscard]] bool shareAPI() const;
    [[nodiscard]] bool shareEmailPasswordEnabled() const;
    [[nodiscard]] bool shareEmailPasswordEnforced() const;
    [[nodiscard]] bool sharePublicLink() const;
    [[nodiscard]] bool sharePublicLinkAllowUpload() const;
    [[nodiscard]] bool sharePublicLinkSupportsUploadOnly() const;
    [[nodiscard]] bool sharePublicLinkAskOptionalPassword() const;
    [[nodiscard]] bool sharePublicLinkEnforcePassword() const;
    [[nodiscard]] bool sharePublicLinkEnforceExpireDate() const;
    [[nodiscard]] int sharePublicLinkExpireDateDays() const;
    [[nodiscard]] bool shareInternalEnforceExpireDate() const;
    [[nodiscard]] int shareInternalExpireDateDays() const;
    [[nodiscard]] bool shareRemoteEnforceExpireDate() const;
    [[nodiscard]] int shareRemoteExpireDateDays() const;
    [[nodiscard]] bool sharePublicLinkMultiple() const;
    [[nodiscard]] bool shareResharing() const;
    [[nodiscard]] int shareDefaultPermissions() const;
    [[nodiscard]] bool chunkingNg() const;
    [[nodiscard]] qint64 maxChunkSize() const;
    [[nodiscard]] int maxConcurrentChunkUploads() const;
    [[nodiscard]] bool bulkUpload() const;
    [[nodiscard]] bool filesLockAvailable() const;
    [[nodiscard]] bool filesLockTypeAvailable() const;
    [[nodiscard]] bool userStatus() const;
    [[nodiscard]] bool userStatusSupportsEmoji() const;
    [[nodiscard]] bool userStatusSupportsBusy() const;
    [[nodiscard]] bool ncAssistantEnabled() const;
    [[nodiscard]] QColor serverColor() const;
    [[nodiscard]] QColor serverTextColor() const;

    /// Returns which kind of push notifications are available
    [[nodiscard]] PushNotificationTypes availablePushNotifications() const;

    /// Websocket url for files push notifications if available
    [[nodiscard]] QUrl pushNotificationsWebSocketUrl() const;

    /// disable parallel upload in chunking
    [[nodiscard]] bool chunkingParallelUploadDisabled() const;

    /// Whether the "privatelink" DAV property is available
    [[nodiscard]] bool privateLinkPropertyAvailable() const;

    /// returns true if the capabilities report notifications
    [[nodiscard]] bool notificationsAvailable() const;

    /// returns true if the server supports client side encryption
    [[nodiscard]] bool clientSideEncryptionAvailable() const;

    [[nodiscard]] double clientSideEncryptionVersion() const;

    /// returns true if the capabilities are loaded already.
    [[nodiscard]] bool isValid() const;

    /// return true if the activity app is enabled
    [[nodiscard]] bool hasActivities() const;

    [[nodiscard]] bool isClientStatusReportingEnabled() const;

    /**
     * Returns the checksum types the server understands.
     *
     * When the client uses one of these checksumming algorithms in
     * the OC-Checksum header of a file upload, the server will use
     * it to validate that data was transmitted correctly.
     *
     * Path: checksums/supportedTypes
     * Default: []
     * Possible entries: "Adler32", "MD5", "SHA1"
     */
    [[nodiscard]] QList<QByteArray> supportedChecksumTypes() const;

    /**
     * The checksum algorithm that the server recommends for file uploads.
     * This is just a preference, any algorithm listed in supportedTypes may be used.
     *
     * Path: checksums/preferredUploadType
     * Default: empty, meaning "no preference"
     * Possible values: empty or any of the supportedTypes
     */
    [[nodiscard]] QByteArray preferredUploadChecksumType() const;

    /**
     * Helper that returns the preferredUploadChecksumType() if set, or one
     * of the supportedChecksumTypes() if it isn't. May return an empty
     * QByteArray if no checksum types are supported.
     */
    [[nodiscard]] QByteArray uploadChecksumType() const;

    /**
     * List of HTTP error codes should be guaranteed to eventually reset
     * failing chunked uploads.
     *
     * The resetting works by tracking UploadInfo::errorCount.
     *
     * Note that other error codes than the ones listed here may reset the
     * upload as well.
     *
     * Motivation: See #5344. They should always be reset on 412 (possibly
     * checksum error), but broken servers may also require resets on
     * unusual error codes such as 503.
     *
     * Path: dav/httpErrorCodesThatResetFailingChunkedUploads
     * Default: []
     * Example: [503, 500]
     */
    [[nodiscard]] QList<int> httpErrorCodesThatResetFailingChunkedUploads() const;

    /**
     * Regex that, if contained in a filename, will result in it not being uploaded.
     *
     * For servers older than 8.1.0 it defaults to [\\:?*"<>|]
     * For servers >= that version, it defaults to the empty regex (the server
     * will indicate invalid characters through an upload error)
     *
     * Note that it just needs to be contained. The regex [ab] is contained in "car".
     */
    [[nodiscard]] QString invalidFilenameRegex() const;

    /**
     * return the list of filename that should not be uploaded
     */
    [[nodiscard]] QStringList blacklistedFiles() const;

    [[nodiscard]] QStringList forbiddenFilenameCharacters() const;
    [[nodiscard]] QStringList forbiddenFilenameBasenames() const;
    [[nodiscard]] QStringList forbiddenFilenameExtensions() const;
    [[nodiscard]] QStringList forbiddenFilenames() const;

    /**
     * Whether conflict files should remain local (default) or should be uploaded.
     */
    [[nodiscard]] bool uploadConflictFiles() const;

    [[nodiscard]] bool groupFoldersAvailable() const;

    [[nodiscard]] bool serverHasValidSubscription() const;
    [[nodiscard]] QString desktopEnterpriseChannel() const;

    [[nodiscard]] bool serverHasClientIntegration() const;
    [[nodiscard]] QList<QVariantMap> fileActionsByMimeType(const QMimeType &fileMimeType) const;

    // Direct Editing
    void addDirectEditor(DirectEditor* directEditor);
    DirectEditor* getDirectEditorForMimetype(const QMimeType &mimeType);
    DirectEditor* getDirectEditorForOptionalMimetype(const QMimeType &mimeType);

private:
    [[nodiscard]] QMap<QString, QVariant> serverThemingMap() const;

    QVariantMap _capabilities;
    QList<DirectEditor*> _directEditors;
};

/*-------------------------------------------------------------------------------------*/

class OWNCLOUDSYNC_EXPORT DirectEditor : public QObject
{
    Q_OBJECT
public:
    DirectEditor(const QString &id, const QString &name, QObject* parent = nullptr);

    void addMimetype(const QByteArray &mimeType);
    void addOptionalMimetype(const QByteArray &mimeType);

    bool hasMimetype(const QMimeType &mimeType);
    bool hasOptionalMimetype(const QMimeType &mimeType);

    [[nodiscard]] QString id() const;
    [[nodiscard]] QString name() const;

    [[nodiscard]] QList<QByteArray> mimeTypes() const;
    [[nodiscard]] QList<QByteArray> optionalMimeTypes() const;

private:
    QString _id;
    QString _name;

    QList<QByteArray> _mimeTypes;
    QList<QByteArray> _optionalMimeTypes;
};

/*-------------------------------------------------------------------------------------*/

}

#endif //CAPABILITIES_H
