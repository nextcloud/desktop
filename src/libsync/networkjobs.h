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

#include "abstractnetworkjob.h"

#include <functional>

class QUrl;
class QJsonObject;

namespace OCC {

/**
 * @brief The EntityExistsJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT EntityExistsJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit EntityExistsJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void exists(QNetworkReply *);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief The LsColJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT LsColXMLParser : public QObject
{
    Q_OBJECT
public:
    explicit LsColXMLParser();

    bool parse(const QByteArray &xml, QHash<QString, qint64> *sizes, const QString &expectedPath);

signals:
    void directoryListingSubfolders(const QStringList &items);
    void directoryListingIterated(const QString &name, const QMap<QString, QString> &properties);
    void finishedWithError(QNetworkReply *reply);
    void finishedWithoutError();
};

class OWNCLOUDSYNC_EXPORT LsColJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit LsColJob(AccountPtr account, const QString &path, QObject *parent = 0);
    explicit LsColJob(AccountPtr account, const QUrl &url, QObject *parent = 0);
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
    void directoryListingIterated(const QString &name, const QMap<QString, QString> &properties);
    void finishedWithError(QNetworkReply *reply);
    void finishedWithoutError();

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QList<QByteArray> _properties;
    QUrl _url; // Used instead of path() if the url is specified in the constructor
};

/**
 * @brief The PropfindJob class
 *
 * Setting the desired properties with setProperties() is mandatory.
 *
 * Note that this job is only for querying one item.
 * There is also the LsColJob which can be used to list collections
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT PropfindJob : public AbstractNetworkJob
{
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
    void finishedWithError(QNetworkReply *reply = 0);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QList<QByteArray> _properties;
};

#ifndef TOKEN_AUTH_ONLY
/**
 * @brief Retrieves the account users avatar from the server using a GET request.
 *
 * If the server does not have the avatar, the result Pixmap is empty.
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT AvatarJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    /**
     * @param userId The user for which to obtain the avatar
     * @param size The size of the avatar (square so size*size)
     */
    explicit AvatarJob(AccountPtr account, const QString &userId, int size, QObject *parent = 0);

    void start() Q_DECL_OVERRIDE;

    /** The retrieved avatar images don't have the circle shape by default */
    static QImage makeCircularAvatar(const QImage &baseAvatar);

signals:
    /**
     * @brief avatarPixmap - returns either a valid pixmap or not.
     */

    void avatarPixmap(const QImage &);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QUrl _avatarUrl;
};
#endif

/**
 * @brief Send a Proppatch request
 *
 * Setting the desired properties with setProperties() is mandatory.
 *
 * WARNING: Untested!
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ProppatchJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit ProppatchJob(AccountPtr account, const QString &path, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

    /**
     * Used to specify which properties shall be set.
     *
     * The property keys can
     *  - contain no colon: they refer to a property in the DAV: namespace
     *  - contain a colon: and thus specify an explicit namespace,
     *    e.g. "ns:with:colons:bar", which is "bar" in the "ns:with:colons" namespace
     */
    void setProperties(QMap<QByteArray, QByteArray> properties);
    QMap<QByteArray, QByteArray> properties() const;

signals:
    void success();
    void finishedWithError();

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QMap<QByteArray, QByteArray> _properties;
};

/**
 * @brief The MkColJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT MkColJob : public AbstractNetworkJob
{
    Q_OBJECT
    QUrl _url; // Only used if the constructor taking a url is taken.
    QMap<QByteArray, QByteArray> _extraHeaders;

public:
    explicit MkColJob(AccountPtr account, const QString &path, QObject *parent = 0);
    explicit MkColJob(AccountPtr account, const QUrl &url,
        const QMap<QByteArray, QByteArray> &extraHeaders, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

signals:
    void finished(QNetworkReply::NetworkError);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief The CheckServerJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT CheckServerJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit CheckServerJob(AccountPtr account, QObject *parent = 0);
    void start() Q_DECL_OVERRIDE;

    static QString version(const QJsonObject &info);
    static QString versionString(const QJsonObject &info);
    static bool installed(const QJsonObject &info);

signals:
    /** Emitted when a status.php was successfully read.
     *
     * \a url see _serverStatusUrl (does not include "/status.php")
     * \a info The status.php reply information
     */
    void instanceFound(const QUrl &url, const QJsonObject &info);

    /** Emitted on invalid status.php reply.
     *
     * \a reply is never null
     */
    void instanceNotFound(QNetworkReply *reply);

    /** A timeout occurred.
     *
     * \a url The specific url where the timeout happened.
     */
    void timeout(const QUrl &url);

private:
    bool finished() Q_DECL_OVERRIDE;
    void onTimedOut() Q_DECL_OVERRIDE;
private slots:
    virtual void metaDataChangedSlot();
    virtual void encryptedSlot();
    void slotRedirected(QNetworkReply *reply, const QUrl &targetUrl, int redirectCount);

private:
    bool _subdirFallback;

    /** The permanent-redirect adjusted account url.
     *
     * Note that temporary redirects or a permanent redirect behind a temporary
     * one do not affect this url.
     */
    QUrl _serverUrl;

    /** Keep track of how many permanent redirect were applied. */
    int _permanentRedirects;
};


/**
 * @brief The RequestEtagJob class
 */
class OWNCLOUDSYNC_EXPORT RequestEtagJob : public AbstractNetworkJob
{
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
 * @brief Job to check an API that return JSON
 *
 * Note! you need to be in the connected state before calling this because of a server bug:
 * https://github.com/owncloud/core/issues/12930
 *
 * To be used like this:
 * \code
 * _job = new JsonApiJob(account, QLatin1String("ocs/v1.php/foo/bar"), this);
 * connect(job, SIGNAL(jsonReceived(QJsonDocument)), ...)
 * The received QVariantMap is null in case of error
 * \encode
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT JsonApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit JsonApiJob(const AccountPtr &account, const QString &path, QObject *parent = 0);

    /**
     * @brief addQueryParams - add more parameters to the ocs call
     * @param params: list pairs of strings containing the parameter name and the value.
     *
     * All parameters from the passed list are appended to the query. Note
     * that the format=json parameter is added automatically and does not
     * need to be set this way.
     *
     * This function needs to be called before start() obviously.
     */
    void addQueryParams(QList<QPair<QString, QString>> params);

public slots:
    void start() Q_DECL_OVERRIDE;

protected:
    bool finished() Q_DECL_OVERRIDE;
signals:

    /**
     * @brief jsonReceived - signal to report the json answer from ocs
     * @param json - the parsed json document
     * @param statusCode - the OCS status code: 100 (!) for success
     */
    void jsonReceived(const QJsonDocument &json, int statusCode);

private:
    QList<QPair<QString, QString>> _additionalParams;
};

/**
 * @brief Checks with auth type to use for a server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT DetermineAuthTypeJob : public QObject
{
    Q_OBJECT
public:
    enum AuthType {
        Basic, // also the catch-all fallback for backwards compatibility reasons
        OAuth,
        Shibboleth
    };

    explicit DetermineAuthTypeJob(AccountPtr account, QObject *parent = 0);
    void start();
signals:
    void authType(AuthType);

private:
    void checkBothDone();

    AccountPtr _account;
    AuthType _resultGet = Basic;
    AuthType _resultPropfind = Basic;
    bool _getDone = false;
    bool _propfindDone = false;
};

/**
 * @brief A basic job around a network request without extra funtionality
 * @ingroup libsync
 *
 * Primarily adds timeout and redirection handling.
 */
class OWNCLOUDSYNC_EXPORT SimpleNetworkJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit SimpleNetworkJob(AccountPtr account, QObject *parent = 0);

    QNetworkReply *startRequest(const QByteArray &verb, const QUrl &url,
        QNetworkRequest req = QNetworkRequest(),
        QIODevice *requestBody = 0);

signals:
    void finishedSignal(QNetworkReply *reply);
private slots:
    bool finished() Q_DECL_OVERRIDE;
};

/**
 * @brief Runs a PROPFIND to figure out the private link url
 *
 * The numericFileId is used only to build the deprecatedPrivateLinkUrl
 * locally as a fallback. If it's empty and the PROPFIND fails, targetFun
 * will be called with an empty string.
 *
 * The job and signal connections are parented to the target QObject.
 *
 * Note: targetFun is guaranteed to be called only through the event
 * loop and never directly.
 */
void OWNCLOUDSYNC_EXPORT fetchPrivateLinkUrl(
    AccountPtr account, const QString &remotePath,
    const QByteArray &numericFileId, QObject *target,
    std::function<void(const QString &url)> targetFun);

} // namespace OCC

#endif // NETWORKJOBS_H
