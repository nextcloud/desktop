/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QStringList>

namespace OCC::Sharing {

class Feature : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type READ type)

public:
    [[nodiscard]] static QSharedPointer<Feature> fromCapability(const QVariantMap &object);

    [[nodiscard]] QString type() const;
    [[nodiscard]] QStringList compatibleSourceTypes() const;
    [[nodiscard]] QStringList compatibleRecipientTypes() const;

private:
    QString _type;
    QStringList _compatibleSourceTypes;
    QStringList _compatibleRecipientTypes;

    explicit Feature(const QString &type, const QStringList &compatibleSourceTypes, const QStringList &compatibleRecipientTypes);
};

}

