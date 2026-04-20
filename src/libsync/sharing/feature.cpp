/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "feature.h"

#include <QVariantMap>

using namespace Qt::StringLiterals;

using namespace OCC::Sharing;

QSharedPointer<Feature> Feature::fromCapability(const QVariantMap &object)
{
    const auto type = object.value("type"_L1).toString();
    const auto compatibleSourceTypes = object.value("compatible_source_types"_L1).toStringList();
    const auto compatibleRecipientTypes = object.value("compatible_recipient_types"_L1).toStringList();

    return QSharedPointer<Feature>{new Feature{type, compatibleSourceTypes, compatibleRecipientTypes}};
}


Feature::Feature(const QString &type, const QStringList &compatibleSourceTypes, const QStringList &compatibleRecipientTypes)
    : _type{type}
    , _compatibleSourceTypes{compatibleSourceTypes}
    , _compatibleRecipientTypes{compatibleRecipientTypes}
{}

QString Feature::type() const
{
    return _type;
}

QStringList Feature::compatibleSourceTypes() const
{
    return _compatibleSourceTypes;
}

QStringList Feature::compatibleRecipientTypes() const
{
    return _compatibleRecipientTypes;
}
