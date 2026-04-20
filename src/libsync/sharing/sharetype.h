/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QStringList>

namespace OCC::Sharing {

class ShareType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString displayName READ displayName)

public:
    [[nodiscard]] static QSharedPointer<ShareType> fromCapability(const QVariantMap &object);

    [[nodiscard]] QString type() const;
    [[nodiscard]] QString displayName() const;

private:
    QString _type;
    QString _displayName;

    explicit ShareType(const QString &type, const QString &displayName);
};

}

