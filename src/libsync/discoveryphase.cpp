/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "discoveryphase.h"
#include <csync_private.h>
#include <qdebug.h>

#include <QUrl>

namespace OCC {

bool DiscoveryJob::isInSelectiveSyncBlackList(const QString& path) const
{
    if (_selectiveSyncBlackList.isEmpty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // If one of the item in the black list is a prefix of the path, it means this path need not to
    // be synced.
    //
    // We know the list is sorted (for it is done in DiscoveryJob::start)
    // So we can do a binary search. If the path is a prefix if another item or right after in the lexical order.

    QString pathSlash = path + QLatin1Char('/');

    auto it = std::lower_bound(_selectiveSyncBlackList.begin(), _selectiveSyncBlackList.end(), pathSlash);

    if (it != _selectiveSyncBlackList.end() && *it == pathSlash) {
        return true;
    }

	if (it == _selectiveSyncBlackList.begin()) {
        return false;
    }
    --it;
    Q_ASSERT(it->endsWith(QLatin1Char('/'))); // Folder::setSelectiveSyncBlackList makes sure of that
    if (pathSlash.startsWith(*it)) {
        return true;
    }
    return false;
}

int DiscoveryJob::isInSelectiveSyncBlackListCallBack(void *data, const char *path)
{
    return static_cast<DiscoveryJob*>(data)->isInSelectiveSyncBlackList(QString::fromUtf8(path));
}

void DiscoveryJob::update_job_update_callback (bool local,
                                    const char *dirUrl,
                                    void *userdata)
{
    DiscoveryJob *updateJob = static_cast<DiscoveryJob*>(userdata);
    if (updateJob) {
        // Don't wanna overload the UI
        if (!updateJob->lastUpdateProgressCallbackCall.isValid()) {
            updateJob->lastUpdateProgressCallbackCall.restart(); // first call
        } else if (updateJob->lastUpdateProgressCallbackCall.elapsed() < 200) {
            return;
        } else {
            updateJob->lastUpdateProgressCallbackCall.restart();
        }

        QString path(QUrl::fromPercentEncoding(QByteArray(dirUrl)).section('/', -1));
        emit updateJob->folderDiscovered(local, path);
    }
}

void DiscoveryJob::start() {
    _selectiveSyncBlackList.sort();
    _csync_ctx->checkSelectiveSyncBlackListHook = isInSelectiveSyncBlackListCallBack;
    _csync_ctx->checkSelectiveSyncBlackListData = this;

    _csync_ctx->callbacks.update_callback = update_job_update_callback;
    _csync_ctx->callbacks.update_callback_userdata = this;


    csync_set_log_callback(_log_callback);
    csync_set_log_level(_log_level);
    csync_set_log_userdata(_log_userdata);
    lastUpdateProgressCallbackCall.invalidate();
    int ret = csync_update(_csync_ctx);

    _csync_ctx->checkSelectiveSyncBlackListHook = 0;
    _csync_ctx->checkSelectiveSyncBlackListData = 0;

    _csync_ctx->callbacks.update_callback = 0;
    _csync_ctx->callbacks.update_callback_userdata = 0;

    emit finished(ret);
    deleteLater();
}

}
