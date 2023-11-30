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

#include <QtGlobal>
#include <QByteArray>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QTimer>

namespace OCC {

class Account;
class ClientStatusReportingDatabase;
struct ClientStatusReportingRecord;

class OWNCLOUDSYNC_EXPORT ClientStatusReportingNetwork : public QObject
{
    Q_OBJECT
public:
    explicit ClientStatusReportingNetwork(Account *account, const QSharedPointer<ClientStatusReportingDatabase> database, QObject *parent = nullptr);
    ~ClientStatusReportingNetwork() override;

private:
    void init();

    [[nodiscard]] QVariantMap prepareReport() const;
    void reportToServerSentSuccessfully();

private slots:
    void sendReportToServer();

public:
    [[nodiscard]] bool isInitialized() const;

    static QByteArray classifyStatus(const ClientStatusReportingStatus status);

    static int clientStatusReportingTrySendTimerInterval;
    static quint64 repordSendIntervalMs;
    // this must be set in unit tests on init
    static QString dbPathForTesting;

private:
    Account *_account = nullptr;

    QSharedPointer<ClientStatusReportingDatabase> _database;

    bool _isInitialized = false;

    QTimer _clientStatusReportingSendTimer;
};
}
