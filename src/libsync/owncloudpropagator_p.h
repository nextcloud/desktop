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
#include "networkjobs.h"
#include <QLoggingCategory>
#include <QNetworkReply>

namespace OCC {

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
    int httpCode, bool *anotherSyncNeeded = nullptr, const QByteArray &errorBody = QByteArray())
{
    Q_ASSERT(nerror != QNetworkReply::NoError); // we should only be called when there is an error

    if (nerror == QNetworkReply::RemoteHostClosedError) {
        // Sometimes server bugs lead to a connection close on certain files,
        // that shouldn't bring the rest of the syncing to a halt.
        return SyncFileItem::NormalError;
    }

    if (nerror > QNetworkReply::NoError && nerror <= QNetworkReply::UnknownProxyError) {
        // network error or proxy error -> fatal
        return SyncFileItem::FatalError;
    }

    if (httpCode == 503) {
        // When the server is in maintenance mode, we want to exit the sync immediatly
        // so that we do not flood the server with many requests
        // BUG: This relies on a translated string and is thus unreliable.
        //      In the future it should return a NormalError and trigger a status.php
        //      check that detects maintenance mode reliably and will terminate the sync run.
        auto probablyMaintenance =
                errorBody.contains(R"(>Sabre\DAV\Exception\ServiceUnavailable<)")
                && !errorBody.contains("Storage is temporarily not available");
        return probablyMaintenance ? SyncFileItem::FatalError : SyncFileItem::NormalError;
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
        return SyncFileItem::FileLocked;
    }

    return SyncFileItem::NormalError;
}
}
