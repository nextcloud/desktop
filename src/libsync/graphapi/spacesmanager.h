/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "owncloudlib.h"

#include "libsync/accountfwd.h"
#include "libsync/graphapi/space.h"

#include <OAIDrive.h>

#include <algorithm>

#include <QFuture>

class QTimer;

namespace OCC {
namespace GraphApi {

    class OWNCLOUDSYNC_EXPORT SpacesManager : public QObject
    {
        Q_OBJECT

    public:
        SpacesManager(Account *parent);

        Space *space(const QString &id) const;

        QVector<Space *> spaces() const;

        // deprecated: we need to migrate to id based spaces
        Space *spaceByUrl(const QUrl &url) const;

        Account *account() const;

        /**
         * Only relevant during bootstraping or when disconnected
         */
        void checkReady();

    Q_SIGNALS:
        void spaceChanged(Space *space) const;
        void updated();
        void ready() const;

    private:
        void refresh();

        Account *_account;
        QTimer *_refreshTimer;
        QMap<QString, Space *> _spacesMap;
        bool _ready = false;
    };

}
}
