/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include "owncloudlib.h"

#include <QVariantMap>
#include <QStringList>

namespace OCC {

/**
 * @brief The Capabilities class represents the capabilities of an ownCloud
 * server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Capabilities {

public:
    Capabilities(const QVariantMap &capabilities);

    bool shareAPI() const;
    bool sharePublicLink() const;
    bool sharePublicLinkAllowUpload() const;
    bool sharePublicLinkEnforcePassword() const;
    bool sharePublicLinkEnforceExpireDate() const;
    int  sharePublicLinkExpireDateDays() const;
    bool shareResharing() const;
    bool chunkingNg() const;

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
    QList<QByteArray> supportedChecksumTypes() const;

    /**
     * The checksum algorithm that the server recommends for file uploads.
     * This is just a preference, any algorithm listed in supportedTypes may be used.
     *
     * Path: checksums/preferredUploadType
     * Default: empty, meaning "no preference"
     * Possible values: empty or any of the supportedTypes
     */
    QByteArray preferredUploadChecksumType() const;

    /**
     * Helper that returns the preferredUploadChecksumType() if set, or one
     * of the supportedChecksumTypes() if it isn't. May return an empty
     * QByteArray if no checksum types are supported.
     */
    QByteArray uploadChecksumType() const;

private:
    QVariantMap _capabilities;
};

}

#endif //CAPABILITIES_H
