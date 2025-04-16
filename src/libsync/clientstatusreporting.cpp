/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "clientstatusreporting.h"

#include "account.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingnetwork.h"
#include "clientstatusreportingrecord.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReporting, "nextcloud.sync.clientstatusreporting", QtInfoMsg)

ClientStatusReporting::ClientStatusReporting(Account *account)
{
    for (int i = 0; i < static_cast<int>(ClientStatusReportingStatus::Count); ++i) {
        const auto statusString = clientStatusstatusStringFromNumber(static_cast<ClientStatusReportingStatus>(i));
        _statusStrings[i] = statusString;
    }

    if (_statusStrings.size() < static_cast<int>(ClientStatusReportingStatus::Count)) {
        return;
    }

    _database = QSharedPointer<ClientStatusReportingDatabase>::create(account);
    if (!_database->isInitialized()) {
        return;
    }

    _reporter = std::make_unique<ClientStatusReportingNetwork>(account, _database);
    if (!_reporter->isInitialized()) {
        return;
    }

    _isInitialized = true;
}

ClientStatusReporting::~ClientStatusReporting() = default;

void ClientStatusReporting::reportClientStatus(const ClientStatusReportingStatus status) const
{
    if (!_isInitialized) {
        return;
    }

    Q_ASSERT(static_cast<int>(status) >= 0 && static_cast<int>(status) < static_cast<int>(ClientStatusReportingStatus::Count));
    if (static_cast<int>(status) < 0 || static_cast<int>(status) >= static_cast<int>(ClientStatusReportingStatus::Count)) {
        qCDebug(lcClientStatusReporting) << "Trying to report invalid status:" << static_cast<int>(status);
        return;
    }

    ClientStatusReportingRecord record;
    record._name = _statusStrings[static_cast<int>(status)];
    record._status = static_cast<int>(status);
    record._lastOccurence = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const auto result = _database->setClientStatusReportingRecord(record);
    if (!result.isValid()) {
        qCDebug(lcClientStatusReporting) << "Could not report client status:" << result.error();
    }
}
}
