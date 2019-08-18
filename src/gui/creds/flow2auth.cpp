/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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

#include <QDesktopServices>
#include <QTimer>
#include <QBuffer>
#include "account.h"
#include "creds/flow2auth.h"
#include <QJsonObject>
#include <QJsonDocument>
#include "theme.h"
#include "networkjobs.h"
#include "configfile.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFlow2auth, "nextcloud.sync.credentials.flow2auth", QtInfoMsg)

Flow2Auth::~Flow2Auth()
{
}

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
    _pollTimer.stop();

    // Step 1: Initiate a login, do an anonymous POST request
    QUrl url = Utility::concatUrlPath(_account->url().toString(), QLatin1String("/index.php/login/v2"));

    auto job = _account->sendRequest("POST", url);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));

    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        auto jsonData = reply->readAll();
        QJsonParseError jsonParseError;
        QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();

        QString pollToken = json.value("poll").toObject().value("token").toString();
        QString pollEndpoint = json.value("poll").toObject().value("endpoint").toString();
        QUrl loginUrl = json["login"].toString();

        if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError
            || json.isEmpty() || pollToken.isEmpty() || pollEndpoint.isEmpty() || loginUrl.isEmpty()) {
            QString errorReason;
            QString errorFromJson = json["error"].toString();
            if (!errorFromJson.isEmpty()) {
                errorReason = tr("Error returned from the server: <em>%1</em>")
                                  .arg(errorFromJson.toHtmlEscaped());
            } else if (reply->error() != QNetworkReply::NoError) {
                errorReason = tr("There was an error accessing the 'token' endpoint: <br><em>%1</em>")
                                  .arg(reply->errorString().toHtmlEscaped());
            } else if (jsonParseError.error != QJsonParseError::NoError) {
                errorReason = tr("Could not parse the JSON returned from the server: <br><em>%1</em>")
                                  .arg(jsonParseError.errorString());
            } else {
                errorReason = tr("The reply from the server did not contain all expected fields");
            }
            qCWarning(lcFlow2auth) << "Error when getting the loginUrl" << json << errorReason;
            emit result(Error);
            return;
        }


        _loginUrl = loginUrl;
        _pollToken = pollToken;
        _pollEndpoint = pollEndpoint;


        ConfigFile cfg;
        std::chrono::milliseconds polltime = cfg.remotePollInterval();
        qCInfo(lcFlow2auth) << "setting remote poll timer interval to" << polltime.count() << "msec";
        _pollTimer.setInterval(polltime.count());
        QObject::connect(&_pollTimer, &QTimer::timeout, this, &Flow2Auth::slotPollTimerTimeout);
        _pollTimer.start();


		if (!QDesktopServices::openUrl(authorisationLink())) {
            // TODO: Ask the user to copy and open the link instead of failing here!

            // We cannot open the browser, then we claim we don't support OAuth.
            emit result(NotSupported, QString());
        }
    });
}

void Flow2Auth::slotPollTimerTimeout()
{
	_pollTimer.stop();

    // Step 2: Poll
    QNetworkRequest req;
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    auto requestBody = new QBuffer;
    QUrlQuery arguments(QString("token=%1").arg(_pollToken));
    requestBody->setData(arguments.query(QUrl::FullyEncoded).toLatin1());

    auto job = _account->sendRequest("POST", _pollEndpoint, req, requestBody);
    job->setTimeout(qMin(30 * 1000ll, job->timeoutMsec()));
    QObject::connect(job, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        auto jsonData = reply->readAll();
        QJsonParseError jsonParseError;
        QJsonObject json = QJsonDocument::fromJson(jsonData, &jsonParseError).object();

        QUrl serverUrl = json["server"].toString();
        QString loginName = json["loginName"].toString();
        QString appPassword = json["appPassword"].toString();

        if (reply->error() != QNetworkReply::NoError || jsonParseError.error != QJsonParseError::NoError
            || json.isEmpty() || serverUrl.isEmpty() || loginName.isEmpty() || appPassword.isEmpty()) {
            QString errorReason;
            QString errorFromJson = json["error"].toString();
            if (!errorFromJson.isEmpty()) {
                errorReason = tr("Error returned from the server: <em>%1</em>")
                                  .arg(errorFromJson.toHtmlEscaped());
            } else if (reply->error() != QNetworkReply::NoError) {
                errorReason = tr("There was an error accessing the 'token' endpoint: <br><em>%1</em>")
                                  .arg(reply->errorString().toHtmlEscaped());
            } else if (jsonParseError.error != QJsonParseError::NoError) {
                errorReason = tr("Could not parse the JSON returned from the server: <br><em>%1</em>")
                                  .arg(jsonParseError.errorString());
            } else {
                errorReason = tr("The reply from the server did not contain all expected fields");
            }
            qCDebug(lcFlow2auth) << "Error when polling for the appPassword" << json << errorReason;

			_pollTimer.start();
            return;
        }

        qCInfo(lcFlow2auth) << "Success getting the appPassword for user: " << loginName << " on server: " << serverUrl;
        
		_account->setUrl(serverUrl);

		emit result(LoggedIn, loginName, appPassword);
    });
}

} // namespace OCC
