/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
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

#ifndef USERINFO_H
#define USERINFO_H

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QTimer>
#include <QDateTime>

namespace OCC {
class AccountState;
class JsonApiJob;

/**
 * @brief handles getting the user info and quota to display in the UI
 *
 * It is typically owned by the AccountSetting page.
 *
 * The user info and quota is requested if these 3 conditions are met:
 *  - This object is active via setActive() (typically if the settings page is visible.)
 *  - The account is connected.
 *  - Every 30 seconds (defaultIntervalT) or 5 seconds in case of failure (failIntervalT)
 *
 * We only request the info when the UI is visible otherwise this might slow down the server with
 * too many requests. But we still need to do it every 30 seconds otherwise user complains that the
 * quota is not updated fast enough when changed on the server.
 *
 * If the fetch job is not finished within 30 seconds, it is cancelled and another one is started
 *
 * Constructor notes:
 *  - allowDisconnectedAccountState: set to true if you want to ignore AccountState's isConnected() state,
 *    this is used by ConnectionValidator (prior having a valid AccountState).
 *  - fetchAvatarImage: set to false if you don't want to fetch the avatar image
 *
 * @ingroup gui
 *
 * Here follows the state machine

 \code{.unparsed}
 *---> slotFetchInfo
         JsonApiJob (ocs/v1.php/cloud/user)
         |
         +-> slotUpdateLastInfo
               AvatarJob (if _fetchAvatarImage is true)
               |
               +-> slotAvatarImage -->
   +-----------------------------------+
   |
   +-> Client Side Encryption Checks --+ --reportResult()
     \endcode
  */
class UserInfo : public QObject
{
    Q_OBJECT
public:
    explicit UserInfo(OCC::AccountState *accountState, bool allowDisconnectedAccountState, bool fetchAvatarImage, QObject *parent = nullptr);

    [[nodiscard]] qint64 lastQuotaTotalBytes() const { return _lastQuotaTotalBytes; }
    [[nodiscard]] qint64 lastQuotaUsedBytes() const { return _lastQuotaUsedBytes; }

    /**
     * When the quotainfo is active, it requests the quota at regular interval.
     * When setting it to active it will request the quota immediately if the last time
     * the quota was requested was more than the interval
     */
    void setActive(bool active);

public Q_SLOTS:
    void slotFetchInfo();

private Q_SLOTS:
    void slotUpdateLastInfo(const QJsonDocument &json);
    void slotAccountStateChanged();
    void slotRequestFailed();
    void slotAvatarImage(const QImage &img);

Q_SIGNALS:
    void quotaUpdated(qint64 total, qint64 used);
    void fetchedLastInfo(OCC::UserInfo *userInfo);

private:
    [[nodiscard]] bool canGetInfo() const;

    QPointer<AccountState> _accountState;
    bool _allowDisconnectedAccountState;
    bool _fetchAvatarImage;

    qint64 _lastQuotaTotalBytes = 0;
    qint64 _lastQuotaUsedBytes = 0;
    QTimer _jobRestartTimer;
    QDateTime _lastInfoReceived; // the time at which the user info and quota was received last
    bool _active = false; // if we should check at regular interval (when the UI is visible)
    QPointer<JsonApiJob> _job; // the currently running job
};


} // namespace OCC

#endif //USERINFO_H
