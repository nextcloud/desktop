/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QMutex>
#include <QSize>
#include <QVariant>

class QJsonDocument;
class QLocalSocket;
class QNetworkReply;

namespace OCC {
class ShellExtensionsServer : public QObject
{
    struct ThumbnailRequestInfo
    {
        QString path;
        QSize size;
        QString folderAlias;

        [[nodiscard]] bool isValid() const { return !path.isEmpty() && !size.isEmpty() && !folderAlias.isEmpty(); }
    };

    struct CustomStateRequestInfo
    {
        QString path;
        QString folderAlias;

        bool isValid() const { return !path.isEmpty() && !folderAlias.isEmpty(); }
    };

    Q_OBJECT
public:
    ShellExtensionsServer(QObject *parent = nullptr);
    ~ShellExtensionsServer() override;

    static QString getFetchThumbnailPath();

    void setIsSharedInvalidationInterval(qint64 interval);

signals:
    void directoryListingIterationFinished(const QString &folderAlias);

private:
    void sendJsonMessageWithVersion(QLocalSocket *socket, const QVariantMap &message);
    void sendEmptyDataAndCloseSession(QLocalSocket *socket);
    void closeSession(QLocalSocket *socket);
    void processCustomStateRequest(QLocalSocket *socket, const CustomStateRequestInfo &customStateRequestInfo);
    void processThumbnailRequest(QLocalSocket *socket, const ThumbnailRequestInfo &thumbnailRequestInfo);

    void parseCustomStateRequest(QLocalSocket *socket, const QVariantMap &message);
    void parseThumbnailRequest(QLocalSocket *socket, const QVariantMap &message);

private slots:
    void slotNewConnection();

private:
    QLocalServer _localServer;
    QStringList _runningLsColJobsForPaths;
    QMap<qintptr, QMetaObject::Connection> _customStateSocketConnections;
    qint64 _isSharedInvalidationInterval = 0;
};
} // namespace OCC
