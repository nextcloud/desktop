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

#include <QNetworkReply>
#include "syncfileitem.h"

namespace OCC {

inline QByteArray parseEtag(const char *header) {
    if (!header)
        return QByteArray();
    QByteArray arr = header;
    arr.replace("-gzip", ""); // https://github.comowncloud/client/issues/1195
    if(arr.length() >= 2 && arr.startsWith('"') && arr.endsWith('"')) {
        arr = arr.mid(1, arr.length() - 2);
    }
    return arr;
}

inline QByteArray getEtagFromReply(QNetworkReply *reply)
{
    QByteArray ret = parseEtag(reply->rawHeader("OC-ETag"));
    if (ret.isEmpty()) {
        ret = parseEtag(reply->rawHeader("ETag"));
    }
    return ret;
}

/**
 * Fiven an error from the network, map to a SyncFileItem::Status error
 */
inline SyncFileItem::Status classifyError(QNetworkReply::NetworkError nerror, int httpCode) {
    Q_ASSERT (nerror != QNetworkReply::NoError); // we should only be called when there is an error

    if (nerror > QNetworkReply::NoError &&  nerror <= QNetworkReply::UnknownProxyError) {
        // network error or proxy error -> fatal
        return SyncFileItem::FatalError;
    }

    if (httpCode == 412) {
        // "Precondition Failed"
        // Happens when the e-tag has changed
        return SyncFileItem::SoftError;
    }

    return SyncFileItem::NormalError;
}

}
