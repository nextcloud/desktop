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


#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include "owncloudlib.h"

#include "common/checksumalgorithms.h"

#include <QVariantMap>
#include <QStringList>
#include <QVersionNumber>

namespace OCC {

struct OWNCLOUDSYNC_EXPORT Status
{
    /**
     <installed>1</installed>
    <maintenance>0</maintenance>
    <needsDbUpgrade>0</needsDbUpgrade>
    <version>10.11.0.0</version>
    <versionstring>10.11.0</versionstring>
    <edition>Community</edition>
    <productname>Infinite Scale</productname>
    <product>Infinite Scale</product>
    <productversion>2.0.0-beta1+7c2e3201b</productversion>
    */

    Status(const QVariantMap &status);
    // legacy version
    QVersionNumber legacyVersion;
    // legacy version
    QString legacyVersionString;

    QString edition;
    QString productname;
    QString product;
    QString productversion;

    QVersionNumber version() const;
    QString versionString() const;
};

struct OWNCLOUDSYNC_EXPORT TusSupport
{
    /**
    <tus_support>
    <version>1.0.0</version>
    <resumable>1.0.0</resumable>
    <extension>creation,creation-with-upload</extension>
    <max_chunk_size>0</max_chunk_size>
    <http_method_override/>
    </tus_support>
    */
    TusSupport(const QVariantMap &tus_support);
    QVersionNumber version;
    QVersionNumber resumable;
    QStringList extensions;
    quint64 max_chunk_size;
    QString http_method_override;

    bool isValid() const;
};

struct OWNCLOUDSYNC_EXPORT SpaceSupport
{
    /**
        "spaces": {
          "version": "0.0.1",
          "enabled": true
        }
    */
    SpaceSupport(const QVariantMap &spaces_support);
    bool enabled = false;
    QVersionNumber version;

    bool isValid() const;
};

/**
 * @brief The Capabilities class represents the capabilities of an ownCloud
 * server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Capabilities
{
public:
    Capabilities(const QVariantMap &capabilities);

    bool shareAPI() const;
    bool sharePublicLink() const;
    bool sharePublicLinkAllowUpload() const;
    bool sharePublicLinkSupportsUploadOnly() const;

    /** Whether read-only link shares require a password.
     *
     * Returns sharePublicLinkEnforcePassword() if the fine-grained
     * permission isn't available.
     */
    bool sharePublicLinkEnforcePasswordForReadOnly() const;
    bool sharePublicLinkEnforcePasswordForReadWrite() const;
    bool sharePublicLinkEnforcePasswordForUploadOnly() const;

    bool sharePublicLinkDefaultExpire() const;
    int sharePublicLinkDefaultExpireDateDays() const;
    bool sharePublicLinkEnforceExpireDate() const;
    bool sharePublicLinkMultiple() const;
    bool shareResharing() const;
    /** Remote Poll interval.
     *
     *  returns the requested poll interval in seconds to be used by the client.
     *  @returns 0 if no capability is set.
     */
    std::chrono::seconds remotePollInterval() const;

    // TODO: return SharePermission
    int defaultPermissions() const;

    bool chunkingNg() const;

    /// Wheter to use chunking
    bool bigfilechunkingEnabled() const;

    const Status &status() const;
    const TusSupport &tusSupport() const;
    const SpaceSupport &spacesSupport() const;

    /// disable parallel upload in chunking
    bool chunkingParallelUploadDisabled() const;

    /// Whether the "privatelink" DAV property is available
    bool privateLinkPropertyAvailable() const;

    /// Whether the "privatelink" DAV property supports the 'details' param
    bool privateLinkDetailsParamAvailable() const;

    /// returns true if the capabilities report notifications
    bool notificationsAvailable() const;

    /// returns true if the capabilities are loaded already.
    bool isValid() const;

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
    QList<OCC::CheckSums::Algorithm> supportedChecksumTypes() const;

    /**
     * The checksum algorithm that the server recommends for file uploads.
     * This is just a preference, any algorithm listed in supportedTypes may be used.
     *
     * Path: checksums/preferredUploadType
     */
    OCC::CheckSums::Algorithm preferredUploadChecksumType() const;

    /**
     * Helper that returns the preferredUploadChecksumType() if set, or one
     * of the supportedChecksumTypes() if it isn't.
     */
    CheckSums::Algorithm uploadChecksumType() const;

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
    QList<int> httpErrorCodesThatResetFailingChunkedUploads() const;

    /**
     * Regex that, if contained in a filename, will result in it not being uploaded.
     *
     * For servers older than 8.1.0 it defaults to [\\:?*"<>|]
     * For servers >= that version, it defaults to the empty regex (the server
     * will indicate invalid characters through an upload error)
     *
     * Note that it just needs to be contained. The regex [ab] is contained in "car".
     */
    QString invalidFilenameRegex() const;

    /**
     * return the list of filename that should not be uploaded
     */
    QStringList blacklistedFiles() const;

    /**
     * Whether conflict files should remain local (default) or should be uploaded.
     */
    bool uploadConflictFiles() const;

    /** Is versioning available? */
    bool versioningEnabled() const;

    /** Are avatars (profile pictures) available? */
    bool avatarsAvailable() const;


    QVariantMap raw() const;

private:
    QVariantMap _capabilities;
    QVariantMap _fileSharingCapabilities;
    QVariantMap _fileSharingPublicCapabilities;
    TusSupport _tusSupport;
    SpaceSupport _spaces;
    Status _status;
};
}

#endif //CAPABILITIES_H
