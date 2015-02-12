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
#include "accountstate.h"
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

QuotaInfo::QuotaInfo(AccountState *accountState)
    : QObject(accountState)
    , _accountState(accountState)
    , _lastQuotaTotalBytes(0)
    , _lastQuotaUsedBytes(0)
    , _jobRestartTimer(new QTimer(this))
{
    connect(accountState, SIGNAL(stateChanged(int)),
            SLOT(slotAccountStateChanged(int)));
    connect(_jobRestartTimer, SIGNAL(timeout()), SLOT(slotCheckQuota()));
    _jobRestartTimer->setSingleShot(true);
    if (canGetQuota()) {
        _jobRestartTimer->start(initialTimeT);
    }
}

void QuotaInfo::slotAccountStateChanged(int /*state*/)
{
    if (canGetQuota()) {
        _jobRestartTimer->start(initialTimeT);
    } else {
        _jobRestartTimer->stop();
    }
}

void QuotaInfo::slotRequestFailed()
{
    _lastQuotaTotalBytes = 0;
    _lastQuotaUsedBytes = 0;
    _jobRestartTimer->start(failIntervalT);
}

bool QuotaInfo::canGetQuota() const
{
    if (! _accountState) {
        return false;
    }
    AccountPtr account = _accountState->account();
    return _accountState->isConnected()
        && account->credentials()
        && account->credentials()->ready();
}

void QuotaInfo::slotCheckQuota()
{
    if (! canGetQuota()) {
        return;
    }

    AccountPtr account = _accountState->account();
    CheckQuotaJob *job = new CheckQuotaJob(account, "/", this);
    connect(job, SIGNAL(quotaRetrieved(qint64,qint64)), SLOT(slotUpdateLastQuota(qint64,qint64)));
    connect(job, SIGNAL(networkError(QNetworkReply*)), SLOT(slotRequestFailed()));
    job->start();
}

void QuotaInfo::slotUpdateLastQuota(qint64 total, qint64 used)
{
    _lastQuotaTotalBytes = total;
    _lastQuotaUsedBytes = used;
    emit quotaUpdated(total, used);
    _jobRestartTimer->start(defaultIntervalT);
}

}
