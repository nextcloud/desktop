/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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
#include <QPointer>
#include <QUrl>
#include <QTimer>
#include "accountfwd.h"

namespace OCC {

/**
 * Job that does the authorization, grants and fetches the access token via Login Flow v2
 *
 * See: https://docs.nextcloud.com/server/latest/developer_manual/client_apis/LoginFlow/index.html#login-flow-v2
 */
class Flow2Auth : public QObject
{
    Q_OBJECT
public:
    Flow2Auth(Account *account, QObject *parent)
            : QObject(parent)
            , _account(account)
    {
    }
    ~Flow2Auth();

    enum Result { NotSupported,
        LoggedIn,
        Error };
    Q_ENUM(Result);
    void start();
    void openBrowser();
    QUrl authorisationLink() const;

signals:
    /**
     * The state has changed.
     * when logged in, appPassword has the value of the app password.
     */
    void result(Flow2Auth::Result result, const QString &user = QString(), const QString &appPassword = QString());

    void statusChanged(int secondsLeft);

public slots:
    void slotPollNow();

private slots:
    void slotPollTimerTimeout();

private:
    Account *_account;
    QUrl _loginUrl;
    QString _pollToken;
    QString _pollEndpoint;
    QTimer _pollTimer;
    int _secondsLeft;
    int _secondsInterval;
};


} // namespace OCC
