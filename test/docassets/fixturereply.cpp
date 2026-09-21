/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixturereply.h"
#include <QTimer>
#include <cstring>

namespace OCC::DocAssets
{
FixtureReply::FixtureReply(QNetworkAccessManager::Operation operation,
                           const QNetworkRequest &request,
                           const QByteArray &payload,
                           const QByteArray &contentType,
                           QObject *parent)
    : QNetworkReply(parent)
    , _payload(payload)
{
    setOperation(operation);
    setRequest(request);
    setUrl(request.url());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, payload.isNull() ? 404 : 200);
    setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    setHeader(QNetworkRequest::ContentLengthHeader, payload.size());
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QTimer::singleShot(0, this, [this] {
        if (isFinished()) {
            return;
        }
        if (_payload.isNull()) {
            setError(ContentNotFoundError, QStringLiteral("No local fixture for request"));
            Q_EMIT errorOccurred(error());
        }
        Q_EMIT metaDataChanged();
        if (!_payload.isEmpty()) {
            Q_EMIT readyRead();
        }
        if (isFinished()) {
            return;
        }
        setFinished(true);
        Q_EMIT finished();
    });
}
void FixtureReply::abort()
{
    if (isFinished()) {
        return;
    }
    setError(OperationCanceledError, QStringLiteral("Fixture request cancelled"));
    setFinished(true);
    Q_EMIT errorOccurred(error());
    Q_EMIT finished();
}
qint64 FixtureReply::bytesAvailable() const
{
    return _payload.size() - _offset + QNetworkReply::bytesAvailable();
}
qint64 FixtureReply::readData(char *data, qint64 maximum)
{
    const auto count = qMin(maximum, qint64(_payload.size() - _offset));
    if (count <= 0) {
        return -1;
    }
    std::memcpy(data, _payload.constData() + _offset, size_t(count));
    _offset += count;
    return count;
}
}
