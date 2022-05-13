/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "checkserverjobfactory.h"
#include "common/utility.h"
#include "creds/httpcredentials.h"
#include <QJsonParseError>

namespace {

// FIXME: this is not a permanent solution, eventually we want to replace the job factories with job classes so we can store such information there
class CheckServerCoreJob : OCC::CoreJob
{
    friend OCC::CheckServerJobFactory;

private:
    // doesn't concern users of the job factory
    // we just need a place to maintain these variables, but the factory is likely deleted before the job has finished
    bool _redirectDistinct;
    bool _firstTry;
};

}

namespace OCC {

Q_LOGGING_CATEGORY(lcCheckServerJob, "sync.checkserverjob", QtInfoMsg)

CheckServerJobResult::CheckServerJobResult(const QJsonObject &statusObject, const QUrl &serverUrl)
    : _statusObject(statusObject)
    , _serverUrl(serverUrl)
{
}

QJsonObject CheckServerJobResult::statusObject() const
{
    return _statusObject;
}

QUrl CheckServerJobResult::serverUrl() const
{
    return _serverUrl;
}

CoreJob *CheckServerJobFactory::startJob(const QUrl &url)
{
    // the custom job class is used to store some state we need to maintain until the job has finished
    auto job = new CheckServerCoreJob;

    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("status.php")));

    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader(QByteArrayLiteral("OC-Connection-Validator"), QByteArrayLiteral("desktop"));
    req.setMaximumRedirectsAllowed(_maxRedirectsAllowed);

    auto *reply = nam()->get(req);

    connect(reply, &QNetworkReply::redirected, job, [reply, job] {
        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 302 || code == 307) {
            job->_redirectDistinct = false;
        }
    });

    connect(reply, &QNetworkReply::finished, job, [url, reply, job] {
        reply->deleteLater();

        // need a mutable copy
        auto serverUrl = url;

        const QUrl targetUrl = reply->url().adjusted(QUrl::RemoveFilename);

        // TODO: still needed?
        if (targetUrl.scheme() == QLatin1String("https")
            && reply->sslConfiguration().sessionTicket().isEmpty()
            && reply->error() == QNetworkReply::NoError) {
            qCWarning(lcCheckServerJob) << "No SSL session identifier / session ticket is used, this might impact sync performance negatively.";
        }

        if (!Utility::urlEqual(serverUrl, targetUrl)) {
            if (job->_redirectDistinct) {
                serverUrl = targetUrl;
            } else {
                if (job->_firstTry) {
                    qCWarning(lcCheckServerJob) << "Server might have moved, retry";
                    job->_firstTry = false;
                    job->_redirectDistinct = true;

                    // FIXME
                } else {
                    qCWarning(lcCheckServerJob) << "We got a temporary moved server aborting";
                    setJobError(job, QStringLiteral("Illegal redirect by server"), reply);
                }
            }
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::TooManyRedirectsError) {
            qCWarning(lcCheckServerJob) << "error:" << reply->errorString();
            setJobError(job, reply->errorString(), reply);
        } else if (httpStatus != 200 || reply->bytesAvailable() == 0) {
            qCWarning(lcCheckServerJob) << "error: status.php replied " << httpStatus;
            setJobError(job, QStringLiteral("Invalid HTTP status code received for status.php: %1").arg(httpStatus), reply);
        } else {
            const QByteArray body = reply->peek(4 * 1024);
            QJsonParseError error;
            auto status = QJsonDocument::fromJson(body, &error);
            // empty or invalid response
            if (error.error != QJsonParseError::NoError || status.isNull()) {
                qCWarning(lcCheckServerJob) << "status.php from server is not valid JSON!" << body << reply->request().url() << error.errorString();
            }

            qCInfo(lcCheckServerJob) << "status.php returns: " << status << " " << reply->error() << " Reply: " << reply;

            if (status.object().contains(QStringLiteral("installed"))) {
                CheckServerJobResult result(status.object(), serverUrl);
                setJobResult(job, QVariant::fromValue(result));
            } else {
                qCWarning(lcCheckServerJob) << "No proper answer on " << reply->url();
                setJobError(job, QStringLiteral("Did not receive expected reply from server"), reply);
            }
        }
    });

    return job;
}

} // OCC
