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
 * Job that does the authorization grant and fetch the access token
 *
 * Normal workflow:
 *
 *   --> start()
 *       |
 *       +----> openBrowser() open the browser to the login page, redirects to http://localhost:xxx
 *       |
 *       +----> _server starts listening on a TCP port waiting for an HTTP request with a 'code'
 *                |
 *                v
 *             request the access_token and the refresh_token via 'apps/oauth2/api/v1/token'
 *                |
 *                v
 *              emit result(...)
 *
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
    bool openBrowser();
    QUrl authorisationLink() const;

signals:
    /**
     * The state has changed.
     * when logged in, token has the value of the token.
     */
    void result(Flow2Auth::Result result, const QString &user = QString(), const QString &token = QString(), const QString &refreshToken = QString());

private slots:
    void slotPollTimerTimeout();

private:
    Account *_account;
    QUrl _loginUrl;
    QString _pollToken;
    QString _pollEndpoint;
    QTimer _pollTimer;

public:
    QString _expectedUser;
};


} // namespace OCC
