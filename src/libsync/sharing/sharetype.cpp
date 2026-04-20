/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "sharetype.h"

#include <QVariantMap>

using namespace Qt::StringLiterals;

using namespace OCC::Sharing;

QSharedPointer<ShareType> ShareType::fromCapability(const QVariantMap &object)
{
    const auto type = object.value("type"_L1).toString();
    const auto displayName = object.value("displayName"_L1).toString();

    auto shareType = new ShareType{type, displayName};

    return QSharedPointer<ShareType>{shareType};
}

ShareType::ShareType(const QString &type, const QString &displayName)
    : _type{type}
    , _displayName{displayName}
{}

QString ShareType::type() const
{
    return _type;
}

QString ShareType::displayName() const
{
    return _displayName;
}
