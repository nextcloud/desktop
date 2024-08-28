/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "libsync/creds/oauth.h"

#include <QtQmlIntegration/QtQmlIntegration>

namespace OCC {

class QmlCredentials : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl host MEMBER _host CONSTANT FINAL)
    Q_PROPERTY(QString displayName MEMBER _displayName CONSTANT FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged FINAL)
    Q_PROPERTY(bool isRefresh READ isRefresh WRITE setIsRefresh NOTIFY isRefreshChanged FINAL)
    QML_ELEMENT
    QML_UNCREATABLE("Abstract class")

public:
    QmlCredentials(const QUrl &host, const QString &displayName, QObject *parent = nullptr);

    virtual bool isReady() const = 0;


    /**
     * Whether we refresh the credentials or are in the wizard
     * @default true
     */
    bool isRefresh() const;
    void setIsRefresh(bool newIsRefresh);

Q_SIGNALS:
    void readyChanged() const;
    void logOutRequested() const;

    void isRefreshChanged();

private:
    QUrl _host;
    QString _displayName;
    bool _isRefresh = true;
};


class QmlOAuthCredentials : public QmlCredentials
{
    Q_OBJECT
    Q_PROPERTY(bool isValid READ isValid NOTIFY readyChanged FINAL)
    QML_ELEMENT
    QML_UNCREATABLE("C++ only")

public:
    QmlOAuthCredentials(OAuth *oauth, const QUrl &host, const QString &displayName, QObject *parent = nullptr);

    Q_INVOKABLE void copyAuthenticationUrlToClipboard();
    Q_INVOKABLE void openAuthenticationUrlInBrowser();

    bool isReady() const override;

    bool isValid() const;

Q_SIGNALS:
    void requestRestart();


private:
    QPointer<OAuth> _oauth = nullptr;
    bool _ready = false;
};

class QmlBasicCredentials : public QmlCredentials
{
    Q_OBJECT
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(bool isReadOnlyName READ isReadOnlyName NOTIFY userNameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString userNameLabel READ userNameLabel CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("C++ only")

public:
    using QmlCredentials::QmlCredentials;

    QString userNameLabel() const;

    QString userName() const;
    void setUserName(const QString &userName);

    QString password() const;
    void setPassword(const QString &password);

    void setReadOnlyName(const QString &userName);
    bool isReadOnlyName() const;

    bool isReady() const override;
Q_SIGNALS:
    void userNameChanged() const;
    void passwordChanged() const;
    void loginRequested() const;

private:
    bool _readOnlyName = false;
    QString _userName;
    QString _password;
};
}
