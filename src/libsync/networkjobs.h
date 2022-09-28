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
#include "common/result.h"
#include <QJsonObject>
#include <QUrlQuery>
#include <functional>

class QUrl;

namespace OCC {

/** Strips quotes and gzip annotations */
OWNCLOUDSYNC_EXPORT QByteArray parseEtag(const QByteArray &header);

struct HttpError
{
    int code; // HTTP error code
    QString message;
};

template <typename T>
using HttpResult = Result<T, HttpError>;

/**
 * @brief The EntityExistsJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT EntityExistsJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit EntityExistsJob(AccountPtr account, const QUrl &rootUrl, const QString &path, QObject *parent = nullptr);
    void start() override;

signals:
    void exists(QNetworkReply *);

private slots:
    void finished() override;
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
    explicit LsColJob(AccountPtr account, const QUrl &url, const QString &path, QObject *parent = nullptr);
    void start() override;

    /**
     * Used to specify which properties shall be retrieved.
     *
     * The properties can
     *  - contain no colon: they refer to a property in the DAV: namespace
     *  - contain a colon: and thus specify an explicit namespace,
     *    e.g. "ns:with:colons:bar", which is "bar" in the "ns:with:colons" namespace
     */
    void setProperties(const QList<QByteArray> &properties);
    QList<QByteArray> properties() const;

    const QHash<QString, qint64> &sizes() const;

signals:
    void directoryListingSubfolders(const QStringList &items);
    void directoryListingIterated(const QString &name, const QMap<QString, QString> &properties);
    void finishedWithError(QNetworkReply *reply);
    void finishedWithoutError();

private slots:
    void finished() override;

protected:
    void startImpl(const QNetworkRequest &req);

private:
    QList<QByteArray> _properties;
    QHash<QString, qint64> _sizes;
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
class OWNCLOUDSYNC_EXPORT PropfindJob : public LsColJob
{
    Q_OBJECT
public:
    using LsColJob::LsColJob;
    void start() override;

signals:
    void result(const QMap<QString, QString> &values);

private:
    bool _done = false;
};

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
    explicit AvatarJob(AccountPtr account, const QString &userId, int size, QObject *parent = nullptr);

    void start() override;

    /** The retrieved avatar images don't have the circle shape by default */
    static QPixmap makeCircularAvatar(const QPixmap &baseAvatar);

signals:
    /**
     * @brief avatarPixmap - returns either a valid pixmap or not.
     */

    void avatarPixmap(const QPixmap &);

private slots:
    void finished() override;
};

/**
 * @brief The MkColJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT MkColJob : public AbstractNetworkJob
{
    Q_OBJECT
    HeaderMap _extraHeaders;

public:
    explicit MkColJob(AccountPtr account, const QUrl &url, const QString &path,
        const HeaderMap &extraHeaders, QObject *parent = nullptr);
    void start() override;

signals:
    void finishedWithError(QNetworkReply *reply);
    void finishedWithoutError();

private:
    void finished() override;
};

/**
 * @brief The RequestEtagJob class
 */
class OWNCLOUDSYNC_EXPORT RequestEtagJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit RequestEtagJob(AccountPtr account, const QUrl &rootUrl, const QString &path, QObject *parent = nullptr);
    void start() override;

signals:
    void etagRetreived(const QByteArray &etag, const QDateTime &time);
    void finishedWithResult(const HttpResult<QByteArray> &etag);

private slots:
    void finished() override;
};

/**
 * @brief Checks with auth type to use for a server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT DetermineAuthTypeJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    enum class AuthType {
        Basic, // also the catch-all fallback for backwards compatibility reasons
        OAuth,
        Unknown
    };
    Q_ENUM(AuthType)

    explicit DetermineAuthTypeJob(AccountPtr account, QObject *parent = nullptr);
    void start() override;
signals:
    void authType(AuthType);

protected Q_SLOTS:
    void finished() override;
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
    using UrlQuery = QList<QPair<QString, QString>>;

    // fully qualified urls can be passed in the QNetworkRequest
    explicit SimpleNetworkJob(AccountPtr account, const QUrl &rootUrl, const QString &path, const QByteArray &verb, QIODevice *requestBody = nullptr, const QNetworkRequest &req = {}, QObject *parent = nullptr);
    explicit SimpleNetworkJob(AccountPtr account, const QUrl &rootUrl, const QString &path, const QByteArray &verb, const UrlQuery &arguments, const QNetworkRequest &req = {}, QObject *parent = nullptr);
    explicit SimpleNetworkJob(AccountPtr account, const QUrl &rootUrl, const QString &path, const QByteArray &verb, const QJsonObject &arguments, const QNetworkRequest &req = {}, QObject *parent = nullptr);
    explicit SimpleNetworkJob(AccountPtr account, const QUrl &rootUrl, const QString &path, const QByteArray &verb, QByteArray &&requestBody, const QNetworkRequest &req = {}, QObject *parent = nullptr);

    virtual ~SimpleNetworkJob();

    void start() override;

    void addNewReplyHook(std::function<void(QNetworkReply *)> &&hook);

protected:
    void finished() override;
    void newReplyHook(QNetworkReply *) override;

    QNetworkRequest _request;

private:
    explicit SimpleNetworkJob(AccountPtr account, const QUrl &rootUrl, const QString &path, const QByteArray &verb, const QNetworkRequest &req, QObject *parent);

    QByteArray _verb;
    QByteArray _body;
    QIODevice *_device = nullptr;
    std::vector<std::function<void(QNetworkReply *)>> _replyHooks;
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
void OWNCLOUDSYNC_EXPORT fetchPrivateLinkUrl(AccountPtr account, const QUrl &baseUrl, const QString &remotePath, QObject *target,
    const std::function<void(const QUrl &url)> &targetFun);

} // namespace OCC


#endif // NETWORKJOBS_H
