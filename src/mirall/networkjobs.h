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
#include <QNetworkRequest>
#include <QNetworkReply>

class QUrl;
class QTimer;

namespace Mirall {

class Account;
class AbstractSslErrorHandler;

/**
 * @brief The AbstractNetworkJob class
 */
class AbstractNetworkJob : public QObject {
    Q_OBJECT
public:
    explicit AbstractNetworkJob(Account *account, const QString &path, QObject* parent = 0);
    virtual ~AbstractNetworkJob();

    virtual void start();

    void setAccount(Account *account);
    Account* account() const { return _account; }
    void setPath(const QString &path);
    QString path() const { return _path; }

    void setReply(QNetworkReply *reply);
    QNetworkReply* reply() const { return _reply; }

    void setTimeout(qint64 msec);
    void resetTimeout();

    void setIgnoreCredentialFailure(bool ignore);
    bool ignoreCredentialFailure() const { return _ignoreCredentialFailure; }

signals:
    void networkError(QNetworkReply *reply);
protected:
    void setupConnections(QNetworkReply *reply);
    QNetworkReply* davRequest(const QByteArray& verb, const QString &relPath,
                              QNetworkRequest req = QNetworkRequest(),
                              QIODevice *data = 0);
    QNetworkReply* davRequest(const QByteArray& verb, const QUrl &url,
                              QNetworkRequest req = QNetworkRequest(),
                              QIODevice *data = 0);
    QNetworkReply* getRequest(const QString &relPath);
    QNetworkReply* getRequest(const QUrl &url);
    QNetworkReply* headRequest(const QString &relPath);
    QNetworkReply* headRequest(const QUrl &url);

    int maxRedirects() const { return 10; }
    virtual void finished() = 0;

private slots:
    void slotFinished();
    void slotError(QNetworkReply::NetworkError);
    virtual void slotTimeout() {}

private:
    bool _ignoreCredentialFailure;
    QNetworkReply *_reply;
    Account *_account;
    QString _path;
    QTimer *_timer;
};

/**
 * @brief The EntityExistsJob class
 */
class EntityExistsJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit EntityExistsJob(Account *account, const QString &path, QObject* parent = 0);
    void start();

signals:
    void exists(QNetworkReply*);

private slots:
    virtual void finished();
};

/**
 * @brief The LsColJob class
 */
class LsColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit LsColJob(Account *account, const QString &path, QObject *parent = 0);
    void start();

signals:
    void directoryListing(const QStringList &items);

private slots:
    virtual void finished();
};

/**
 * @brief The PropfindJob class
 */
class PropfindJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit PropfindJob(Account *account, const QString &path, QObject *parent = 0);
    void start();
    void setProperties(QList<QByteArray> properties);
    QList<QByteArray> properties() const;

signals:
    void result(const QVariantMap &values);

private slots:
    virtual void finished();

private:
    QList<QByteArray> _properties;
};

/**
 * @brief The MkColJob class
 */
class MkColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit MkColJob(Account *account, const QString &path, QObject *parent = 0);
    void start();

signals:
    void finished(QNetworkReply::NetworkError);

private slots:
    virtual void finished();
};

/**
 * @brief The CheckServerJob class
 */
class CheckServerJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckServerJob(Account *account, bool followRedirect = false, QObject *parent = 0);
    void start();

    static QString version(const QVariantMap &info);
    static QString versionString(const QVariantMap &info);
    static bool installed(const QVariantMap &info);

signals:
    void instanceFound(const QUrl&url, const QVariantMap &info);
    void timeout(const QUrl&url);

private slots:
    virtual void finished();
    virtual void slotTimeout();

private:
    bool _followRedirects;
    int _redirectCount;
};


/**
 * @brief The RequestEtagJob class
 */
class RequestEtagJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit RequestEtagJob(Account *account, const QString &path, QObject *parent = 0);
    void start();

signals:
    void etagRetreived(const QString &etag);

private slots:
    virtual void finished();
};

/**
 * @brief The CheckQuota class
 */
class CheckQuotaJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckQuotaJob(Account *account, const QString &path, QObject *parent = 0);
    void start();

signals:
    void quotaRetrieved(qint64 totalBytes, qint64 availableBytes);

private slots:
    virtual void finished();
};

} // namespace Mirall

#endif // NETWORKJOBS_H
