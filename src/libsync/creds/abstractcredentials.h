/*
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

    virtual bool changed(AbstractCredentials* credentials) const = 0;
    virtual QString authType() const = 0;
    virtual QString user() const = 0;
    virtual QNetworkAccessManager* getQNAM() const = 0;
    virtual bool ready() const = 0;
    virtual void fetchFromKeychain() = 0;
    virtual void askFromUser() = 0;
    virtual bool stillValid(QNetworkReply *reply) = 0;
    virtual void persist() = 0;

    /** Invalidates token used to authorize requests, it will no longer be used.
     *
     * For http auth, this would be the session cookie.
     *
     * Note that sensitive data (like the password used to acquire the
     * session cookie) may be retained. See forgetSensitiveData().
     */
    virtual void invalidateToken() = 0;

    /** Clears out all sensitive data; used for fully signing out users.
     *
     * This should always imply invalidateToken() but may go beyond it.
     *
     * For http auth, this would clear the session cookie and password.
     */
    virtual void forgetSensitiveData() = 0;

    static QString keychainKey(const QString &url, const QString &user);

Q_SIGNALS:
    void fetched();
    void asked();

protected:
    Account* _account;
};

} // namespace OCC

#endif
