/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantapijob.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace OCC
{

namespace
{

Q_LOGGING_CATEGORY(lcAssistantApiJob, "nextcloud.gui.assistant.api", QtInfoMsg)

int statusCodeFromJson(const QString &jsonStr, int fallback)
{
    if (jsonStr.contains("<?xml version=\"1.0\"?>"_L1)) {
        static const QRegularExpression xmlRegex("<statuscode>(\\d+)</statuscode>"_L1);
        const auto match = xmlRegex.match(jsonStr);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
        return fallback;
    }

    static const QRegularExpression jsonRegex(R"("statuscode":(\d+))");
    const auto match = jsonRegex.match(jsonStr);
    return match.hasMatch() ? match.captured(1).toInt() : fallback;
}

}

AssistantApiJob::AssistantApiJob(const AccountPtr &account, const QString &path, QObject *parent)
    : SimpleApiJob(account, path, parent)
{
}

void AssistantApiJob::setFormBody(const QUrlQuery &query)
{
    const auto body = query.toString(QUrl::FullyEncoded).toUtf8();
    setBody(body);
    request().setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
}

bool AssistantApiJob::finished()
{
    qCInfo(lcAssistantApiJob) << "Assistant API request to" << reply()->request().url() << "finished with status" << replyStatusString();

    const auto httpStatusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcAssistantApiJob) << "Network error:" << path() << errorString() << httpStatusCode;
        Q_EMIT jsonReceived({}, httpStatusCode);
        return true;
    }

    const auto replyData = reply()->readAll();
    const auto statusCode = statusCodeFromJson(QString::fromUtf8(replyData), httpStatusCode);

    auto error = QJsonParseError{};
    const auto json = QJsonDocument::fromJson(replyData, &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(lcAssistantApiJob) << "Invalid JSON response:" << error.errorString();
    }

    Q_EMIT jsonReceived(json, statusCode);
    return true;
}

}
