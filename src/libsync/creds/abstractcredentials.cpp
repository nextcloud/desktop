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

#include <QString>
#include <QDebug>

#include "asserts.h"
#include "creds/abstractcredentials.h"

namespace OCC
{

AbstractCredentials::AbstractCredentials()
    : _account(0)
{
}

void AbstractCredentials::setAccount(Account *account)
{
    ENFORCE(!_account, "should only setAccount once");
    _account = account;
}

QString AbstractCredentials::keychainKey(const QString &url, const QString &user)
{
    QString u(url);
    if( u.isEmpty() ) {
        qDebug() << "Empty url in keyChain, error!";
        return QString::null;
    }
    if( user.isEmpty() ) {
        qDebug() << "Error: User is empty!";
        return QString::null;
    }

    if( !u.endsWith(QChar('/')) ) {
        u.append(QChar('/'));
    }

    QString key = user+QLatin1Char(':')+u;
    return key;
}
} // namespace OCC
