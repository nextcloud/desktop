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

class Account;

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
    QNetworkReply* takeReply(); // for redirect handling

signals:
    void networkError(QNetworkReply::NetworkError, const QString& errorString);

protected:
    void setupConnections(QNetworkReply *reply);
    QNetworkReply* davRequest(const QByteArray& verb, const QString &relPath,
                              QNetworkRequest req = QNetworkRequest(),
                              QIODevice *data = 0);
    QNetworkReply* getRequest(const QString &relPath);

private slots:
    virtual void slotFinished() = 0;
    void slotError();

private:
    QNetworkReply *_reply;
    Account *_account;
    QString _path;
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
    void networkError();

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
    void finished();
    void networkError();

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
    explicit RequestEtagJob(Account *account, const QString &path, QObject *parent = 0);

signals:
    void etagRetreived(const QString &etag);

private slots:
    virtual void slotFinished();
};

} // namespace Mirall

#endif // NETWORKJOBS_H
