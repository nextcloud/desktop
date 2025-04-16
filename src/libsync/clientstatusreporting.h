/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
