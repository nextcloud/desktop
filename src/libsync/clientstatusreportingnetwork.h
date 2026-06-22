/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
