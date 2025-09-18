/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocssharejob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>
#include <QJsonDocument>

namespace OCC {

OcsShareJob::OcsShareJob(AccountPtr account)
    : OcsJob(account)
{
    setPath(_pathForSharesRequest);
    connect(this, &OcsJob::jobFinished, this, &OcsShareJob::jobDone);
}

void OcsShareJob::getShares(const QString &path, const QMap<QString, QString> &params)
{
    setVerb("GET");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("reshares"), QStringLiteral("true"));

    for (auto it = std::cbegin(params); it != std::cend(params); ++it) {
        addParam(it.key(), it.value());
    }

    addPassStatusCode(404);

    start();
}

void OcsShareJob::deleteShare(const QString &shareId)
{
    appendPath(shareId);
    setVerb("DELETE");

    start();
}

void OcsShareJob::setExpireDate(const QString &shareId, const QDate &date)
{
    appendPath(shareId);
    setVerb("PUT");

    if (date.isValid()) {
        addParam(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd"));
    } else {
        addParam(QString::fromLatin1("expireDate"), QString());
    }
    _value = date;

    start();
}

void OcsShareJob::setPassword(const QString &shareId, const QString &password)
{
    appendPath(shareId);
    setVerb("PUT");

    addParam(QString::fromLatin1("password"), password);
    _value = password;

    start();
}

void OcsShareJob::setNote(const QString &shareId, const QString &note)
{
    appendPath(shareId);
    setVerb("PUT");

    addParam(QString::fromLatin1("note"), note);
    _value = note;

    start();
}

void OcsShareJob::setPublicUpload(const QString &shareId, bool publicUpload)
{
    appendPath(shareId);
    setVerb("PUT");

    const QString value = QString::fromLatin1(publicUpload ? "true" : "false");
    addParam(QString::fromLatin1("publicUpload"), value);
    _value = publicUpload;

    start();
}

void OcsShareJob::setName(const QString &shareId, const QString &name)
{
    appendPath(shareId);
    setVerb("PUT");
    addParam(QString::fromLatin1("name"), name);
    _value = name;

    start();
}

void OcsShareJob::setPermissions(const QString &shareId,
    const Share::Permissions permissions)
{
    appendPath(shareId);
    setVerb("PUT");

    addParam(QString::fromLatin1("permissions"), QString::number(permissions));
    _value = (int)permissions;

    start();
}

void OcsShareJob::setLabel(const QString &shareId, const QString &label)
{
    appendPath(shareId);
    setVerb("PUT");
    
    addParam(QStringLiteral("label"), label);
    _value = label;
    
    start();
}

void OcsShareJob::setHideDownload(const QString &shareId, const bool hideDownload)
{
    appendPath(shareId);
    setVerb("PUT");

    const auto value = QString::fromLatin1(hideDownload ? QByteArrayLiteral("true") : QByteArrayLiteral("false"));
    addParam(QStringLiteral("hideDownload"), value);
    _value = hideDownload;

    start();
}

void OcsShareJob::createLinkShare(const QString &path,
    const QString &name,
    const QString &password)
{
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(Share::TypeLink));

    if (!name.isEmpty()) {
        addParam(QString::fromLatin1("name"), name);
    }
    if (!password.isEmpty()) {
        addParam(QString::fromLatin1("password"), password);
    }

    addPassStatusCode(403);

    start();
}

void OcsShareJob::createSecureFileDropLinkShare(const QString &path, const QString &name, const QString &password)
{
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(Share::TypeLink));
    addParam(QString::fromLatin1("permissions"), QString::number(4));

    if (!name.isEmpty()) {
        addParam(QString::fromLatin1("name"), name);
    }
    if (!password.isEmpty()) {
        addParam(QString::fromLatin1("password"), password);
    }

    addPassStatusCode(403);

    start();
}

void OcsShareJob::createShare(const QString &path,
    const Share::ShareType shareType,
    const QString &shareWith,
    const Share::Permissions permissions,
    const QString &password)
{
    Q_UNUSED(permissions)
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(shareType));
    addParam(QString::fromLatin1("shareWith"), shareWith);

    if (!password.isEmpty()) {
        addParam(QString::fromLatin1("password"), password);
    }

    start();
}

void OcsShareJob::getSharedWithMe(const QString &path)
{
    setVerb("GET");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shared_with_me"), QStringLiteral("true"));

    start();
}

void OcsShareJob::jobDone(QJsonDocument reply)
{
    emit shareJobFinished(reply, _value);
}

QString const OcsShareJob::_pathForSharesRequest = QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares");
}
