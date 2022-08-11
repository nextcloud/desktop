/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
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

#include "sharemanager.h"
#include "ocssharejob.h"
#include "account.h"
#include "folderman.h"
#include "accountstate.h"

#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace OCC {

/**
 * When a share is modified, we need to tell the folders so they can adjust overlay icons
 */
static void updateFolder(const AccountPtr &account, const QString &path)
{
    for (auto *f : FolderMan::instance()->folders()) {
        if (f->accountState()->account() != account)
            continue;
        auto folderPath = f->remotePath();
        if (path.startsWith(folderPath) && (path == folderPath || folderPath.endsWith('/') || path[folderPath.size()] == '/')) {
            // Workaround the fact that the server does not invalidate the etags of parent directories
            // when something is shared.
            auto relative = path.midRef(f->remotePathTrailingSlash().length());
            f->journalDb()->schedulePathForRemoteDiscovery(relative.toString());

            // Schedule a sync so it can update the remote permission flag and let the socket API
            // know about the shared icon.
            f->scheduleThisFolderSoon();
        }
    }
}


Share::Share(AccountPtr account,
    const QString &id,
    const QString &path,
    const ShareType shareType,
    const Permissions permissions,
    const QSharedPointer<Sharee> shareWith)
    : _account(account)
    , _id(id)
    , _path(path)
    , _shareType(shareType)
    , _permissions(permissions)
    , _shareWith(shareWith)
{
}

AccountPtr Share::account() const
{
    return _account;
}

QString Share::path() const
{
    return _path;
}

QString Share::getId() const
{
    return _id;
}

Share::ShareType Share::getShareType() const
{
    return _shareType;
}

QSharedPointer<Sharee> Share::getShareWith() const
{
    return _shareWith;
}

void Share::setPermissions(Permissions permissions)
{
    auto *job = OcsShareJob::setPermissions(_account, this, getId(), permissions);
    connect(job, &JsonApiJob::finishedSignal, this, [job, permissions, this] {
        if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            _permissions = permissions;
            emit permissionsSet();
        }
    });
    job->start();
}

Share::Permissions Share::getPermissions() const
{
    return _permissions;
}

void Share::deleteShare()
{
    auto *job = OcsShareJob::deleteShare(_account, this, getId());
    connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            emit shareDeleted();
            updateFolder(_account, _path);
        }
    });
    job->start();
}

QUrl LinkShare::getLink() const
{
    return _url;
}

QUrl LinkShare::getDirectDownloadLink() const
{
    QUrl url = _url;
    url.setPath(url.path() + "/download");
    return url;
}

QDate LinkShare::getExpireDate() const
{
    return _expireDate;
}

bool LinkShare::isPasswordSet() const
{
    return _passwordSet;
}

LinkShare::LinkShare(AccountPtr account,
    const QString &id,
    const QString &path,
    const QString &name,
    const QString &token,
    Permissions permissions,
    bool passwordSet,
    const QUrl &url,
    const QDate &expireDate)
    : Share(account, id, path, Share::TypeLink, permissions)
    , _name(name)
    , _token(token)
    , _passwordSet(passwordSet)
    , _expireDate(expireDate)
    , _url(url)
{
}

bool LinkShare::getPublicUpload() const
{
    return _permissions & SharePermissionCreate;
}

bool LinkShare::getShowFileListing() const
{
    return _permissions & SharePermissionRead;
}

QString LinkShare::getName() const
{
    return _name;
}

void LinkShare::setName(const QString &name)
{
    auto *job = OcsShareJob::setName(_account, this, getId(), name);
    connect(job, &JsonApiJob::finishedSignal, this, [job, name, this] {
        if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            _name = name;
            emit nameSet();
        }
    });
    job->start();
}

QString LinkShare::getToken() const
{
    return _token;
}

void LinkShare::setPassword(const QString &password)
{
    auto *job = OcsShareJob::setPassword(_account, this, getId(), password);
    connect(job, &JsonApiJob::finishedSignal, this, [job, password, this] {
        if (!job->ocsSuccess()) {
            emit passwordSetError(job->ocsStatus(), job->ocsMessage());
        } else {
            _passwordSet = !password.isEmpty();
            emit passwordSet();
        }
    });
    job->start();
}

void LinkShare::setExpireDate(const QDate &date)
{
    auto *job = OcsShareJob::setExpireDate(_account, this, getId(), date);
    connect(job, &JsonApiJob::finishedSignal, this, [job, date, this] {
        if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            auto data = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
            /*
             * If the reply provides a data back (more REST style)
             * they use this date.
             */
            if (data.value(QStringLiteral("expiration")).isString()) {
                _expireDate = QDate::fromString(data.value(QStringLiteral("expiration")).toString(), QStringLiteral("yyyy-MM-dd 00:00:00"));
            } else {
                _expireDate = date;
            }
            emit expireDateSet();
        }
    });
    job->start();
}

ShareManager::ShareManager(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

void ShareManager::createLinkShare(const QString &path,
    const QString &name,
    const QString &password,
    const QDate &expireDate,
    const Share::Permissions permissions)
{
    auto *job = OcsShareJob::createLinkShare(_account, this, path, name, password, expireDate, permissions);
    connect(job, &JsonApiJob::finishedSignal, this, [job, password, this] {
        if (job->ocsStatus() == 403) {
            // A 403 generally means some of the settings for the share are not allowed.
            // Maybe a password is required, or the expire date isn't acceptable.
            emit linkShareCreationForbidden(job->ocsMessage());
        } else if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            // Parse share
            auto data = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
            QSharedPointer<LinkShare> share(parseLinkShare(data));

            emit linkShareCreated(share);

            updateFolder(_account, share->path());
        }
    });
    job->start();
}

void ShareManager::createShare(const QString &path,
    const Share::ShareType shareType,
    const QString &shareWith,
    const Share::Permissions desiredPermissions)
{
    auto *job = OcsShareJob::getSharedWithMe(_account, this);
    connect(job, &JsonApiJob::finishedSignal, this, [=] {
        if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            // Note: The following code attempts to determine if the item was shared with
            // the user and what the permissions were. It doesn't do a good job at it since
            // the == path comparison will mean it doesn't work for subitems of shared
            // folders. Also, it's nicer if the calling code determines the share-permissions
            // (see maxSharingPermissions) via a PropFind and passes in valid permissions.
            // Remove this code for >= 2.7.0.
            // TODO: sigh

            // Find existing share permissions (if this was shared with us)
            Share::Permissions existingPermissions = SharePermissionDefault;
            const auto &array = job->data()[QLatin1String("ocs")].toObject()[QLatin1String("data")].toArray();
            for (const auto &element : array) {
                auto map = element.toObject();
                if (map[QStringLiteral("file_target")] == path)
                    existingPermissions = Share::Permissions(map[QStringLiteral("permissions")].toInt());
            }

            // Limit the permissions we request for a share to the ones the item
            // was shared with initially.
            auto validPermissions = desiredPermissions;
            if (validPermissions == SharePermissionDefault) {
                validPermissions = existingPermissions;
            }
            if (existingPermissions != SharePermissionDefault) {
                validPermissions &= existingPermissions;
            }

            auto *job2 = OcsShareJob::createShare(_account, this, path, shareType, shareWith, validPermissions);
            connect(job2, &JsonApiJob::finishedSignal, this, [job2, this] {
            if (!job2->ocsSuccess()) {
                emit serverError(job2->ocsStatus(), job2->ocsMessage());
            } else {
                //Parse share
                auto data = job2->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
                QSharedPointer<Share> share(parseShare(data));

                emit shareCreated(share);
                updateFolder(_account, share->path());

            } });
            job2->start();
        }
    });
    job->start();
}

void ShareManager::fetchShares(const QString &path)
{
    auto *job = OcsShareJob::getShares(_account, this, path);
    connect(job, &JsonApiJob::finishedSignal, this, [job, path, this] {
        // 404 seems to be ok according to refactored code
        if (job->ocsStatus() == 404) {
            emit sharesFetched({});
        } else if (!job->ocsSuccess()) {
            emit serverError(job->ocsStatus(), job->ocsMessage());
        } else {
            const auto &tmpShares = job->data().value(QLatin1String("ocs")).toObject().value(QLatin1String("data")).toArray();
            qCDebug(lcSharing) << "Fetched" << tmpShares.count() << "shares";

            QList<QSharedPointer<Share>> shares;

            for (const auto &share : tmpShares) {
                auto data = share.toObject();

                auto shareType = data.value(QStringLiteral("share_type")).toInt();

                QSharedPointer<Share> newShare;

                if (shareType == Share::TypeLink) {
                    newShare = parseLinkShare(data);
                } else {
                    newShare = parseShare(data);
                }
                shares.append(QSharedPointer<Share>(newShare));
            }
            qCDebug(lcSharing) << "Sending " << shares.count() << "shares";
            emit sharesFetched(shares);
        }
    });
    job->start();
}

QSharedPointer<LinkShare> ShareManager::parseLinkShare(const QJsonObject &data)
{
    QUrl url;

    // From ownCloud server 8.2 the url field is always set for public shares
    if (data.contains(QStringLiteral("url"))) {
        url = QUrl(data.value(QStringLiteral("url")).toString());
    } else {
        // From ownCloud server version 8 on, a different share link scheme is used.
        url = QUrl(Utility::concatUrlPath(_account->url(), QLatin1String("index.php/s/") + data.value(QStringLiteral("token")).toString())).toString();
    }

    QDate expireDate;
    if (data.value(QStringLiteral("expiration")).isString()) {
        expireDate = QDate::fromString(data.value(QStringLiteral("expiration")).toString(), QStringLiteral("yyyy-MM-dd 00:00:00"));
    }

    return QSharedPointer<LinkShare>(new LinkShare(_account,
        data.value(QStringLiteral("id")).toVariant().toString(), // "id" used to be an integer, support both
        data.value(QStringLiteral("path")).toString(),
        data.value(QStringLiteral("name")).toString(),
        data.value(QStringLiteral("token")).toString(),
        (Share::Permissions)data.value(QStringLiteral("permissions")).toInt(),
        data.value(QStringLiteral("share_with")).isString(), // has password?
        url,
        expireDate));
}

QSharedPointer<Share> ShareManager::parseShare(const QJsonObject &data)
{
    QSharedPointer<Sharee> sharee(new Sharee(data.value(QStringLiteral("share_with")).toString(),
        data.value(QStringLiteral("share_with_displayname")).toString(),
        (Sharee::Type)data.value(QStringLiteral("share_type")).toInt()));

    return QSharedPointer<Share>(new Share(_account,
        data.value(QStringLiteral("id")).toVariant().toString(), // "id" used to be an integer, support both
        data.value(QStringLiteral("path")).toString(),
        (Share::ShareType)data.value(QStringLiteral("share_type")).toInt(),
        (Share::Permissions)data.value(QStringLiteral("permissions")).toInt(),
        sharee));
}
}
