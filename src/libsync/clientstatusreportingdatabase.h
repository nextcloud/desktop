/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"
#include <common/result.h>
#include "clientstatusreportingcommon.h"
#include "clientstatusreportingrecord.h"

#include <QByteArray>
#include <QMutex>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>

namespace OCC {

class Account;

class OWNCLOUDSYNC_EXPORT ClientStatusReportingDatabase
{
public:
    explicit ClientStatusReportingDatabase(const Account *account);
    ~ClientStatusReportingDatabase();

    [[nodiscard]] Result<void, QString> setClientStatusReportingRecord(const ClientStatusReportingRecord &record) const;
    [[nodiscard]] QVector<ClientStatusReportingRecord> getClientStatusReportingRecords() const;
    [[nodiscard]] Result<void, QString> deleteClientStatusReportingRecords() const;

    [[nodiscard]] QVariantMap lastSentReport() const;
    void setLastSentReport(const QVariantMap &report) const;

    void setLastSentReportTimestamp(const quint64 timestamp) const;
    [[nodiscard]] quint64 getLastSentReportTimestamp() const;

    [[nodiscard]] Result<void, QString> setStatusNamesHash(const QByteArray &hash) const;
    [[nodiscard]] QByteArray getStatusNamesHash() const;

    [[nodiscard]] bool isInitialized() const;

private:
    [[nodiscard]] QString makeDbPath(const Account *account) const;
    [[nodiscard]] bool updateStatusNamesHash() const;
    [[nodiscard]] QVector<QByteArray> getTableColumns(const QString &table) const;
    [[nodiscard]]bool addColumn(const QString &tableName, const QString &columnName, const QString &dataType, const bool withIndex = false) const;

public:
    // this must be set in unit tests on init
    static QString dbPathForTesting;

private:
    QSqlDatabase _database;

    bool _isInitialized = false;

    // inspired by SyncJournalDb
    mutable QRecursiveMutex _mutex;
};
}
