/*
 * SPDX-FileCopyrightText: 2020 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "httplogger.h"

#include <QRegularExpression>
#include <QLoggingCategory>
#include <QBuffer>

using namespace Qt::StringLiterals;

namespace
{
Q_LOGGING_CATEGORY(lcNetworkHttp, "sync.httplogger", QtWarningMsg)

const qint64 PeekSize = 1024 * 1024;

const QByteArray XRequestId(){
    return QByteArrayLiteral("X-Request-ID");
}

bool isTextBody(const QString &s)
{
    static const QRegularExpression regexp(QStringLiteral("^(text/.*|(application/(xml|json|x-www-form-urlencoded)(;|$)))"));
    return regexp.match(s).hasMatch();
}

void logHttp(const QByteArray &verb,
             const QString &url,
             const QByteArray &id,
             const QString &contentType,
             const QList<QNetworkReply::RawHeaderPair> &header,
             QIODevice *device,
             bool metadataOnly)
{
    const auto reply = qobject_cast<QNetworkReply *>(device);
    QString msg;
    QTextStream stream(&msg);
    stream << id << ": ";
    if (!reply) {
        stream << "Request: ";
    } else {
        stream << "Response: ";
    }
    stream << verb;
    if (reply) {
        stream << " " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    if (metadataOnly) {
        qCInfo(lcNetworkHttp) << msg;
        return;
    }

    stream << " " << url << " Header: { ";
    for (const auto &it : header) {
        stream << it.first << ": ";
        if (it.first.compare("Authorization"_ba, Qt::CaseInsensitive) == 0) {
            stream << (it.second.startsWith("Bearer ") ? "Bearer" : "Basic");
            stream << " [redacted]";
        } else {
            stream << it.second;
        }
        stream << ", ";
    }
    stream << "} Data: [";
    const auto contentLength = device ? device->size() : 0;
    if (contentLength > 0) {
        if (isTextBody(contentType)) {
            if (!device->isOpen()) {
                Q_ASSERT(dynamic_cast<QBuffer *>(device));
                // should we close it again?
                device->open(QIODevice::ReadOnly);
            }
            Q_ASSERT(device->pos() == 0);
            stream << device->peek(PeekSize);
            if (PeekSize < contentLength)
            {
                stream << "...(" << (contentLength - PeekSize) << "bytes elided)";
            }
        } else {
            stream << contentLength << " bytes of " << contentType << " data";
        }
    }
    stream << "]";
    qCInfo(lcNetworkHttp) << msg;
}
}

namespace OCC {

QString HttpLogger::requestLogContext(const QNetworkReply &reply)
{
    const auto request = reply.request();
    if (isMetadataOnly(request)) {
        return u"request ID %1"_s.arg(QString::fromLatin1(request.rawHeader(XRequestId())));
    }
    return reply.url().toString();
}

void HttpLogger::logRequest(QNetworkReply *reply, QNetworkAccessManager::Operation operation, QIODevice *device)
{
    const auto request = reply->request();
    if (!lcNetworkHttp().isInfoEnabled()) {
        return;
    }
    const auto metadataOnly = isMetadataOnly(request);
    const auto keys = request.rawHeaderList();
    QList<QNetworkReply::RawHeaderPair> header;
    header.reserve(keys.size());
    for (const auto &key : keys) {
        header << qMakePair(key, request.rawHeader(key));
    }
    logHttp(requestVerb(operation, request),
            request.url().toString(),
            request.rawHeader(XRequestId()),
            request.header(QNetworkRequest::ContentTypeHeader).toString(),
            header,
            device,
            metadataOnly);

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, metadataOnly] {
        logHttp(requestVerb(*reply),
                reply->url().toString(),
                reply->request().rawHeader(XRequestId()),
                reply->header(QNetworkRequest::ContentTypeHeader).toString(),
                reply->rawHeaderPairs(),
                reply,
                metadataOnly);
    });
}

QByteArray HttpLogger::requestVerb(QNetworkAccessManager::Operation operation, const QNetworkRequest &request)
{
    switch (operation) {
    case QNetworkAccessManager::HeadOperation:
        return QByteArrayLiteral("HEAD");
    case QNetworkAccessManager::GetOperation:
        return QByteArrayLiteral("GET");
    case QNetworkAccessManager::PutOperation:
        return QByteArrayLiteral("PUT");
    case QNetworkAccessManager::PostOperation:
        return QByteArrayLiteral("POST");
    case QNetworkAccessManager::DeleteOperation:
        return QByteArrayLiteral("DELETE");
    case QNetworkAccessManager::CustomOperation:
        return request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
    case QNetworkAccessManager::UnknownOperation:
        break;
    }
    Q_UNREACHABLE();
}

}
