/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef QUOTAINFO_H
#define QUOTAINFO_H

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QTimer>
#include <QDateTime>

#include "accountstate.h"

namespace OCC {
class PropfindJob;

/**
 * @brief handles getting the quota to display in the UI
 *
 * It is typically owned by the AccountSetting page.
 *
 * The quota is requested if these 3 conditions are met:
 *  - This object is active via setActive() (typically if the settings page is visible.)
 *  - The account is connected.
 *  - Every 30 seconds (defaultIntervalT) or 5 seconds in case of failure (failIntervalT)
 *
 * We only request the quota when the UI is visible otherwise this might slow down the server with
 * too many requests. But we still need to do it every 30 seconds otherwise user complains that the
 * quota is not updated fast enough when changed on the server.
 *
 * If the quota job is not finished within 30 seconds, it is cancelled and another one is started
 *
 * @ingroup gui
 */
class QuotaInfo : public QObject
{
    Q_OBJECT
public:
    explicit QuotaInfo(const AccountStatePtr &accountState, QObject *parent = nullptr);

    qint64 lastQuotaTotalBytes() const { return _lastQuotaTotalBytes; }
    qint64 lastQuotaUsedBytes() const { return _lastQuotaUsedBytes; }

    /**
     * When the quotainfo is active, it requests the quota at regular interval.
     * When setting it to active it will request the quota immediately if the last time
     * the quota was requested was more than the interval
     */
    void setActive(bool active);

public Q_SLOTS:
    void slotCheckQuota();

private Q_SLOTS:
    void slotUpdateLastQuota(const QMap<QString, QString> &);
    void slotAccountStateChanged();
    void slotRequestFailed();

Q_SIGNALS:
    void quotaUpdated(qint64 total, qint64 used);

private:
    bool canGetQuota() const;

    /// Returns the folder that quota shall be retrieved for
    QString quotaBaseFolder() const;

    AccountStatePtr _accountState;
    qint64 _lastQuotaTotalBytes;
    qint64 _lastQuotaUsedBytes;
    QTimer _jobRestartTimer;
    QDateTime _lastQuotaRecieved; // the time at which the quota was received last
    bool _active; // if we should check at regular interval (when the UI is visible)
    QPointer<PropfindJob> _job; // the currently running job
};


} // namespace OCC

#endif //QUOTAINFO_H
