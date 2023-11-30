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
#include "clientstatusreportingrecord.h"

#include <QtGlobal>
#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QVector>
#include <QSqlDatabase>

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
