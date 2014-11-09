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

#ifndef QUOTAINFO_H
#define QUOTAINFO_H

#include <QObject>
#include <QPointer>

class QTimer;

namespace OCC {

class Account;

class QuotaInfo : public QObject {
    Q_OBJECT
public:
    QuotaInfo(Account *account);

    qint64 lastQuotaTotalBytes() const { return _lastQuotaTotalBytes; }
    qint64 lastQuotaUsedBytes() const { return _lastQuotaUsedBytes; }

public Q_SLOTS:
    void slotUpdateLastQuota(qint64 total, qint64 used);
    void slotCheckQuota();

private Q_SLOTS:
    void slotAccountChanged(Account *newAccount, Account *oldAccount);
    void slotAccountStateChanged(int state);
    void slotRequestFailed();

Q_SIGNALS:
    void quotaUpdated(qint64 total, qint64 used);

private:
    QPointer<Account> _account;
    qint64 _lastQuotaTotalBytes;
    qint64 _lastQuotaUsedBytes;
    QTimer *_jobRestartTimer;
};



} // namespace OCC

#endif //QUOTAINFO_H
