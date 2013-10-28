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

    void setAccount(Account *account);
    Account* account() const { return _account; }
    void setPath(const QString &path);
    QString path() const { return _path; }

    void setReply(QNetworkReply *reply);
    QNetworkReply* reply() const { return _reply; }

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

private slots:
    virtual void slotFinished() = 0;
    void slotError();

private:
    QNetworkReply *_reply;
    Account *_account;
    QString _path;
};

/**
 * @brief The EntityExistsJob class
 */
class EntityExistsJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit EntityExistsJob(Account *account, const QString &path, QObject* parent = 0);
signals:
    void exists(QNetworkReply*);

private slots:
    virtual void slotFinished();
};

/**
 * @brief The LsColJob class
 */
class LsColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit LsColJob(Account *account, const QString &path, QObject *parent = 0);

signals:
    void directoryListing(const QStringList &items);

private slots:
    virtual void slotFinished();
};

/**
 * @brief The CheckQuotaJob class
 */
class PropfindJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit PropfindJob(Account *account, const QString &path,
                         QList<QByteArray> properties,
                         QObject *parent = 0);

signals:
    void result(const QVariantMap &values);

private slots:
    virtual void slotFinished();
};

/**
 * @brief The MkColJob class
 */
class MkColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit MkColJob(Account *account, const QString &path, QObject *parent = 0);

signals:
    void finished(QNetworkReply::NetworkError);

private slots:
    virtual void slotFinished();
};

/**
 * @brief The CheckOwncloudJob class
 */
class CheckServerJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckServerJob(Account *account, bool followRedirect = false, QObject *parent = 0);

    static QString version(const QVariantMap &info);
    static QString versionString(const QVariantMap &info);
    static bool installed(const QVariantMap &info);

signals:
    void instanceFound(const QUrl&url, const QVariantMap &info);

private slots:
    virtual void slotFinished();

private:
    bool _followRedirects;
    bool _redirectCount;
};


/**
 * @brief The RequestEtagJob class
 */
class RequestEtagJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit RequestEtagJob(Account *account, const QString &path, QObject *parent = 0);

signals:
    void etagRetreived(const QString &etag);

private slots:
    virtual void slotFinished();
};

} // namespace Mirall

#endif // NETWORKJOBS_H
