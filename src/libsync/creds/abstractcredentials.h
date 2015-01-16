/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef MIRALL_CREDS_ABSTRACT_CREDENTIALS_H
#define MIRALL_CREDS_ABSTRACT_CREDENTIALS_H

#include <QObject>

#include <csync.h>
#include "owncloudlib.h"
#include "accountfwd.h"

class QNetworkAccessManager;
class QNetworkReply;
namespace OCC
{

class OWNCLOUDSYNC_EXPORT AbstractCredentials : public QObject
{
    Q_OBJECT

public:
    AbstractCredentials();
    // No need for virtual destructor - QObject already has one.

    /** The bound account for the credentials instance.
     *
     * Credentials are always used in conjunction with an account.
     * Calling Account::setCredentials() will call this function.
     * Credentials only live as long as the underlying account object.
     */
    virtual void setAccount(Account* account);

    virtual void syncContextPreInit(CSYNC* ctx) = 0;
    virtual void syncContextPreStart(CSYNC* ctx) = 0;
    virtual bool changed(AbstractCredentials* credentials) const = 0;
    virtual QString authType() const = 0;
    virtual QString user() const = 0;
    virtual QNetworkAccessManager* getQNAM() const = 0;
    virtual bool ready() const = 0;
    virtual void fetch() = 0;
    virtual bool stillValid(QNetworkReply *reply) = 0;
    virtual void persist() = 0;
    /** Invalidates auth token, or password for basic auth */
    virtual void invalidateToken() = 0;
    virtual void invalidateAndFetch() {
        invalidateToken();
        fetch();
    }


    static QString keychainKey(const QString &url, const QString &user);

Q_SIGNALS:
    void fetched();

protected:
    Account* _account;
};

} // namespace OCC

#endif
