/*
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
#include "creds/httpcredentials.h"
#include "creds/httpcredentials_p.h"

#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>
#include <QSettings>
#include <QSslKey>
#include <QJsonObject>
#include <QJsonDocument>
#include <QBuffer>

#include "account.h"
#include "accessmanager.h"
#include "configfile.h"
#include "theme.h"
#include "syncengine.h"
#include "oauth.h"
#include "creds/credentialscommon.h"
#include "creds/credentialmanager.h"
#include <QAuthenticator>

Q_LOGGING_CATEGORY(lcHttpCredentials, "sync.credentials.http", QtInfoMsg)
Q_LOGGING_CATEGORY(lcHttpLegacyCredentials, "sync.credentials.http.legacy", QtInfoMsg)

namespace {
constexpr int CredentialVersion = 1;
const char authenticationFailedC[] = "owncloud-authentication-failed";

auto isOAuthC()
{
    return QStringLiteral("oauth");
}

auto passwordKeyC()
{
    return QStringLiteral("http/password");
}

auto refreshTokenKeyC()
{
    return QStringLiteral("http/oauthtoken");
}

auto clientCertBundleKeyC()
{
    return QStringLiteral("clientsideCert/bundle");
}

auto clientCertPasswordKeyC()
{
    return QStringLiteral("clientsideCert/password");
}

auto CredentialVersionKey()
{
    return QStringLiteral("CredentialVersion");
}

const QString userC()
{
    return QStringLiteral("user");
}
}

namespace OCC {

class HttpCredentialsAccessManager : public AccessManager
{
    Q_OBJECT
public:
    HttpCredentialsAccessManager(const HttpCredentials *cred, QObject *parent = nullptr)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        QNetworkRequest req(request);
        if (!req.attribute(HttpCredentials::DontAddCredentialsAttribute).toBool()) {
            if (_cred && !_cred->_password.isEmpty()) {
                if (_cred->isUsingOAuth()) {
                    req.setRawHeader("Authorization", "Bearer " + _cred->_password.toUtf8());
                } else {
                    QByteArray credHash = QByteArray(_cred->user().toUtf8() + ":" + _cred->_password.toUtf8()).toBase64();
                    req.setRawHeader("Authorization", "Basic " + credHash);
                }
            } else if (!request.url().password().isEmpty()) {
                // Typically the requests to get or refresh the OAuth access token. The client
                // credentials are put in the URL from the code making the request.
                QByteArray credHash = request.url().userInfo().toUtf8().toBase64();
                req.setRawHeader("Authorization", "Basic " + credHash);
            }
        }

        if (_cred && !_cred->_clientSslKey.isNull() && !_cred->_clientSslCertificate.isNull()) {
            // SSL configuration
            QSslConfiguration sslConfiguration = req.sslConfiguration();
            sslConfiguration.setLocalCertificate(_cred->_clientSslCertificate);
            sslConfiguration.setPrivateKey(_cred->_clientSslKey);
            req.setSslConfiguration(sslConfiguration);
        }

        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const HttpCredentials> _cred;
};

HttpCredentials::HttpCredentials(DetermineAuthTypeJob::AuthType authType, const QString &user, const QString &password, const QByteArray &clientCertBundle, const QByteArray &clientCertPassword)
    : _user(user)
    , _password(password)
    , _ready(true)
    , _clientCertBundle(clientCertBundle)
    , _clientCertPassword(clientCertPassword)
    , _retryOnKeyChainError(false)
    , _authType(authType)
{
    if (!unpackClientCertBundle(clientCertPassword)) {
        OC_ASSERT_X(false, "pkcs12 client cert bundle passed to HttpCredentials must be valid");
    }
}

QString HttpCredentials::authType() const
{
    return QStringLiteral("http");
}

QString HttpCredentials::user() const
{
    return _user;
}

void HttpCredentials::setAccount(Account *account)
{
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        fetchUser();
    }
    const auto isOauth = account->credentialSetting(isOAuthC());
    if (isOauth.isValid()) {
        _authType = isOauth.toBool() ? DetermineAuthTypeJob::AuthType::OAuth : DetermineAuthTypeJob::AuthType::Basic;
    }
}

QNetworkAccessManager *HttpCredentials::createQNAM() const
{
    AccessManager *qnam = new HttpCredentialsAccessManager(this);

    connect(qnam, &QNetworkAccessManager::authenticationRequired,
        this, &HttpCredentials::slotAuthentication);

    return qnam;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

QString HttpCredentials::fetchUser()
{
    _user = _account->credentialSetting(userC()).toString();
    return _user;
}

void HttpCredentials::fetchFromKeychain()
{
    _wasFetched = true;

    // User must be fetched from config file
    fetchUser();

    if (!_ready && !_refreshToken.isEmpty()) {
        // This happens if the credentials are still loaded from the keychain, bur we are called
        // here because the auth is invalid, so this means we simply need to refresh the credentials
        refreshAccessToken();
        return;
    }

    if (_ready) {
        Q_EMIT fetched();
    } else {
        fetchFromKeychainHelper();
    }
}

void HttpCredentials::fetchFromKeychainHelper()
{
    const int version = _account->credentialSetting(CredentialVersionKey()).toInt();
    if (version < CredentialVersion) {
        auto legacyCreds = new HttpLegacyCredentials(this);
        legacyCreds->fetchFromKeychainHelper();
        return;
    }

    auto readPassword = [this] {
        auto job = _account->credentialManager()->get(isUsingOAuth() ? refreshTokenKeyC() : passwordKeyC());
        connect(job, &CredentialJob::finished, this, [job, this] {
            const auto error = job->error();
            if (job->error() != error) {
                qCWarning(lcHttpLegacyCredentials) << "Could not retrieve client password from keychain" << job->errorString();
                return;
            }
            if (isUsingOAuth()) {
                _refreshToken = job->data().toString();
            } else {
                _password = job->data().toString();
            }
            if (_user.isEmpty()) {
                qCWarning(lcHttpCredentials) << "Strange: User is empty!";
            }
            if (!_refreshToken.isEmpty() && error == QKeychain::NoError) {
                refreshAccessToken();
            } else if (!_password.isEmpty() && error == QKeychain::NoError) {
                // All cool, the keychain did not come back with error.
                // Still, the password can be empty which indicates a problem and
                // the password dialog has to be opened.
                _ready = true;
                emit fetched();
            } else {
                // we come here if the password is empty or any other keychain
                // error happend.

                _fetchErrorString = job->error() != QKeychain::EntryNotFound ? job->errorString() : QString();

                _password = QString();
                _ready = false;
                emit fetched();
            }
        });
    };
    _clientCertBundle = _account->credentialSetting(clientCertBundleKeyC()).toByteArray();
    if (!_clientCertBundle.isEmpty()) {
        // New case (>=2.6): We have a bundle in the settings and read the password from
        // the keychain
        auto job = _account->credentialManager()->get(clientCertPasswordKeyC());
        connect(job, &CredentialJob::finished, this, [job, readPassword, this] {
            const auto clientCertPassword = job->data().toByteArray();
            if (job->error() != QKeychain::NoError) {
                qCWarning(lcHttpLegacyCredentials) << "Could not retrieve client cert password from keychain" << job->errorString();
            }
            // might be an empty passowrd
            if (!unpackClientCertBundle(clientCertPassword)) {
                qCWarning(lcHttpCredentials) << "Could not unpack client cert bundle";
            }
            _clientCertBundle.clear();
            readPassword();
        });
        return;
    }
    readPassword();
}

bool HttpCredentials::stillValid(QNetworkReply *reply)
{
    // The function is called in order to determine whether we need to ask the user for a password
    // if we are using oaut we already started a refresh in slotAuthentication.
    // If the refresh fails we will emit authenticationFailed ourself.
    if (isUsingOAuth()) {
        return true;
    }
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
        // returned if user or password is incorrect
        && (reply->error() != QNetworkReply::OperationCanceledError
               || !reply->property(authenticationFailedC).toBool()));
}

bool HttpCredentials::refreshAccessToken()
{
    if (_refreshToken.isEmpty())
        return false;
    _ready = false;

    OAuth *oauth = new OAuth(_account, this);
    connect(oauth, &OAuth::refreshFinished, this, [this, oauth](const QString &accessToken, const QString &refreshToken){
        oauth->deleteLater();
        _isRenewingOAuthToken = false;
        if (refreshToken.isEmpty()) {
            // an error occured, log out
            forgetSensitiveData();
            _account->handleInvalidCredentials();
            Q_EMIT authenticationFailed();
            return;
        }
        _refreshToken = refreshToken;
        if (!accessToken.isNull())
        {
            _ready = true;
            _password = accessToken;
            persist();
        }
        emit fetched();
    });
    oauth->refreshAuthentication(_refreshToken);
    _isRenewingOAuthToken = true;
    Q_EMIT authenticationStarted();
    return true;
}

void HttpCredentials::invalidateToken()
{
    if (!_password.isEmpty()) {
        _previousPassword = _password;
    }
    _password = QString();
    _ready = false;

    // User must be fetched from config file to generate a valid key
    fetchUser();

    // clear the session cookie.
    _account->clearCookieJar();

    if (!_refreshToken.isEmpty()) {
        // Only invalidate the access_token (_password) but keep the _refreshToken in the keychain
        // (when coming from forgetSensitiveData, the _refreshToken is cleared)
        return;
    }

    _account->credentialManager()->clear(QStringLiteral("http"));
    // let QNAM forget about the password
    // This needs to be done later in the event loop because we might be called (directly or
    // indirectly) from QNetworkAccessManagerPrivate::authenticationRequired, which itself
    // is a called from a BlockingQueuedConnection from the Qt HTTP thread. And clearing the
    // cache needs to synchronize again with the HTTP thread.
    QTimer::singleShot(0, _account, &Account::clearQNAMCache);
}

void HttpCredentials::forgetSensitiveData()
{
    // need to be done before invalidateToken, so it actually deletes the refresh_token from the keychain
    _refreshToken.clear();

    invalidateToken();
    _previousPassword.clear();
}

void HttpCredentials::persist()
{
    if (_user.isEmpty()) {
        // We never connected or fetched the user, there is nothing to save.
        return;
    }
    _account->setCredentialSetting(CredentialVersionKey(), CredentialVersion);
    _account->setCredentialSetting(userC(), _user);
    _account->setCredentialSetting(isOAuthC(), isUsingOAuth());
    if (!_clientCertBundle.isEmpty()) {
        // Note that the _clientCertBundle will often be cleared after usage,
        // it's just written if it gets passed into the constructor.
        _account->setCredentialSetting(clientCertBundleKeyC(), _clientCertBundle);
    }
    Q_EMIT _account->wantsAccountSaved(_account);

    // write secrets to the keychain
    if (!_clientCertBundle.isEmpty()) {
        // If we have a pkcs12 bundle, that'll be written to the config file
        // and we'll just store the bundle password in the keychain. That's prefered
        // since the keychain on older Windows platforms can only store a limited number
        // of bytes per entry and key/cert may exceed that.
        _account->credentialManager()->set(clientCertPasswordKeyC(), _clientCertPassword);
        _clientCertBundle.clear();
    } else if (isUsingOAuth()) {
        _account->credentialManager()->set(refreshTokenKeyC(), _refreshToken);
    } else {
        _account->credentialManager()->set(passwordKeyC(), _password);
    }
}

void HttpCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!_ready)
        return;
    Q_UNUSED(authenticator)
    // Because of issue #4326, we need to set the login and password manually at every requests
    // Thus, if we reach this signal, those credentials were invalid and we terminate.
    qCWarning(lcHttpCredentials) << "Stop request: Authentication failed for " << reply->url().toString() << reply->request().rawHeader("Original-Request-ID");
    reply->setProperty(authenticationFailedC, true);

    if (!_isRenewingOAuthToken && isUsingOAuth()) {
        qCInfo(lcHttpCredentials) << "Refreshing token";
        refreshAccessToken();
    }
}

bool HttpCredentials::unpackClientCertBundle(const QByteArray &clientCertPassword)
{
    if (_clientCertBundle.isEmpty())
        return true;

    QBuffer certBuffer(&_clientCertBundle);
    certBuffer.open(QIODevice::ReadOnly);
    QList<QSslCertificate> clientCaCertificates;
    return QSslCertificate::importPkcs12(
        &certBuffer, &_clientSslKey, &_clientSslCertificate, &clientCaCertificates, clientCertPassword);
}

} // namespace OCC

#include "httpcredentials.moc"
