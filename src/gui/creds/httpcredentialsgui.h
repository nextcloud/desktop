/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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
#include "creds/httpcredentials.h"
#include "creds/oauth.h"
#include <QPointer>
#include <QTcpServer>

namespace OCC {

/**
 * @brief The HttpCredentialsGui class
 * @ingroup gui
 */
class HttpCredentialsGui : public HttpCredentials
{
    Q_OBJECT
public:
    explicit HttpCredentialsGui()
        : HttpCredentials()
    {
    }
    HttpCredentialsGui(const QString &user, const QString &password, const QSslCertificate &certificate, const QSslKey &key)
        : HttpCredentials(user, password, certificate, key)
    {
    }
    HttpCredentialsGui(const QString &user, const QString &password, const QString &refreshToken,
        const QSslCertificate &certificate, const QSslKey &key)
        : HttpCredentials(user, password, certificate, key)
    {
        _refreshToken = refreshToken;
    }

    /**
     * This will query the server and either uses OAuth via _asyncAuth->start()
     * or call showDialog to ask the password
     */
    void askFromUser() Q_DECL_OVERRIDE;
    /**
     * In case of oauth, return an URL to the link to open the browser.
     * An invalid URL otherwise
     */
    QUrl authorisationLink() const { return _asyncAuth ? _asyncAuth->authorisationLink() : QUrl(); }


    static QString requestAppPasswordText(const Account *account);
private slots:
    void asyncAuthResult(OAuth::Result, const QString &user, const QString &accessToken, const QString &refreshToken);
    void showDialog();

signals:
    void authorisationLinkChanged();

private:
    Q_INVOKABLE void askFromUserAsync();

    QScopedPointer<OAuth, QScopedPointerObjectDeleteLater<OAuth>> _asyncAuth;
};

} // namespace OCC
