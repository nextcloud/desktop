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

#include "mirall/quotainfo.h"
#include "mirall/account.h"
#include "mirall/networkjobs.h"
#include "creds/abstractcredentials.h"

#include <QTimer>

namespace Mirall {

namespace {
static const int defaultIntervalT = 30*1000;
static const int failIntervalT = 5*1000;
static const int initialTimeT = 1*1000;
}

QuotaInfo::QuotaInfo(QObject *parent)
    : QObject(parent)
    , _lastQuotaTotalBytes(0)
    , _lastQuotaUsedBytes(0)
    , _refreshTimer(new QTimer(this))
{
    connect(AccountManager::instance(), SIGNAL(accountChanged(Account*,Account*)),
            SLOT(slotAccountChanged(Account*,Account*)));
    connect(_refreshTimer, SIGNAL(timeout()), SLOT(slotCheckQuota()));
    _refreshTimer->setSingleShot(true);
    _refreshTimer->start(initialTimeT);
}

void QuotaInfo::slotAccountChanged(Account *newAccount, Account *oldAccount)
{
    Q_UNUSED(oldAccount);
    _account = newAccount;
}

void QuotaInfo::slotCheckQuota()
{
    if (!_account.isNull() && _account->credentials() && _account->credentials()->ready()) {
        CheckQuotaJob *job = new CheckQuotaJob(_account, "/", this);
        connect(job, SIGNAL(quotaRetrieved(qint64,qint64)), SLOT(slotUpdateLastQuota(qint64,qint64)));
        job->start();
        _refreshTimer->start(defaultIntervalT);
    } else {
        _lastQuotaTotalBytes = 0;
        _lastQuotaUsedBytes = 0;
        _refreshTimer->start(failIntervalT);
    }
}

void QuotaInfo::slotUpdateLastQuota(qint64 total, qint64 used)
{
    _lastQuotaTotalBytes = total;
    _lastQuotaUsedBytes = used;
    emit quotaUpdated(total, used);
}

}
