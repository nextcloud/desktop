/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H
#define MIRALL_CREDS_SHIBBOLETH_CREDENTIALS_H

#include <QNetworkCookie>

#include "mirall/creds/abstractcredentials.h"

namespace Mirall
{

class ShibbolethWebView;

class ShibbolethCredentials : public AbstractCredentials
{
Q_OBJECT

public:
    ShibbolethCredentials();
    ShibbolethCredentials(const QNetworkCookie& cookie);

    void syncContextPreInit(CSYNC* ctx);
    void syncContextPreStart(CSYNC* ctx);
    bool changed(AbstractCredentials* credentials) const;
    QString authType() const;
    QNetworkAccessManager* getQNAM() const;
    bool ready() const;
    void fetch();
    void persistForUrl(const QString& url);

    QNetworkCookie cookie() const;

private Q_SLOTS:
    void onShibbolethCookieReceived(const QNetworkCookie& cookie);

private:
    QNetworkCookie _shibCookie;
    bool _ready;
    ShibbolethWebView* _browser;
};

} // ns Mirall

#endif
