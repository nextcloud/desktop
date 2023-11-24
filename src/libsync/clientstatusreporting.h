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
#include "accountfwd.h"
#include <common/result.h>

#include <QObject>
#include <QHash>
#include <QByteArray>
#include <qstring.h>
#include <QTimer>
#include <QtSql>
#include <QPair>
#include <QRecursiveMutex>

namespace OCC {

class Account;
struct ClientStatusReportingRecord;

class OWNCLOUDSYNC_EXPORT ClientStatusReporting : public QObject
{
    Q_OBJECT
public:
    enum Status {
        DownloadError_ConflictInvalidCharacters = 0,
        DownloadError_ConflictCaseClash,
        UploadError_ServerError,
        Count,
    };

    explicit ClientStatusReporting(Account *account, QObject *parent = nullptr);
    ~ClientStatusReporting() = default;

    void reportClientStatus(const Status status);

    void init();

private:
    [[nodiscard]] Result<void, QString> setClientStatusReportingRecord(const ClientStatusReportingRecord &record);
    [[nodiscard]] QVector<ClientStatusReportingRecord> getClientStatusReportingRecords() const;
    [[nodiscard]] bool deleteClientStatusReportingRecords();
    [[nodiscard]] bool setLastSentReportTimestamp(const qulonglong timestamp);
    [[nodiscard]] qulonglong getLastSentReportTimestamp() const;

private slots:
    void sendReportToServer();
    void slotSendReportToserverFinished();

private:
    static QByteArray statusStringFromNumber(const Status status);
    static QString classifyStatus(const Status status);
    Account *_account = nullptr;
    QHash<int, QPair<QByteArray, qint64>> _statusNamesAndHashes;
    QSqlDatabase _database;
    bool _isInitialized = false;
    QTimer _clientStatusReportingSendTimer;
    mutable QRecursiveMutex _mutex;
};
}
