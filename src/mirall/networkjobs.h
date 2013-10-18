/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef NETWORKJOBS_H
#define NETWORKJOBS_H

#include <QObject>
#include <QNetworkReply>

class QNetworkReply;
class QUrl;

namespace Mirall {

/**
 * @brief The AbstractNetworkJob class
 */
class AbstractNetworkJob : public QObject {
    Q_OBJECT
public:
    explicit AbstractNetworkJob(QObject* parent = 0);
    virtual ~AbstractNetworkJob();

    void setReply(QNetworkReply *reply);
    QNetworkReply* reply() const { return _reply; }
    QNetworkReply* takeReply(); // for redirect handling

signals:
    void networkError(QNetworkReply::NetworkError, const QString& errorString);

protected:
    void setupConnections(QNetworkReply *reply);
    QNetworkReply* davRequest(const QByteArray& verb, QNetworkRequest& req, QIODevice *data);
    QNetworkReply* getRequest(const QUrl& url);

private slots:
    virtual void slotFinished() = 0;
    void slotError();

private:
    QNetworkReply *_reply;
};

/**
 * @brief The LsColJob class
 */
class LsColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit LsColJob(const QUrl& url, QObject* parent = 0);

signals:
    void directoryListing(const QStringList &items);
    void networkError();

private slots:
    virtual void slotFinished();
};

class MkColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit MkColJob(const QUrl& url, QObject* parent = 0);

signals:
    void finished();
    void networkError();

private slots:
    virtual void slotFinished();
};

/**
 * @brief The CheckOwncloudJob class
 */
class CheckOwncloudJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckOwncloudJob(const QUrl& url, bool followRedirect = false, QObject* parent = 0);

signals:
    void instanceFound(const QVariantMap &info);
    void networkError();

private slots:
    virtual void slotFinished();

private:
    bool _followRedirects;
    bool _redirectCount;

    static const int MAX_REDIRECTS = 10;
};


/**
 * @brief The RequestEtagJob class
 */
class RequestEtagJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit RequestEtagJob(const QUrl &url, bool isRoot = false, QObject* parent = 0);

signals:
    void etagRetreived(const QString &etag);

private slots:
    virtual void slotFinished();
};


/**
 * @brief The CheckQuotaJob class
 */
class CheckQuotaJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckQuotaJob(const QUrl &url, QObject* parent = 0);

signals:
    void quotaRetreived(qint64 total, qint64 quotaUsedBytes);

private slots:
    virtual void slotFinished();
};
} // namespace Mirall

#endif // NETWORKJOBS_H
