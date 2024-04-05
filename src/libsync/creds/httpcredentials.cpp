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

#include "accessmanager.h"
#include "account.h"
#include "configfile.h"
#include "creds/credentialmanager.h"
#include "oauth.h"
#include "syncengine.h"

#include <QAuthenticator>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>

#include <chrono>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcHttpCredentials, "sync.credentials.http", QtInfoMsg)

namespace {
constexpr int TokenRefreshMaxRetries = 3;
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
            }
        }
        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const HttpCredentials> _cred;
};

HttpCredentials::HttpCredentials(DetermineAuthTypeJob::AuthType authType, const QString &user, const QString &password)
    : _user(user)
    , _password(password)
    , _ready(true)
    , _authType(authType)
{
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

AccessManager *HttpCredentials::createAM() const
{
    AccessManager *am = new HttpCredentialsAccessManager(this);

    connect(am, &QNetworkAccessManager::authenticationRequired,
        this, &HttpCredentials::slotAuthentication);

    return am;
}

bool HttpCredentials::ready() const
{
    return _ready;
}

QString HttpCredentials::fetchUser()
{
    // it makes no sense to overwrite an existing username with a config file value
    if (_user.isEmpty()) {
        qCDebug(lcHttpCredentials) << "user not set, populating from settings";
        _user = _account->credentialSetting(userC()).toString();
    } else {
        qCDebug(lcHttpCredentials) << "user already set, no need to fetch from settings";
    }
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
    if (_user.isEmpty()) {
        _password.clear();
        _ready = false;
        Q_EMIT fetched();
        return;
    }
    auto job = _account->credentialManager()->get(isUsingOAuth() ? refreshTokenKeyC() : passwordKeyC());
    connect(job, &CredentialJob::finished, this, [job, this] {
        auto handleError = [job, this] {
            qCWarning(lcHttpCredentials) << "Could not retrieve client password from keychain" << job->errorString();

            // we come here if the password is empty or any other keychain
            // error happend.

            _fetchErrorString = job->error() != QKeychain::EntryNotFound ? job->errorString() : QString();

            _password.clear();
            _ready = false;
            Q_EMIT fetched();
        };
        if (job->error() != QKeychain::NoError) {
            handleError();
            return;
        }
        const auto data = job->data().toString();
        if (OC_ENSURE(!data.isEmpty())) {
            if (isUsingOAuth()) {
                _refreshToken = data;
                refreshAccessToken();
            } else {
                _password = data;
                _ready = true;
                Q_EMIT fetched();
            }
        } else {
            handleError();
        }
    });
}

bool HttpCredentials::stillValid(QNetworkReply *reply)
{
    if (isUsingOAuth()) {
        // The function is called in order to determine whether we need to ask the user for a password
        // if we are using OAuth, we already started a refresh in slotAuthentication, at least in theory, ensure the auth is started.
        // If the refresh fails, we are going to Q_EMIT authenticationFailed ourselves
        if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            slotAuthentication(reply, nullptr);
        }
        return true;
    }
    return ((reply->error() != QNetworkReply::AuthenticationRequiredError)
        // returned if user or password is incorrect
        && (reply->error() != QNetworkReply::OperationCanceledError
               || !reply->property(authenticationFailedC).toBool()));
}

void HttpCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator)
{
    qCDebug(lcHttpCredentials) << Q_FUNC_INFO << reply;
    if (!_ready)
        return;
    Q_UNUSED(authenticator)
    // Because of issue #4326, we need to set the login and password manually at every requests
    // Thus, if we reach this signal, those credentials were invalid and we terminate.
    qCWarning(lcHttpCredentials) << "Stop request: Authentication failed for " << reply->url().toString() << reply->request().rawHeader("Original-Request-ID");
    reply->setProperty(authenticationFailedC, true);

    if (!_oAuthJob && isUsingOAuth()) {
        qCInfo(lcHttpCredentials) << "Refreshing token";
        refreshAccessToken();
    }
}

bool HttpCredentials::refreshAccessToken()
{
    return refreshAccessTokenInternal(0);
}

bool HttpCredentials::refreshAccessTokenInternal(int tokenRefreshRetriesCount)
{
    if (_refreshToken.isEmpty())
        return false;
    if (_oAuthJob) {
        return true;
    }

    // don't touch _ready or the account state will start a new authentication
    // _ready = false;

    // parent with nam to ensure we reset when the nam is reset
    _oAuthJob = new AccountBasedOAuth(_account->sharedFromThis(), _account->accessManager());
    connect(_oAuthJob, &AccountBasedOAuth::refreshError, this, [tokenRefreshRetriesCount, this](QNetworkReply::NetworkError error, const QString &) {
        _oAuthJob->deleteLater();
        int nextTry = tokenRefreshRetriesCount + 1;
        std::chrono::seconds timeout = {};
        switch (error) {
        case QNetworkReply::ContentNotFoundError:
            // 404: bigip f5?
            timeout = 0s;
            break;
        case QNetworkReply::HostNotFoundError:
            [[fallthrough]];
        case QNetworkReply::TimeoutError:
            [[fallthrough]];
        // Qt reports OperationCanceledError if the request timed out
        case QNetworkReply::OperationCanceledError:
            [[fallthrough]];
        case QNetworkReply::TemporaryNetworkFailureError:
            [[fallthrough]];
        // VPN not ready?
        case QNetworkReply::ConnectionRefusedError:
            nextTry = 0;
            [[fallthrough]];
        default:
            timeout = 30s;
        }
        if (nextTry >= TokenRefreshMaxRetries) {
            qCWarning(lcHttpCredentials) << "Too many failed refreshes" << nextTry << "-> log out";
            forgetSensitiveData();
            Q_EMIT authenticationFailed();
            Q_EMIT fetched();
            return;
        }
        QTimer::singleShot(timeout, this, [nextTry, this] {
            refreshAccessTokenInternal(nextTry);
        });
        Q_EMIT authenticationFailed();
    });

    connect(_oAuthJob, &AccountBasedOAuth::refreshFinished, this, [this](const QString &accessToken, const QString &refreshToken) {
        _oAuthJob->deleteLater();
        if (refreshToken.isEmpty()) {
            // an error occured, log out
            forgetSensitiveData();
            Q_EMIT authenticationFailed();
            Q_EMIT fetched();
            return;
        }
        _refreshToken = refreshToken;
        if (!accessToken.isNull()) {
            _ready = true;
            _password = accessToken;
            persist();
        }
        Q_EMIT fetched();
    });
    Q_EMIT authenticationStarted();
    _oAuthJob->refreshAuthentication(_refreshToken);

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
    QTimer::singleShot(0, _account, &Account::clearAMCache);
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
    Q_EMIT _account->wantsAccountSaved(_account);

    // write secrets to the keychain
    if (isUsingOAuth()) {
        // _refreshToken should only be empty when we are logged out...
        if (!_refreshToken.isEmpty()) {
            _account->credentialManager()->set(refreshTokenKeyC(), _refreshToken);
        }
    } else {
        if (!_password.isEmpty()) {
            _account->credentialManager()->set(passwordKeyC(), _password);
        }
    }
}

} // namespace OCC

#include "httpcredentials.moc"
