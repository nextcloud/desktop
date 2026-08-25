/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipientmodel.h"

#include <QPointer>

#include "unifiedshare.h"
#include "recipient.h"
#include "recipienticonutils.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

RecipientModel::RecipientModel(QObject *parent)
    : ShareDetailsListModel{parent}
{}

int RecipientModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !_share) {
        return 0;
    }

    return _share->recipients().size();
}

QVariant RecipientModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid));

    const auto recipients = _share->recipients();
    const auto recipient = recipients.at(index.row());

    switch (role) {
    case LabelRole:
        return recipient->displayName();
    case ClassNameRole:
        return recipient->className();
    case ValueRole:
        return recipient->value();
    case InstanceRole:
        return recipient->instance() ? QVariant{*recipient->instance()} : QVariant{};
    case IconSvgUrlRole:
        return RecipientIconUtils::svgDataUrl(recipient->iconSvg());
    case IconLightRole:
        return recipient->iconLight();
    case IconDarkRole:
        return recipient->iconDark();
    case SecretUpdatableRole:
        return recipient->secretUpdatable();
    case SecretValueRole:
        return recipient->secretValue().value_or(QString{});
    case SecretUrlRole:
        return recipient->secretUrl().value_or(QString{});
    case InitiatorDisplayNameRole:
        return recipient->initiatorDisplayName();
    default:
        return {};
    }
}

QHash<int, QByteArray> RecipientModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { ClassNameRole, "className"_ba},
        { ValueRole, "value"_ba},
        { InstanceRole, "instance"_ba},
        { IconSvgUrlRole, "iconSvgUrl"_ba},
        { IconLightRole, "iconLight"_ba},
        { IconDarkRole, "iconDark"_ba},
        { SecretUpdatableRole, "secretUpdatable"_ba},
        { SecretValueRole, "secretValue"_ba},
        { SecretUrlRole, "secretUrl"_ba},
        { InitiatorDisplayNameRole, "initiatorDisplayName"_ba},
    };
}

void RecipientModel::setShare(Share *share)
{
    ShareDetailsListModel::setShare(share);
    if (!_share) {
        return;
    }

    connect(_share, &Share::recipientsChanged, this, [this]() -> void {
        beginResetModel();
        endResetModel();
    });
}
