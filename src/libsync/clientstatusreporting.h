/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include <common/result.h>
#include "clientstatusreportingcommon.h"

#include <memory>

#include <QByteArray>
#include <QHash>
#include <QSharedPointer>

namespace OCC {

class Account;
class ClientStatusReportingDatabase;
class ClientStatusReportingNetwork;
struct ClientStatusReportingRecord;

class OWNCLOUDSYNC_EXPORT ClientStatusReporting
{
public:
    explicit ClientStatusReporting(Account *account);
    ~ClientStatusReporting();

private:
    // reporting must happen via Account
    void reportClientStatus(const ClientStatusReportingStatus status) const;

    bool _isInitialized = false;

    QHash<int, QByteArray> _statusStrings;

    QSharedPointer<ClientStatusReportingDatabase> _database;

    std::unique_ptr<ClientStatusReportingNetwork> _reporter;

    friend class Account;
};
}
