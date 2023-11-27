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

class TestClientStatusReporting;

namespace OCC {

class Account;
struct ClientStatusReportingRecord;

class OWNCLOUDSYNC_EXPORT ClientStatusReporting : public QObject
{
    Q_OBJECT
public:
    enum Status {
        DownloadError_Cannot_Create_File = 100,
        DownloadError_Conflict = 101,
        DownloadError_ConflictCaseClash = 102,
        DownloadError_ConflictInvalidCharacters = 103,
        DownloadError_No_Free_Space = 104,
        DownloadError_ServerError = 105,
        DownloadError_Virtual_File_Hydration_Failure = 106,
        UploadError_Conflict = 107,
        UploadError_ConflictInvalidCharacters = 108,
        UploadError_No_Free_Space = 109,
        UploadError_No_Write_Permissions = 110,
        UploadError_ServerError = 111,
        Count = UploadError_ServerError + 1,
    };

    explicit ClientStatusReporting(Account *account, QObject *parent = nullptr);
    ~ClientStatusReporting();

private:
    void init();
    void reportClientStatus(const Status status);

    [[nodiscard]] Result<void, QString> setClientStatusReportingRecord(const ClientStatusReportingRecord &record);
    [[nodiscard]] QVector<ClientStatusReportingRecord> getClientStatusReportingRecords() const;
    [[nodiscard]] bool deleteClientStatusReportingRecords();
    void setLastSentReportTimestamp(const qulonglong timestamp);
    [[nodiscard]] qulonglong getLastSentReportTimestamp() const;
    [[nodiscard]] QVariantMap prepareReport() const;
    void reportToServerSentSuccessfully();
    [[nodiscard]] QString makeDbPath() const;

private slots:
    void sendReportToServer();

private:
    static QByteArray statusStringFromNumber(const Status status);
    static QString classifyStatus(const Status status);

    static int clientStatusReportingTrySendTimerInterval;
    static int repordSendIntervalMs;

    static QString dbPathForTesting;

    Account *_account = nullptr;
    QHash<int, QPair<QByteArray, qint64>> _statusNamesAndHashes;
    QSqlDatabase _database;
    bool _isInitialized = false;
    QTimer _clientStatusReportingSendTimer;
    mutable QRecursiveMutex _mutex;

    friend class Account;
    friend class TestClientStatusReporting;
};
}
