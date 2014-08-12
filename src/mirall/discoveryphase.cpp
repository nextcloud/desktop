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

bool DiscoveryJob::isInWhiteList(const QString& path) const
{
    if (_selectiveSyncWhiteList.isEmpty()) {
        // If there is no white list, everything is allowed
        return true;
    }

    // If the path is a prefix of any item of the list, this means we need to go deeper, so we sync.
    //  (this means it was partially checked)
    // If one of the item in the white list is a prefix of the path, it means this path need to
    // be synced.
    //
    // We know the list is sorted (for it is done in DiscoveryJob::start)
    // So we can do a binary search. If the path is a prefix if another item, this item will be
    // equal, or right after in the lexical order.
    // If an item has the path as a prefix, it will be right before in the lexicographic order.

    QString pathSlash = path + QLatin1Char('/');

    auto it = std::lower_bound(_selectiveSyncWhiteList.begin(), _selectiveSyncWhiteList.end(), pathSlash);
    if (it != _selectiveSyncWhiteList.end() && (*it + QLatin1Char('/')).startsWith(pathSlash)) {
        // If the path is a prefix of something in the white list, we need to sync the contents
        return true;
    }

    // If the item before is a prefix of the path, we are also good
    if (it == _selectiveSyncWhiteList.begin()) {
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
    return static_cast<DiscoveryJob*>(data)->isInWhiteList(QString::fromUtf8(path));
}


void DiscoveryJob::start() {
    _selectiveSyncWhiteList.sort();
    _csync_ctx->checkWhiteListHook = isInWhiteListCallBack;
    _csync_ctx->checkWhiteListData = this;
    csync_set_log_callback(_log_callback);
    csync_set_log_level(_log_level);
    csync_set_log_userdata(_log_userdata);
    emit finished(csync_update(_csync_ctx));
    deleteLater();
}
