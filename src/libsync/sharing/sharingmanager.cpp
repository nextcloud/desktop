/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmanager.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "networkjobs.h"
#include "share.h"

using namespace Qt::StringLiterals;

using namespace OCC::Sharing;

const QLatin1String SharingManager::SOURCE_TYPE_NODE = "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1;

namespace
{
    constexpr auto SHARING_V1_BASE = "/ocs/v2.php/apps/sharing/api/v1"_L1;
}

SharingManager::SharingManager(AccountPtr account, QObject *parent)
    : QObject{parent}
    , _account{account}
{

}

void SharingManager::updateFromCapabilities(const QVariantMap &capabilities)
{
    if (const auto property = capabilities.value("sharing"_L1); !property.isValid()) {
        // no sharing capability present, we're done here
        setAvailable(false);
        return;
    }
    setAvailable(true);

    const auto capability = capabilities.value("sharing"_L1).toMap();

    if (const auto property = capability.value("api_versions"_L1); property.isValid() && property.canConvert<QStringList>()) {
        _apiVersions = property.value<QStringList>();
    }
}

bool SharingManager::isAvailable() const
{
    return _available;
}

void SharingManager::setAvailable(bool available)
{
    if (_available == available) {
        return;
    }

    _available = available;
    Q_EMIT availableChanged();
}

QFuture<QSharedPointer<Share>> SharingManager::createShare(QPromise<QSharedPointer<Share>> *promise)
{
    auto future = promise->future();

    auto job = new JsonApiJob(_account, SHARING_V1_BASE % "/share"_L1, this);
    job->setVerb(SimpleApiJob::Verb::Post);
    connect(job, &OCC::JsonApiJob::jsonReceived, this, [&promise](const QJsonDocument &json, int statusCode) -> void {
        qCritical() << "request finished with code" << statusCode << "data" << json;
        promise->start();
        auto share = Share::fromJson(json);
        promise->emplaceResult(share);
        qCritical() << "promise finished, share id:" << share->id();
        promise->finish();
    });
    job->start();

    return future;
}

OCC::JsonApiJob *SharingManager::createShareJob(QObject *parent)
{
    // TODO: use promises?
    auto job = new JsonApiJob(_account, SHARING_V1_BASE % "/share"_L1, parent);
    job->setVerb(SimpleApiJob::Verb::Post);
    return job;
}

OCC::JsonApiJob *SharingManager::createAddSourceJob(QSharedPointer<Share> share, const QString &fileId, QObject *parent)
{
    // TODO: use promises?
    auto job = new JsonApiJob(_account, SHARING_V1_BASE % "/share/%1/source"_L1.arg(share->id()), parent);
    job->setVerb(SimpleApiJob::Verb::Post);

    QJsonObject dataObject;
    dataObject.insert("class"_L1, SOURCE_TYPE_NODE);
    dataObject.insert("value"_L1, fileId);
    QJsonDocument body;
    body.setObject(dataObject);
    job->setBody(body);

    return job;
}

OCC::JsonApiJob *SharingManager::createAddRecipientJob(QSharedPointer<Share> share, QObject *parent)
{
    // TODO: use promises?
    auto job = new JsonApiJob(_account, SHARING_V1_BASE % "/share/%1/recipient"_L1.arg(share->id()), parent);
    job->setVerb(SimpleApiJob::Verb::Post);

    QJsonObject dataObject;
    dataObject.insert("class"_L1, "OC\\Core\\Sharing\\Recipient\\GroupShareRecipientType"_L1);
    dataObject.insert("value"_L1, "admin"_L1);
    QJsonDocument body;
    body.setObject(dataObject);
    job->setBody(body);

    return job;
}

OCC::JsonApiJob *SharingManager::createSearchJob(const QString &query, int64_t offset, int64_t limit, QObject *parent)
{
    // TODO: use promises?
    auto job = new JsonApiJob(_account, "/ocs/v2.php/apps/sharing/api/v1/recipients"_L1, parent);
    QUrlQuery params;
    params.addQueryItem("query"_L1, query);
    params.addQueryItem("offset"_L1, QString::number(offset));
    params.addQueryItem("limit"_L1, QString::number(limit));
    job->addQueryParams(params);

    return job;
}
