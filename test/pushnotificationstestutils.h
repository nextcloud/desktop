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

    ~FakeWebSocketServer() override;

    QWebSocket *authenticateAccount(
        const OCC::AccountPtr account, std::function<void(OCC::PushNotifications *pushNotifications)> beforeAuthentication = [](OCC::PushNotifications *) {}, std::function<void(void)> afterAuthentication = [] {});

    void close();

    [[nodiscard]] bool waitForTextMessages() const;

    [[nodiscard]] uint32_t textMessagesCount() const;

    [[nodiscard]] QString textMessage(int messageNumber) const;

    [[nodiscard]] QWebSocket *socketForTextMessage(int messageNumber) const;

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
    [[nodiscard]] QString authType() const override;
    [[nodiscard]] QString user() const override;
    [[nodiscard]] QString password() const override;
    [[nodiscard]] QNetworkAccessManager *createQNAM() const override;
    [[nodiscard]] bool ready() const override;
    void fetchFromKeychain() override;
    void askFromUser() override;

    bool stillValid(QNetworkReply *reply) override;
    void persist() override;
    void invalidateToken() override;
    void forgetSensitiveData() override;

private:
    QString _user;
    QString _password;
};
