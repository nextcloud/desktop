/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_CREDS_ABSTRACT_CREDENTIALS_H
#define MIRALL_CREDS_ABSTRACT_CREDENTIALS_H

#include <QObject>
#include <QNetworkRequest>

#include <csync.h>
#include "owncloudlib.h"
#include "accountfwd.h"

class QNetworkAccessManager;
class QNetworkReply;
namespace OCC {

class AbstractNetworkJob;

class OWNCLOUDSYNC_EXPORT AbstractCredentials : public QObject
{
    Q_OBJECT

public:
    /// Don't add credentials if this is set on a QNetworkRequest
    static constexpr QNetworkRequest::Attribute DontAddCredentialsAttribute = QNetworkRequest::User;

    AbstractCredentials();
    // No need for virtual destructor - QObject already has one.

    /** The bound account for the credentials instance.
     *
     * Credentials are always used in conjunction with an account.
     * Calling Account::setCredentials() will call this function.
     * Credentials only live as long as the underlying account object.
     */
    virtual void setAccount(Account *account);

    [[nodiscard]] virtual QString authType() const = 0;
    [[nodiscard]] virtual QString user() const = 0;
    [[nodiscard]] virtual QString password() const = 0;
    [[nodiscard]] virtual QNetworkAccessManager *createQNAM() const = 0;

    /** Whether there are credentials that can be used for a connection attempt. */
    [[nodiscard]] virtual bool ready() const = 0;

    /** Whether fetchFromKeychain() was called before. */
    [[nodiscard]] bool wasFetched() const { return _wasFetched; }

    /** Trigger (async) fetching of credential information using the appplication name
     *
     * Should set _wasFetched = true, and later emit fetched() when done.
     */
    virtual void fetchFromKeychain(const QString &appName = {}) = 0;

    /** Ask credentials from the user (typically async)
     *
     * Should emit asked() when done.
     */
    virtual void askFromUser() = 0;

    virtual bool stillValid(QNetworkReply *reply) = 0;
    virtual void persist() = 0;

    /** Invalidates token used to authorize requests, it will no longer be used.
     *
     * For http auth, this would be the session cookie.
     *
     * Note that sensitive data (like the password used to acquire the
     * session cookie) may be retained. See forgetSensitiveData().
     *
     * ready() must return false afterwards.
     */
    virtual void invalidateToken() = 0;

    /** Clears out all sensitive data; used for fully signing out users.
     *
     * This should always imply invalidateToken() but may go beyond it.
     *
     * For http auth, this would clear the session cookie and password.
     */
    virtual void forgetSensitiveData() = 0;

    static QString keychainKey(const QString &url, const QString &user, const QString &accountId, const QString &appName = {});

    /** If the job need to be restarted or queue, this does it and returns true. */
    virtual bool retryIfNeeded(AbstractNetworkJob *) { return false; }

Q_SIGNALS:
    /** Emitted when fetchFromKeychain() is done.
     *
     * Note that ready() can be true or false, depending on whether there was useful
     * data in the keychain.
     */
    void fetched();

    /** Emitted when askFromUser() is done.
     *
     * Note that ready() can be true or false, depending on whether the user provided
     * data or not.
     */
    void asked();

protected:
    Account *_account = nullptr;
    bool _wasFetched = false;
};

} // namespace OCC

#endif
