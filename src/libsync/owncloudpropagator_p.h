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
#include "helpers.h"
#include "syncfileitem.h"
#include "networkjobs.h"
#include "syncengine.h"
#include <QLoggingCategory>
#include <QNetworkReply>

namespace {

/**
 * We do not want to upload files that are currently being modified.
 * To avoid that, we don't upload files that have a modification time
 * that is too close to the current time.
 *
 * This interacts with the msBetweenRequestAndSync delay in the folder
 * manager. If that delay between file-change notification and sync
 * has passed, we should accept the file for upload here.
 */
inline bool fileIsStillChanging(const OCC::SyncFileItem &item)
{
    const auto modtime = OCC::Utility::qDateTimeFromTime_t(item._modtime);
    const qint64 msSinceMod = modtime.msecsTo(QDateTime::currentDateTimeUtc());

    return std::chrono::milliseconds(msSinceMod) < OCC::SyncEngine::minimumFileAgeForUpload
        // if the mtime is too much in the future we *do* upload the file
        && msSinceMod > -10000;
}

}

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

inline QPair<QByteArray, QByteArray> getExceptionFromReply(QNetworkReply * const reply)
{
    Q_ASSERT(reply);
    if (!reply) {
        qDebug() << "Could not parse null reply";
        return {};
    }
    const auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // only for BadRequest and UnsupportedMediaType
    if (httpStatusCode != 400 && httpStatusCode != 415) {
        return {};
    }

    // we must not modify the buffer
    const auto replyBody = reply->peek(reply->bytesAvailable());

    // parse exception name
    auto exceptionNameStartIndex = replyBody.indexOf(QByteArrayLiteral("<s:exception>"));
    if (exceptionNameStartIndex == -1) {
        qDebug() << "Could not parse exception. No <s:exception> tag.";
        return {};
    }
    exceptionNameStartIndex += QByteArrayLiteral("<s:exception>").size();
    const auto exceptionNameEndIndex = replyBody.indexOf('<', exceptionNameStartIndex);
    if (exceptionNameEndIndex == -1) {
        return {};
    }
    const auto exceptionName = replyBody.mid(exceptionNameStartIndex, exceptionNameEndIndex - exceptionNameStartIndex);

    // parse exception message
    auto exceptionMessageStartIndex = replyBody.indexOf(QByteArrayLiteral("<s:message>"), exceptionNameEndIndex);
    if (exceptionMessageStartIndex == -1) {
        qDebug() << "Could not parse exception. No <s:message> tag.";
        return {exceptionName, {}};
    }
    exceptionMessageStartIndex += QByteArrayLiteral("<s:message>").size();
    const auto exceptionMessageEndIndex = replyBody.indexOf('<', exceptionMessageStartIndex);
    if (exceptionMessageEndIndex == -1) {
        return {exceptionName, {}};
    }

    const auto exceptionMessage = replyBody.mid(exceptionMessageStartIndex, exceptionMessageEndIndex - exceptionMessageStartIndex);

    return {exceptionName, exceptionMessage};
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
        // When the server is in maintenance mode, we want to exit the sync immediately
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
