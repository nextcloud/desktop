/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocssharingjob.h"

#include <QLoggingCategory>
#include <QJsonDocument>

#include "share.h"

Q_LOGGING_CATEGORY(lcOcsSharingJob, "nextcloud.gui.sharing.ocssharingjob", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC;
using namespace OCC::Gui::Sharing;

namespace
{
    constexpr auto SHARING_V1_BASE = "/ocs/v2.php/apps/sharing/api/v1"_L1;

    constexpr auto SOURCE_TYPE_NODE = "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1;
}

OcsSharingJob::OcsSharingJob(AccountPtr account, const QString &shareId)
    : OcsJob{account}
    , _shareId{shareId}
{
}

void OcsSharingJob::createShare()
{
    setPath(SHARING_V1_BASE % "/share");
    setVerb("POST"_ba);
    addPassStatusCode(201);

    connect(this, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcOcsSharingJob) << "share received with status" << statusCode << json;

        auto share = Share::fromJson(json, _account);

        Q_EMIT shareCreated(share);
    });

    start();
}

void OcsSharingJob::addSource(const QString &fileId)
{
    if (_shareId.isEmpty()) {
        qCWarning(lcOcsSharingJob) << "addSource called without a shareId, not starting job";
        Q_EMIT jobFinished({}, 0);
        return;
    }

    setPath(SHARING_V1_BASE % "/share/%1/source"_L1.arg(_shareId));
    setVerb("POST"_ba);

    addParam("class"_L1, SOURCE_TYPE_NODE);
    addParam("value"_L1, fileId);

    start();
}

void OcsSharingJob::addRecipient(const QString &recipientType, const QString &recipientValue)
{
    if (_shareId.isEmpty()) {
        qCWarning(lcOcsSharingJob) << "addRecipient called without a shareId, not starting job";
        Q_EMIT jobFinished({}, 0);
        return;
    }

    setPath(SHARING_V1_BASE % "/share/%1/recipient"_L1.arg(_shareId));
    setVerb("POST"_ba);

    addParam("class"_L1, recipientType);
    addParam("value"_L1, recipientValue);

    start();
}

void OcsSharingJob::searchRecipients(const QString &query, int64_t offset, int64_t limit)
{
    setPath(SHARING_V1_BASE % "/recipients");
    setVerb("GET"_ba);

    addParam("query"_L1, query);
    addParam("offset"_L1, QString::number(offset));
    addParam("limit"_L1, QString::number(limit));

    start();
}

void OcsSharingJob::setPermission(const QString &permissionClass, bool enabled)
{
    if (_shareId.isEmpty()) {
        qCWarning(lcOcsSharingJob) << "setPermission called without a shareId, not starting job";
        Q_EMIT jobFinished({}, 0);
        return;
    }

    setPath(SHARING_V1_BASE % "/share/%1/enabled"_L1.arg(_shareId));
    setVerb("PUT"_ba);

    addParam("class"_L1, permissionClass);
    addParam("enabled"_L1, enabled ? "true"_L1 : "false"_L1);

    start();
}
