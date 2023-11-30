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

#include <QtGlobal>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QRecursiveMutex>
#include <QString>
#include <QTimer>
#include <QtSql>

namespace OCC {

class Account;
struct ClientStatusReportingRecord;

class OWNCLOUDSYNC_EXPORT ClientStatusReporting : public QObject
{
    Q_OBJECT
public:
    enum Status {
        DownloadError_Cannot_Create_File = 0,
        DownloadError_Conflict,
        DownloadError_ConflictCaseClash,
        DownloadError_ConflictInvalidCharacters,
        DownloadError_No_Free_Space,
        DownloadError_ServerError,
        DownloadError_Virtual_File_Hydration_Failure,
        E2EeError_GeneralError,
        UploadError_Conflict,
        UploadError_ConflictInvalidCharacters,
        UploadError_No_Free_Space,
        UploadError_No_Write_Permissions,
        UploadError_ServerError,
        UploadError_Virus_Detected,
        Count,
    };

    explicit ClientStatusReporting(Account *account, QObject *parent = nullptr);
    ~ClientStatusReporting() override;

    static QByteArray statusStringFromNumber(const Status status);

private:
    void init();
    // reporting must happen via Account
    void reportClientStatus(const Status status) const;

    [[nodiscard]] Result<void, QString> setClientStatusReportingRecord(const ClientStatusReportingRecord &record) const;
    [[nodiscard]] QVector<ClientStatusReportingRecord> getClientStatusReportingRecords() const;
    void deleteClientStatusReportingRecords() const;

    void setLastSentReportTimestamp(const quint64 timestamp) const;
    [[nodiscard]] quint64 getLastSentReportTimestamp() const;

    void setStatusNamesHash(const QByteArray &hash) const;
    [[nodiscard]] QByteArray getStatusNamesHash() const;

    [[nodiscard]] QVariantMap prepareReport() const;
    void reportToServerSentSuccessfully();

    [[nodiscard]] QString makeDbPath() const;

private slots:
    void sendReportToServer();

private:
    static QByteArray classifyStatus(const Status status);

public:
    static int clientStatusReportingTrySendTimerInterval;
    static quint64 repordSendIntervalMs;
    // this must be set in unit tests on init
    static QString dbPathForTesting;

private:

    Account *_account = nullptr;

    QSqlDatabase _database;

    bool _isInitialized = false;

    QTimer _clientStatusReportingSendTimer;

    QHash<int, QPair<QByteArray, quint64>> _statusNamesAndHashes;

    // inspired by SyncJournalDb
    mutable QRecursiveMutex _mutex;

    friend class Account;
};
}
