/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "flow2auth.h"

#include "abstractnetworkjob.h"
#include "account.h"
#include "config.h"
#include "configfile.h"
#include "guiutility.h"
#include "networkjobs.h"
#include "theme.h"

#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QBuffer>
#include <QJsonObject>
#include <QJsonDocument>

namespace OCC {

Q_LOGGING_CATEGORY(lcFlow2auth, "nextcloud.sync.credentials.flow2auth", QtInfoMsg)


Flow2Auth::Flow2Auth(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
    _pollTimer.setInterval(1000);
    QObject::connect(&_pollTimer, &QTimer::timeout, this, &Flow2Auth::slotPollTimerTimeout);
}

Flow2Auth::~Flow2Auth() = default;

void Flow2Auth::start()
{
    // Note: All startup code is in openBrowser() to allow reinitiate a new request with
    //       fresh tokens. Opening the same pollEndpoint link twice triggers an expiration
    //       message by the server (security, intended design).
    openBrowser();
}

QUrl Flow2Auth::authorisationLink() const
{
    return _loginUrl;
}

void Flow2Auth::openBrowser()
{
    fetchNewToken(TokenAction::actionOpenBrowser);
}

void Flow2Auth::copyLinkToClipboard()
{
    fetchNewToken(TokenAction::actionCopyLinkToClipboard);
}

void Flow2Auth::fetchNewToken(const TokenAction action)
{
    if (_isBusy) {
        return;
    }

    _isBusy = true;
    _hasToken = false;

    emit statusChanged(PollStatus::statusFetchToken, 0);

    // Step 1: Initiate a login, do an anonymous POST request
    const auto loginV2url = Utility::concatUrlPath(_account->url().toString(), QLatin1String("/index.php/login/v2"));
    _enforceHttps = loginV2url.scheme() == QStringLiteral("https");

    // add 'Content-Length: 0' header (see https://github.com/nextcloud/desktop/issues/1473)
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentLengthHeader, "0");
    request.setHeader(QNetworkRequest::UserAgentHeader, Utility::userAgentString());

    auto job = _account->sendRequest("POST", loginV2url, request);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));

    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this, action](QNetworkReply *reply) {
        const auto json = handleResponse(reply);
        QString pollToken, pollEndpoint, loginUrl;

        if (!json.isEmpty()) {
            pollToken = json.value("poll").toObject().value("token").toString();
            pollEndpoint = json.value("poll").toObject().value("endpoint").toString();
            loginUrl = json["login"].toString();
        }

        if (json.isEmpty() || pollToken.isEmpty() || pollEndpoint.isEmpty() || loginUrl.isEmpty()) {
            _pollTimer.stop();
            _isBusy = false;
            return;
        }

        _loginUrl = loginUrl;

        if (_account->isUsernamePrefillSupported()) {
            constexpr auto setUserNameForLogin = [] (const auto &userName, auto &loginUrl) -> void {
                auto query = QUrlQuery(loginUrl);
                query.addQueryItem(QStringLiteral("user"), userName);
                loginUrl.setQuery(query);
            };

            if (const auto userNameFromCredentials = _account->userFromCredentials(); !userNameFromCredentials.isEmpty()) {
                setUserNameForLogin(userNameFromCredentials, _loginUrl);
            } else if (const auto currentUserName = Utility::getCurrentUserName(); !WIN_DISABLE_USERNAME_PREFILL && !currentUserName.isEmpty()) {
                setUserNameForLogin(currentUserName, _loginUrl);
            }
        }

        _pollToken = pollToken;
        _pollEndpoint = pollEndpoint;

        // Start polling
        std::chrono::milliseconds polltime = ConfigFile().remotePollInterval();
        qCInfo(lcFlow2auth) << "setting remote poll timer interval to" << polltime.count() << "msec";
        _secondsInterval = (polltime.count() / 1000);
        _secondsLeft = _secondsInterval;
        emit statusChanged(PollStatus::statusPollCountdown, _secondsLeft);

        if (!_pollTimer.isActive()) {
            _pollTimer.start();
        }

        switch (action) {
        case actionOpenBrowser:
            // Try to open Browser
            if (!Utility::openBrowser(authorisationLink())) {
                // We cannot open the browser, then we claim we don't support Flow2Auth.
                // Our UI callee will ask the user to copy and open the link.
                emit result(NotSupported);
            }
            break;
        case actionCopyLinkToClipboard:
            QApplication::clipboard()->setText(authorisationLink().toString(QUrl::FullyEncoded));
            emit statusChanged(PollStatus::statusCopyLinkToClipboard, 0);
            break;
        }

        _isBusy = false;
        _hasToken = true;
    });
}

void Flow2Auth::slotPollTimerTimeout()
{
    if (_isBusy || !_hasToken) {
        return;
    }

    _isBusy = true;

    _secondsLeft--;
    if(_secondsLeft > 0) {
        emit statusChanged(PollStatus::statusPollCountdown, _secondsLeft);
        _isBusy = false;
        return;
    }
    emit statusChanged(PollStatus::statusPollNow, 0);

    // Step 2: Poll
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    auto requestBody = new QBuffer;
    QUrlQuery arguments(QStringLiteral("token=%1").arg(_pollToken));
    requestBody->setData(arguments.query(QUrl::FullyEncoded).toLatin1());

    auto job = _account->sendRequest("POST", _pollEndpoint, request, requestBody);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));

    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        const QJsonObject json = handleResponse(reply);
        QUrl serverUrl;
        QString loginName, appPassword;

        if (!json.isEmpty()) {
            serverUrl = json["server"].toString();
            loginName = json["loginName"].toString();
            appPassword = json["appPassword"].toString();
        }

        if (json.isEmpty() || serverUrl.isEmpty() || loginName.isEmpty() || appPassword.isEmpty()) {
            // Forget sensitive data
            appPassword.clear();
            loginName.clear();

            // Failed: poll again
            _secondsLeft = _secondsInterval;
            _isBusy = false;
            return;
        }

        _pollTimer.stop();

        // Success
        qCInfo(lcFlow2auth) << "Success getting the appPassword for user: " << loginName << ", server: " << serverUrl.toString();

        _account->setUrl(serverUrl);

        emit result(LoggedIn, QString(), loginName, appPassword);

        // Forget sensitive data
        appPassword.clear();
        loginName.clear();

        _loginUrl.clear();
        _pollToken.clear();
        _pollEndpoint.clear();

        _isBusy = false;
        _hasToken = false;
    });
}

QJsonObject Flow2Auth::handleResponse(QNetworkReply *reply)
{
    const auto jsonData = reply->readAll();
    QJsonParseError jsonParseError{};
    const auto json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();
    if (reply->error() == QNetworkReply::NoError && jsonParseError.error == QJsonParseError::NoError
        && !json.isEmpty()) {
        const auto isHttps = [&]() {
            const auto endpoint = json["server"].toString().isEmpty()
                ? json.value("poll").toObject().value("endpoint").toString() //from login/v2 endpoint
                : json["server"].toString(); //from login/v2/poll endpoint

            if (endpoint.isEmpty()) {
                return false;
            }

            qCDebug(lcFlow2auth) << "Server url returned is" << endpoint;
            if (QUrl(endpoint).scheme() != QStringLiteral("https")) {
                return false;
            }

            return true;
        };

        if (_enforceHttps && !isHttps()) {
            qCWarning(lcFlow2auth) << "Returned server url | poll endpoint does not start with https";
            emit result(Error, tr("The returned server URL does not start with HTTPS despite the login URL started with HTTPS. "
                                  "Login will not be possible because this might be a security issue. Please contact your administrator."));
            return {};
        }
    }

    if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError) {
        QString errorReason;
        const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (httpStatus == 503) {
            errorReason = tr("The server is temporarily unavailable because it is in maintenance mode. "
                             "Please try again once maintenance has finished.");
        } else if (const auto errorFromJson = json["error"].toString();
            !errorFromJson.isEmpty()) {
            errorReason = tr("An unexpected error occurred when trying to access the server. "
                "Please try to access it again later or contact your server administrator if the issue continues.");
            qCWarning(lcFlow2auth) << "Error returned from JSON:" << errorFromJson;
        } else if (reply->error() != QNetworkReply::NoError) {
            auto errorStringFromReply = reply->errorString();
            if (const auto hstsError = AbstractNetworkJob::hstsErrorStringFromReply(reply)) {
                errorStringFromReply = *hstsError;
            }
            errorReason = tr("An unexpected error occurred when trying to access the server. "
                "Please try to access it again later or contact your server administrator if the issue continues.");
            qCWarning(lcFlow2auth) << "Error string returned from the server:" << errorStringFromReply;
        } else if (jsonParseError.error != QJsonParseError::NoError || json.isEmpty()) {
            // Could not parse the JSON returned from the server
            errorReason = tr("We couldn't parse the server response. "
                "Please try connecting again later or contact your server administrator if the issue continues.");
        } else if (json.isEmpty()) {
            // The reply from the server did not contain all expected fields
            errorReason = tr("The server did not reply with the expected data. "
                "Please try connecting again later or contact your server administrator if the issue continues.");
        }

        qCWarning(lcFlow2auth) << "Error when requesting:" << reply->url()
                               << "- json returned:" << json
                               << "- http status code:" << httpStatus
                               << "- error:" << jsonParseError.errorString();

        // We get a 404 until authentication is done, so don't show this error in the GUI.
        if (reply->error() != QNetworkReply::ContentNotFoundError) {
            emit result(Error, errorReason);
        }

        return {};
    }

    return json;
}

void Flow2Auth::slotPollNow()
{
    // poll now if we're not already doing so
    if (_isBusy || !_hasToken) {
        return;
    }

    _secondsLeft = 1;
    slotPollTimerTimeout();
}

} // namespace OCC
