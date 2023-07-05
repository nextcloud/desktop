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
#include "libsync/accessmanager.h"
#include "libsync/account.h"
#include "libsync/cookiejar.h"
#include "libsync/creds/abstractcredentials.h"
#include "libsync/theme.h"

#include <QJsonParseError>

namespace {

// FIXME: this is not a permanent solution, eventually we want to replace the job factories with job classes so we can store such information there
class CheckServerCoreJob : OCC::CoreJob
{
    Q_OBJECT
    friend OCC::CheckServerJobFactory;

public:
    using OCC::CoreJob::CoreJob;

private:
    // doesn't concern users of the job factory
    // we just need a place to maintain these variables, but the factory is likely deleted before the job has finished
    bool _redirectDistinct;
    bool _firstTry;
};

}

namespace OCC {

Q_LOGGING_CATEGORY(lcCheckServerJob, "sync.checkserverjob", QtInfoMsg)

CheckServerJobResult::CheckServerJobResult() = default;

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

CheckServerJobFactory CheckServerJobFactory::createFromAccount(const AccountPtr &account, bool clearCookies, QObject *parent)
{
    // in order to receive all ssl erorrs we need a fresh QNam
    auto nam = account->credentials()->createAM();
    nam->setCustomTrustedCaCertificates(account->approvedCerts());
    nam->setParent(parent);
    // do we start with the old cookies or new
    if (!(clearCookies && Theme::instance()->connectionValidatorClearCookies())) {
        const auto accountCookies = account->accessManager()->ownCloudCookieJar()->allCookies();
        nam->ownCloudCookieJar()->setAllCookies(accountCookies);
    }
    return CheckServerJobFactory(nam);
}

CoreJob *CheckServerJobFactory::startJob(const QUrl &url, QObject *parent)
{
    // the custom job class is used to store some state we need to maintain until the job has finished

    auto req = makeRequest(Utility::concatUrlPath(url, QStringLiteral("status.php")));

    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader(QByteArrayLiteral("OC-Connection-Validator"), QByteArrayLiteral("desktop"));
    req.setMaximumRedirectsAllowed(_maxRedirectsAllowed);

    auto job = new CheckServerCoreJob(nam()->get(req), parent);

    QObject::connect(job->reply(), &QNetworkReply::redirected, job, [job] {
        const auto code = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 302 || code == 307) {
            job->_redirectDistinct = false;
        }
    });

    QObject::connect(job->reply(), &QNetworkReply::finished, job, [url, job] {
        // need a mutable copy
        auto serverUrl = url;

        const QUrl targetUrl = job->reply()->url().adjusted(QUrl::RemoveFilename);

        // TODO: still needed?
        if (targetUrl.scheme() == QLatin1String("https")
            && job->reply()->sslConfiguration().sessionTicket().isEmpty()
            && job->reply()->error() == QNetworkReply::NoError) {
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
                    setJobError(job, QStringLiteral("Illegal redirect by server"));
                }
            }
        }

        const int httpStatus = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (job->reply()->error() == QNetworkReply::TooManyRedirectsError) {
            qCWarning(lcCheckServerJob) << "error:" << job->reply()->errorString();
            setJobError(job, job->reply()->errorString());
        } else if (httpStatus != 200 || job->reply()->bytesAvailable() == 0) {
            qCWarning(lcCheckServerJob) << "error: status.php replied" << httpStatus;
            setJobError(job, QStringLiteral("Invalid HTTP status code received for status.php: %1").arg(httpStatus));
        } else {
            const QByteArray body = job->reply()->peek(4 * 1024);
            QJsonParseError error;
            auto status = QJsonDocument::fromJson(body, &error);
            // empty or invalid response
            if (error.error != QJsonParseError::NoError || status.isNull()) {
                qCWarning(lcCheckServerJob) << "status.php from server is not valid JSON!" << body << job->reply()->request().url() << error.errorString();
            }

            qCInfo(lcCheckServerJob) << "status.php returns: " << status << " " << job->reply()->error() << " Reply: " << job->reply();

            if (status.object().contains(QStringLiteral("installed"))) {
                CheckServerJobResult result(status.object(), serverUrl);
                setJobResult(job, QVariant::fromValue(result));
            } else {
                qCWarning(lcCheckServerJob) << "No proper answer on " << job->reply()->url();
                setJobError(job, QStringLiteral("Did not receive expected reply from server"));
            }
        }
    });

    return job;
}

} // OCC

#include "checkserverjobfactory.moc"
