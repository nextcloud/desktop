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

#include "accountfwd.h"
#include "common/utility.h"
#include "owncloudlib.h"

#include <OAIDrive.h>

#include <algorithm>
#include <unordered_map>

class QTimer;

namespace OCC {
namespace GraphApi {

    class OWNCLOUDSYNC_EXPORT SpacesManager : public QObject
    {
        Q_OBJECT

    public:
        SpacesManager(Account *parent);

        OpenAPI::OAIDrive drive(const QString &id) const;

        // deprecated: we need to migrate to id based spaces
        OpenAPI::OAIDrive driveByUrl(const QUrl &url) const;

        void refresh();

    private:
        Account *_account;
        QTimer *_refreshTimer;
        std::unordered_map<QString, OpenAPI::OAIDrive> _drivesMap;
    };

}
}
