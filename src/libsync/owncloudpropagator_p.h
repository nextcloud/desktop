/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#pragma once

#include "owncloudpropagator.h"
#include "syncfileitem.h"
#include <QLoggingCategory>
#include <QNetworkReply>

namespace OCC {

inline QByteArray parseEtag(const char *header)
{
    if (!header)
        return QByteArray();
    QByteArray arr = header;

    // Weak E-Tags can appear when gzip compression is on, see #3946
    if (arr.startsWith("W/"))
        arr = arr.mid(2);

    // https://github.com/owncloud/client/issues/1195
    arr.replace("-gzip", "");

    if (arr.length() >= 2 && arr.startsWith('"') && arr.endsWith('"')) {
        arr = arr.mid(1, arr.length() - 2);
    }
    return arr;
}

inline QByteArray getEtagFromReply(QNetworkReply *reply)
{
    QByteArray ocEtag = parseEtag(reply->rawHeader("OC-ETag"));
    QByteArray etag = parseEtag(reply->rawHeader("ETag"));
    QByteArray ret = ocEtag;
    if (ret.isEmpty()) {
        ret = etag;
    }
    if (ocEtag.length() > 0 && ocEtag != etag) {
        qCDebug(lcPropagator) << "Quite peculiar, we have an etag != OC-Etag [no problem!]" << etag << ocEtag;
    }
    return ret;
}

/**
 * Given an error from the network, map to a SyncFileItem::Status error
 */
inline SyncFileItem::Status classifyError(QNetworkReply::NetworkError nerror,
    int httpCode,
    bool *anotherSyncNeeded = NULL)
{
    Q_ASSERT(nerror != QNetworkReply::NoError); // we should only be called when there is an error

    if (nerror > QNetworkReply::NoError && nerror <= QNetworkReply::UnknownProxyError) {
        // network error or proxy error -> fatal
        return SyncFileItem::FatalError;
    }

    if (httpCode == 503) {
        // "Service unavailable"
        // Happens for maintenance mode and other temporary outages
        return SyncFileItem::FatalError;
    }

    if (httpCode == 412) {
        // "Precondition Failed"
        // Happens when the e-tag has changed
        return SyncFileItem::SoftError;
    }

    if (httpCode == 423) {
        // "Locked"
        // Should be temporary.
        if (anotherSyncNeeded) {
            *anotherSyncNeeded = true;
        }
        return SyncFileItem::SoftError;
    }

    return SyncFileItem::NormalError;
}
}
