/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"
#include <common/result.h>
#include "clientstatusreportingcommon.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <QTimer>
#include <QtGlobal>

namespace OCC {

class Account;
class JsonApiJob;
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
    void reportToServerSentSuccessfully(const QVariantMap &report, bool hasQueuedRecords);

private Q_SLOTS:
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
    QPointer<JsonApiJob> _reportJob;

    QSharedPointer<ClientStatusReportingDatabase> _database;

    bool _isInitialized = false;

    QTimer _clientStatusReportingSendTimer;
};
}
