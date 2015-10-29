/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "share.h"
#include "ocssharejob.h"
#include "account.h"

#include <QUrl>

namespace OCC {

Share::Share(AccountPtr account, const QString& id, const QString& path, int shareType,
             int permissions, QObject *parent)
: QObject(parent),
  _account(account),
  _id(id),
  _path(path),
  _shareType(shareType),
  _permissions(permissions)
{

}

QString Share::getId() const
{
    return _id;
}

int Share::getShareType() const
{
    return _shareType;
}

int Share::getPermissions() const
{
    return _permissions;
}

void Share::deleteShare()
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotDeleted(QVariantMap)));
    job->deleteShare(getId());
}

void Share::slotDeleted(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        //emit error!
    }

    emit shareDeleted();
}

QUrl LinkShare::getLink() const
{
    return _url;
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
                     const QString& id,
                     const QString& path,
                     int shareType,
                     int permissions,
                     bool passwordSet,
                     const QUrl& url,
                     const QDate& expireDate,
                     QObject *parent)
: Share(account, id, path, shareType, permissions, parent),
  _passwordSet(passwordSet),
  _expireDate(expireDate),
  _url(url)
{

}

bool LinkShare::getPublicUpload()
{
    return ((_permissions & OcsShareJob::Permission::Update) &&
            (_permissions & OcsShareJob::Permission::Create));
}

void LinkShare::setPublicUpload(bool publicUpload)
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), this, SLOT(slotPublicUploadSet(QVariantMap, QVariant)));
    job->setPublicUpload(getId(), publicUpload);
}

void LinkShare::slotPublicUploadSet(const QVariantMap &reply, const QVariant &value)
{
    qDebug() << Q_FUNC_INFO << reply;
    qDebug() << Q_FUNC_INFO << value;

    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        //emit error
    }

    //TODO FIX permission with names
    if (value.toBool()) {
        _permissions = 7;
    } else {
        _permissions = 1;
    }

    emit publicUploadSet();
}

void LinkShare::setPassword(const QString &password)
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), this, SLOT(slotPasswordSet(QVariantMap, QVariant)));
    job->setPassword(getId(), password);
}

void LinkShare::slotPasswordSet(const QVariantMap &reply, const QVariant &value)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        //emit error
    }

    _passwordSet = value.toString() == "";
    emit passwordSet();
}

void LinkShare::setExpireDate(const QDate &date)
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), this, SLOT(slotExpireDateSet(QVariantMap, QVariant)));
    job->setExpireDate(getId(), date);
}

void LinkShare::slotExpireDateSet(const QVariantMap &reply, const QVariant &value)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        //emit error
    }

    _expireDate = value.toDate();
    emit expireDateSet();
}

ShareManager::ShareManager(AccountPtr account, QObject *parent)
: QObject(parent),
  _account(account)
{

}

void ShareManager::createLinkShare(const QString &path,
                                   const QString &password)
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotLinkShareCreated(QVariantMap)));
    job->createShare(path, OcsShareJob::ShareType::Link, password);
}

void ShareManager::slotLinkShareCreated(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);

    /*
     * Before we had decent sharing capabilities on the server a 403 "generally"
     * meant that a share was password protected
     */
    if (code == 403) {
        emit linkShareRequiresPassword();
        return;
    } else if (code != 100) {
        //emit error
    }

    //Parse share
    auto data = reply.value("ocs").toMap().value("data").toMap();
    QSharedPointer<LinkShare> share(parseLinkShare(data));

    emit linkShareCreated(share);
}

void ShareManager::fetchShares(const QString &path)
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotSharesFetched(QVariantMap)));
    job->getShares(path);
}

void ShareManager::slotSharesFetched(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100 && code != 404) {
        //emit error!
    }

    auto tmpShares = reply.value("ocs").toMap().value("data").toList();
    const QString versionString = _account->serverVersion();
    qDebug() << Q_FUNC_INFO << versionString << "Fetched" << tmpShares.count() << "shares";

    QList<QSharedPointer<Share>> shares;

    foreach(const auto &share, tmpShares) {
        auto data = share.toMap();

        auto shareType = data.value("share_type").toInt();

        Share *newShare = NULL;

        if (shareType == OcsShareJob::ShareType::Link) {
            newShare = parseLinkShare(data);
        } else {
            newShare = new Share(_account,
                                 data.value("id").toString(),
                                 data.value("path").toString(),
                                 shareType,
                                 data.value("permissions").toInt(),
                                 this);
        }

        shares.append(QSharedPointer<Share>(newShare));    
    }

    qDebug() << Q_FUNC_INFO << "Sending " << shares.count() << "shares";
    emit sharesFetched(shares);
}

LinkShare *ShareManager::parseLinkShare(const QVariantMap &data) {
    QUrl url;

    // From ownCloud server 8.2 the url field is always set for public shares
    if (data.contains("url")) {
        url = QUrl(data.value("url").toString());
    } else if (_account->serverVersionInt() >= (8 << 16)) {
        // From ownCloud server version 8 on, a different share link scheme is used.
        url = QUrl(Account::concatUrlPath(_account->url(), QString("index.php/s/%1").arg(data.value("token").toString())).toString());
    } else {
        QList<QPair<QString, QString>> queryArgs;
        queryArgs.append(qMakePair(QString("service"), QString("files")));
        queryArgs.append(qMakePair(QString("t"), data.value("token").toString()));
        url = QUrl(Account::concatUrlPath(_account->url(), QLatin1String("public.php"), queryArgs).toString());
    }

    QDate expireDate;
    if (data.value("expiration").isValid()) {
       expireDate = QDate::fromString(data.value("expiration").toString(), "yyyy-MM-dd 00:00:00");
    }

    return new LinkShare(_account,
                         data.value("id").toString(),
                         data.value("path").toString(),
                         data.value("share_type").toInt(),
                         data.value("permissions").toInt(),
                         data.value("share_with").isValid(),
                         url,
                         expireDate,
                         this);
}

}
