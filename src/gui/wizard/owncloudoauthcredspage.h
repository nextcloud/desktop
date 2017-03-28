/*
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

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "wizard/abstractcredswizardpage.h"
#include "accountfwd.h"
#include "creds/oauth.h"

namespace OCC {


class OwncloudOAuthCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    OwncloudOAuthCredsPage();

    AbstractCredentials *getCredentials() const Q_DECL_OVERRIDE;

    void initializePage() Q_DECL_OVERRIDE;
    int nextId() const Q_DECL_OVERRIDE;
    void setConnected();

public Q_SLOTS:
    void setVisible(bool visible) Q_DECL_OVERRIDE;
    void asyncAuthResult(OAuth::Result, const QString &user, const QString &token,
        const QString &reniewToken);

signals:
    void connectToOCUrl(const QString &);

private:
    bool _afterInitialSetup;

public:
    QString _user;
    QString _token;
    QString _refreshToken;
    QScopedPointer<OAuth> _asyncAuth;
};

} // namespace OCC
