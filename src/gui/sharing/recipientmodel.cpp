/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipientmodel.h"

#include <QPointer>

#include "share.h"
#include "recipient.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

RecipientModel::RecipientModel(QObject *parent)
    : AbstractShareModel{parent}
{}

int RecipientModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !_share) {
        return 0;
    }

    qCritical() << "recipients size:" << _share->recipients().size();
    return _share->recipients().size();
}

QVariant RecipientModel::data(const QModelIndex &index, int role) const
{
    if (!_share) {
        return {};
    }

    const auto recipients = _share->recipients();
    const auto recipient = recipients.at(index.row());

    switch (role) {
    case LabelRole:
        return recipient->displayName();
    case ClassNameRole:
        return recipient->className();
    case ValueRole:
        return recipient->value();
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
    };
};

void RecipientModel::setShare(Share *share)
{
    AbstractShareModel::setShare(share);
    if (!_share) {
        return;
    }

    connect(_share, &Share::recipientsChanged, this, [this]() -> void {
        beginResetModel();
        endResetModel();
    });
}
