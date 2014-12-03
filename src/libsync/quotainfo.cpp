/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "quotainfo.h"
#include "account.h"
#include "networkjobs.h"
#include "creds/abstractcredentials.h"

#include <QTimer>
#include <QDebug>

namespace OCC {

namespace {
static const int defaultIntervalT = 30*1000;
static const int failIntervalT = 5*1000;
static const int initialTimeT = 1*1000;
}

QuotaInfo::QuotaInfo(Account *account)
    : QObject(account)
    , _account(account)
    , _lastQuotaTotalBytes(0)
    , _lastQuotaUsedBytes(0)
    , _jobRestartTimer(new QTimer(this))
{
    connect(_account, SIGNAL(stateChanged(int)), SLOT(slotAccountStateChanged(int)));
    connect(_jobRestartTimer, SIGNAL(timeout()), SLOT(slotCheckQuota()));
    _jobRestartTimer->setSingleShot(true);
    _jobRestartTimer->start(initialTimeT);
}

void QuotaInfo::slotAccountChanged(Account *newAccount, Account *oldAccount)
{
    _account = newAccount;
    disconnect(oldAccount, SIGNAL(stateChanged(int)), this, SLOT(slotAccountStateChanged(int)));
    connect(newAccount, SIGNAL(stateChanged(int)), this, SLOT(slotAccountStateChanged(int)));
}

void QuotaInfo::slotAccountStateChanged(int state)
{
    switch (state) {
    case Account::SignedOut: // fall through
    case Account::InvalidCredential:
    case Account::Disconnected:
        _jobRestartTimer->stop();
        break;
    case Account::Connected: // fall through
        slotCheckQuota();
    }
}

void QuotaInfo::slotRequestFailed()
{
    _lastQuotaTotalBytes = 0;
    _lastQuotaUsedBytes = 0;
    _jobRestartTimer->start(failIntervalT);
}

void QuotaInfo::slotCheckQuota()
{
    if (!_account.isNull() && _account->state() == Account::Connected
            && _account->credentials() && _account->credentials()->ready()) {
        CheckQuotaJob *job = new CheckQuotaJob(_account, "/", this);
        connect(job, SIGNAL(quotaRetrieved(qint64,qint64)), SLOT(slotUpdateLastQuota(qint64,qint64)));
        connect(job, SIGNAL(networkError(QNetworkReply*)), SLOT(slotRequestFailed()));
        job->start();
    }
}

void QuotaInfo::slotUpdateLastQuota(qint64 total, qint64 used)
{
    _lastQuotaTotalBytes = total;
    _lastQuotaUsedBytes = used;
    emit quotaUpdated(total, used);
    _jobRestartTimer->start(defaultIntervalT);
}

}
