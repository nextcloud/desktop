/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsshareejob.h"

#include <QJsonDocument>

namespace OCC {

OcsShareeJob::OcsShareeJob(AccountPtr account)
    : OcsJob(account)
{
    setPath("ocs/v2.php/apps/files_sharing/api/v1/sharees");
    connect(this, &OcsJob::jobFinished, this, &OcsShareeJob::jobDone);
}

void OcsShareeJob::getSharees(const QString &search,
    const QString &itemType,
    int page,
    int perPage,
    bool lookup)
{
    setVerb("GET");

    addParam(QString::fromLatin1("search"), search);
    addParam(QString::fromLatin1("itemType"), itemType);
    addParam(QString::fromLatin1("page"), QString::number(page));
    addParam(QString::fromLatin1("perPage"), QString::number(perPage));
    addParam(QString::fromLatin1("lookup"), QVariant(lookup).toString());

    start();
}

void OcsShareeJob::jobDone(const QJsonDocument &reply)
{
    emit shareeJobFinished(reply);
}
}
