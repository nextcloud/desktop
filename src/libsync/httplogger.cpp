/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "httplogger.h"

#include "common/chronoelapsedtimer.h"
#include "common/utility.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QRegularExpression>

#include <memory>

using namespace std::chrono;

namespace {
Q_LOGGING_CATEGORY(lcNetworkHttp, "sync.httplogger", QtWarningMsg)

const qint64 PeekSize = 1024 * 1024;

bool isTextBody(const QString &s)
{
    static const QRegularExpression regexp(QStringLiteral("^(text/.*?|(application/(xml|.*?json|x-www-form-urlencoded)(;|$)))"));
    return regexp.match(s).hasMatch();
}

struct HttpContext
{
    HttpContext(const QNetworkRequest &request)
        : url(request.url().toString())
        , id(QString::fromUtf8(request.rawHeader(QByteArrayLiteral("X-Request-ID"))))
    {
    }

    const QString url;
    const QString id;

    OCC::Utility::ChronoElapsedTimer timer;
};

void logHttp(const QByteArray &verb, HttpContext *ctx, QJsonObject &&header, QIODevice *device)
{
    static const bool redact = !qEnvironmentVariableIsSet("OWNCLOUD_HTTPLOGGER_NO_REDACT");
    const auto reply = qobject_cast<QNetworkReply *>(device);
    const auto contentLength = device ? device->size() : 0;

    if (redact) {
        const QString authKey = QStringLiteral("Authorization");
        const QString auth = header.value(authKey).toString();
        header.insert(authKey, auth.startsWith(QStringLiteral("Bearer ")) ? QStringLiteral("Bearer [redacted]") : QStringLiteral("Basic [redacted]"));
    }

    QJsonObject info{{QStringLiteral("method"), QString::fromUtf8(verb)}, {QStringLiteral("id"), ctx->id}, {QStringLiteral("url"), ctx->url}};

    if (reply) {
        // respond
        QJsonObject replyInfo{{QStringLiteral("status"), reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()},
            {QStringLiteral("cached"), reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool()},
            // downcast to int, this is json
            {QStringLiteral("duration"), static_cast<int>(duration_cast<milliseconds>(ctx->timer.duration()).count())}, //
            {QStringLiteral("durationString"), QDebug::toString(ctx->timer.duration())},
            {QStringLiteral("version"),
                QStringLiteral("HTTP %1").arg(
                    reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool() ? QStringLiteral("1.1") : QStringLiteral("2"))}};
        if (reply->error() != QNetworkReply::NoError) {
            replyInfo.insert(QStringLiteral("error"), reply->errorString());
        }
        info.insert(QStringLiteral("reply"), replyInfo);
    }

    QJsonObject body = {{QStringLiteral("length"), contentLength}};
    if (contentLength > 0) {
        const QString contentType = header.value(QStringLiteral("Content-Type")).toString();
        if (isTextBody(contentType)) {
            if (!device->isOpen()) {
                Q_ASSERT(dynamic_cast<QBuffer *>(device));
                // should we close it again?
                device->open(QIODevice::ReadOnly);
            }
            Q_ASSERT(device->pos() == 0);
            QString data = QString::fromUtf8(device->peek(PeekSize));
            if (PeekSize < contentLength)
            {
                data += QStringLiteral("...(%1 bytes elided)").arg(QString::number(contentLength - PeekSize));
            }
            body[QStringLiteral("data")] = data;
        } else {
            body[QStringLiteral("data")] = QStringLiteral("%1 bytes of %2 data").arg(QString::number(contentLength), contentType);
        }
    }

    qCInfo(lcNetworkHttp).noquote() << (reply ? "RESPONSE" : "REQUEST") << ctx->id
                                    << QJsonDocument{QJsonObject{{reply ? QStringLiteral("response") : QStringLiteral("request"),
                                                         QJsonObject{{QStringLiteral("info"), info}, {QStringLiteral("header"), header},
                                                             {QStringLiteral("body"), body}}}}}
                                           .toJson(QJsonDocument::Compact);
}
}


namespace OCC {

void HttpLogger::logRequest(QNetworkReply *reply, QNetworkAccessManager::Operation operation, QIODevice *device)
{
    if (!lcNetworkHttp().isInfoEnabled()) {
        return;
    }

    auto ctx = std::make_unique<HttpContext>(reply->request());

    // device should still exist, lets still use a qpointer to ensure we have valid data
    const auto logSend = [ctx = ctx.get(), operation, reply, device = QPointer<QIODevice>(device), deviceRaw = device] {
        Q_ASSERT(!deviceRaw || device);
        ctx->timer.reset();

        const auto request = reply->request();
        QJsonObject header;
        for (const auto &key : request.rawHeaderList()) {
            header[QString::fromUtf8(key)] = QString::fromUtf8(request.rawHeader(key));
        }
        logHttp(requestVerb(operation, request), ctx, std::move(header), device);
    };
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    QObject::connect(reply, &QNetworkReply::requestSent, reply, logSend);
#else
    logSend();
#endif

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, ctx = std::move(ctx), logSend] {
        ctx->timer.stop();
        QJsonObject header;
        for (const auto &[key, value] : reply->rawHeaderPairs()) {
            header[QString::fromUtf8(key)] = QString::fromUtf8(value);
        }
        logHttp(requestVerb(*reply), ctx.get(), std::move(header), reply);
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
