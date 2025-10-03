/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include <functional>

#include <QWebSocketServer>
#include <QWebSocket>
#include <QSignalSpy>

#include "creds/abstractcredentials.h"
#include "account.h"

class FakeWebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit FakeWebSocketServer(quint16 port = 12345, QObject *parent = nullptr);

    ~FakeWebSocketServer();

    QWebSocket *authenticateAccount(
        const OCC::AccountPtr account, std::function<void(OCC::PushNotifications *pushNotifications)> beforeAuthentication = [](OCC::PushNotifications *) {}, std::function<void(void)> afterAuthentication = [] {});

    void close();

    bool waitForTextMessages() const;

    uint32_t textMessagesCount() const;

    QString textMessage(uint32_t messageNumber) const;

    QWebSocket *socketForTextMessage(uint32_t messageNumber) const;

    void clearTextMessages();

    static OCC::AccountPtr createAccount(const QString &username = "user", const QString &password = "password");

signals:
    void closed();
    void processTextMessage(QWebSocket *sender, const QString &message);

private slots:
    void processTextMessageInternal(const QString &message);
    void onNewConnection();
    void socketDisconnected();

private:
    QWebSocketServer *_webSocketServer;
    QList<QWebSocket *> _clients;

    std::unique_ptr<QSignalSpy> _processTextMessageSpy;
};

class CredentialsStub : public OCC::AbstractCredentials
{
    Q_OBJECT

public:
    CredentialsStub(const QString &user, const QString &password);
    virtual QString authType() const;
    virtual QString user() const;
    virtual QString password() const;
    virtual QNetworkAccessManager *createQNAM() const;
    virtual bool ready() const;
    virtual void fetchFromKeychain();
    virtual void askFromUser();

    virtual bool stillValid(QNetworkReply *reply);
    virtual void persist();
    virtual void invalidateToken();
    virtual void forgetSensitiveData();

private:
    QString _user;
    QString _password;
};
