/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QVariantMap>
#include <QStringList>
#include <QFuture>

#include "accountfwd.h"
#include "networkjobs.h"

namespace OCC::Sharing {

class Share;

class SharingManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

public:
    static const QLatin1String SOURCE_TYPE_NODE;

    explicit SharingManager(AccountPtr account, QObject *parent = nullptr);

    void updateFromCapabilities(const QVariantMap &capabilities);

    /**
     * \brief Whether the new sharing system is available (announced via capabilities).
     */
    [[nodiscard]] bool isAvailable() const;

    QFuture<QSharedPointer<Share>> createShare(QPromise<QSharedPointer<Share>> *promise);
    JsonApiJob *createShareJob(QObject *parent);
    JsonApiJob *createAddSourceJob(QSharedPointer<Share> share, const QString &fileId, QObject *parent);
    JsonApiJob *createAddRecipientJob(QSharedPointer<Share> share, QObject *parent);
    JsonApiJob *createSearchJob(const QString &query, int64_t offset, int64_t limit, QObject *parent);

Q_SIGNALS:
    void availableChanged();
    void shareReceived(QSharedPointer<Share> share);
    void shareJsonReceived(const QJsonDocument &json);

private:
    AccountPtr _account;

    bool _available = false;

    QStringList _apiVersions;

    void setAvailable(bool available);
};

}

