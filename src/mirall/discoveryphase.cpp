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

bool DiscoveryJob::isInBlackList(const QString& path) const
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

	if (it == _selectiveSyncBlackList.begin()) {
        return false;
    }
    --it;
    if (pathSlash.startsWith(*it + QLatin1Char('/'))) {
        return true;
    }
    return false;
}

int DiscoveryJob::isInWhiteListCallBack(void *data, const char *path)
{
    return static_cast<DiscoveryJob*>(data)->isInBlackList(QString::fromUtf8(path));
}


void DiscoveryJob::start() {
    _selectiveSyncBlackList.sort();
    _csync_ctx->checkBlackListHook = isInWhiteListCallBack;
    _csync_ctx->checkBlackListData = this;
    csync_set_log_callback(_log_callback);
    csync_set_log_level(_log_level);
    csync_set_log_userdata(_log_userdata);
    emit finished(csync_update(_csync_ctx));
    deleteLater();
}
