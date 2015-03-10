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

#include "owncloudlib.h"
#include <QObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QTimer>
#include "accountfwd.h"

class QUrl;

namespace OCC {

class AbstractSslErrorHandler;


/**
 * @brief Internal Helper class
 */
class NetworkJobTimeoutPauser {
public:
    NetworkJobTimeoutPauser(QNetworkReply *reply);
    ~NetworkJobTimeoutPauser();
private:
    QPointer<QTimer> _timer;
};

/**
 * @brief The AbstractNetworkJob class
 */
class OWNCLOUDSYNC_EXPORT AbstractNetworkJob : public QObject {
    Q_OBJECT
public:
    explicit AbstractNetworkJob(AccountPtr account, const QString &path, QObject* parent = 0);
    virtual ~AbstractNetworkJob();

    virtual void start();

    AccountPtr account() const { return _account; }

    void setPath(const QString &path);
    QString path() const { return _path; }

    void setReply(QNetworkReply *reply);
    QNetworkReply* reply() const { return _reply; }

    void setIgnoreCredentialFailure(bool ignore);
    bool ignoreCredentialFailure() const { return _ignoreCredentialFailure; }

    QByteArray responseTimestamp();
    quint64 duration();

public slots:
    void setTimeout(qint64 msec);
    void resetTimeout();
signals:
    void networkError(QNetworkReply *reply);
    void networkActivity();
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
    virtual bool finished() = 0;
    QByteArray    _responseTimestamp;
    QElapsedTimer _durationTimer;
    quint64       _duration;
    bool          _timedout;  // set to true when the timeout slot is recieved
    bool          _followRedirects;

private slots:
    void slotFinished();
    virtual void slotTimeout();

private:
    QNetworkReply* addTimer(QNetworkReply *reply);
    bool _ignoreCredentialFailure;
    QPointer<QNetworkReply> _reply; // (QPointer because the NetworkManager may be destroyed before the jobs at exit)
    AccountPtr _account;
    QString _path;
    QTimer _timer;
    int _redirectCount;
};

/**
 * @brief The EntityExistsJob class
 */
class OWNCLOUDSYNC_EXPORT EntityExistsJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit EntityExistsJob(AccountPtr account, const QString &path, QObject* parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void exists(QNetworkReply*);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief The LsColJob class
 */
class OWNCLOUDSYNC_EXPORT LsColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit LsColJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;
    QHash<QString, qint64> _sizes;

    /**
     * Used to specify which properties shall be retrieved.
     *
     * The properties can
     *  - contain no colon: they refer to a property in the DAV: namespace
     *  - contain a colon: and thus specify an explicit namespace,
     *    e.g. "ns:with:colons:bar", which is "bar" in the "ns:with:colons" namespace
     */
    void setProperties(QList<QByteArray> properties);
    QList<QByteArray> properties() const;

signals:
    void directoryListingSubfolders(const QStringList &items);
    void directoryListingIterated(const QString &name, const QMap<QString,QString> &properties);
    void finishedWithError(QNetworkReply *reply);
    void finishedWithoutError();

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QList<QByteArray> _properties;
};

/**
 * @brief The PropfindJob class
 *
 * Setting the desired properties with setProperties() is mandatory.
 *
 * Note that this job is only for querying one item.
 * There is also the LsColJob which can be used to list collections
 */
class OWNCLOUDSYNC_EXPORT PropfindJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit PropfindJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

    /**
     * Used to specify which properties shall be retrieved.
     *
     * The properties can
     *  - contain no colon: they refer to a property in the DAV: namespace
     *  - contain a colon: and thus specify an explicit namespace,
     *    e.g. "ns:with:colons:bar", which is "bar" in the "ns:with:colons" namespace
     */
    void setProperties(QList<QByteArray> properties);
    QList<QByteArray> properties() const;

signals:
    void result(const QVariantMap &values);
    void finishedWithError();

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QList<QByteArray> _properties;
};

/**
 * @brief The MkColJob class
 */
class OWNCLOUDSYNC_EXPORT MkColJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit MkColJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void finished(QNetworkReply::NetworkError);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief The CheckServerJob class
 */
class OWNCLOUDSYNC_EXPORT CheckServerJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckServerJob(AccountPtr account, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

    static QString version(const QVariantMap &info);
    static QString versionString(const QVariantMap &info);
    static bool installed(const QVariantMap &info);

signals:
    void instanceFound(const QUrl&url, const QVariantMap &info);
    void instanceNotFound(QNetworkReply *reply);
    void timeout(const QUrl&url);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
    virtual void slotTimeout() Q_DECL_OVERRIDE;

private:
    bool _subdirFallback;
};


/**
 * @brief The RequestEtagJob class
 */
class OWNCLOUDSYNC_EXPORT RequestEtagJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit RequestEtagJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void etagRetreived(const QString &etag);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief The CheckQuota class
 */
class OWNCLOUDSYNC_EXPORT CheckQuotaJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit CheckQuotaJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void quotaRetrieved(qint64 totalBytes, qint64 availableBytes);

private slots:
    /** Return true if you want the job to be deleted after this slot has finished running. */
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief Job to check an API that return JSON
 *
 * Note! you need to be in the connected state before calling this because of a server bug:
 * https://github.com/owncloud/core/issues/12930
 *
 * To be used like this
 * _job = new JsonApiJob(account, QLatin1String("ocs/v1.php/foo/bar"), this);
 * connect(job, SIGNAL(jsonRecieved(QVariantMap)), ...)
 * The recieved QVariantMap is empty in case of error  or otherwise is a map as parsed by QtJson
 *
 */
class OWNCLOUDSYNC_EXPORT JsonApiJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit JsonApiJob(const AccountPtr &account, const QString &path, QObject *parent = 0);
public slots:
    void start() Q_DECL_OVERRIDE;
protected:
    bool finished() Q_DECL_OVERRIDE;
signals:
    void jsonRecieved(const QVariantMap &json);
};


} // namespace OCC

#endif // NETWORKJOBS_H
