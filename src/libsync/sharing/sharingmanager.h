/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QVariantMap>
#include <QStringList>

#include "accountfwd.h"

#include "sharetype.h"
#include "feature.h"

namespace OCC::Sharing {

class SharingManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

public:
    explicit SharingManager(AccountPtr account, QObject *parent = nullptr);

    void updateFromCapabilities(const QVariantMap &capabilities);

    [[nodiscard]] Q_INVOKABLE QList<QSharedPointer<Feature>> availableFeatures(const QStringList &sourceTypes, const QStringList &recipientTypes) const;
    [[nodiscard]] Q_INVOKABLE bool isFeatureAvailable(const QString &feature, const QStringList &sourceTypes, const QStringList &recipientTypes) const;

    /**
     * \brief Whether the new sharing system is available (announced via capabilities).
     */
    [[nodiscard]] bool isAvailable() const;

    [[nodiscard]] QMap<QString, QSharedPointer<ShareType>> sourceTypes() const;
    [[nodiscard]] QMap<QString, QSharedPointer<ShareType>> recipientTypes() const;
    [[nodiscard]] QMap<QString, QSharedPointer<Feature>> features() const;

Q_SIGNALS:
    void availableChanged();

private:
    AccountPtr _account;

    bool _available = false;

    QStringList _apiVersions;
    QMap<QString, QSharedPointer<ShareType>> _sourceTypes;
    QMap<QString, QSharedPointer<ShareType>> _recipientTypes;
    QMap<QString, QSharedPointer<Feature>> _features;

    void setAvailable(bool available);
};

}

