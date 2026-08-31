/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharelistmodel.h"

#include <algorithm>
#include <utility>

#include "unifiedshare.h"
#include "sharingcontroller.h"

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

namespace
{
QString recipientNames(const Share *share)
{
    auto names = QStringList{};
    if (!share) {
        return {};
    }

    for (const auto &recipient : share->recipients()) {
        if (!recipient) {
            continue;
        }

        const auto name = recipient->displayName().isEmpty() ? recipient->value() : recipient->displayName();
        if (!name.isEmpty()) {
            names.append(name);
        }
    }
    return names.join(", "_L1);
}
}

UnifiedShareListModel::UnifiedShareListModel(QObject *parent)
    : QAbstractListModel{parent}
{
}

SharingController *UnifiedShareListModel::sharingController() const
{
    return _sharingController;
}

void UnifiedShareListModel::setSharingController(SharingController *sharingController)
{
    if (_sharingController == sharingController) {
        return;
    }

    if (_sharingController) {
        disconnect(_sharingController, nullptr, this, nullptr);
    }

    _sharingController = sharingController;
    if (_sharingController) {
        connect(_sharingController, &SharingController::sharesChanged, this, &UnifiedShareListModel::rebuild);
        connect(_sharingController, &QObject::destroyed, this, [this] {
            _sharingController = nullptr;
            rebuild();
        });
    }

    rebuild();
    Q_EMIT sharingControllerChanged();
}

int UnifiedShareListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

QVariant UnifiedShareListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &item = _items.at(index.row());
    const auto share = item.share.data();
    switch (role) {
    case ShareRole:
        return QVariant::fromValue<Share *>(share);
    case RecipientNamesRole:
        return recipientNames(share);
    case ItemTypeRole:
        return QVariant::fromValue(item.type);
    case PublicLinkRole:
        return share && share->isPublicLink();
    case PublicLinkUrlRole:
        return share ? share->publicLinkUrl() : QString{};
    default:
        return {};
    }
}

QHash<int, QByteArray> UnifiedShareListModel::roleNames() const
{
    return {
        {ShareRole, "share"},
        {RecipientNamesRole, "recipientNames"},
        {ItemTypeRole, "itemType"},
        {PublicLinkRole, "publicLink"},
        {PublicLinkUrlRole, "publicLinkUrl"},
    };
}

void UnifiedShareListModel::rebuild()
{
    beginResetModel();

    for (const auto &connection : std::as_const(_shareConnections)) {
        disconnect(connection);
    }
    _shareConnections.clear();

    _items.clear();
    if (!_sharingController) {
        endResetModel();
        return;
    }

    auto shares = _sharingController->shares();
    shares.removeIf([](const Share *share) {
        return !share || share->state() == Share::State::Deleted || share->state() == Share::State::Unknown;
    });

    _shareConnections.reserve(shares.size() * 2);
    for (const auto share : std::as_const(shares)) {
        _shareConnections.append(connect(share, &Share::recipientsChanged, this, &UnifiedShareListModel::rebuild));
        _shareConnections.append(connect(share, &Share::stateChanged, this, &UnifiedShareListModel::rebuild));
    }

    _items.append({ItemType::InternalLink, nullptr});
    const auto hasPublicLink = std::ranges::any_of(shares, [](const Share *share) {
        return share && share->isPublicLink();
    });
    if (!hasPublicLink) {
        _items.append({ItemType::CreatePublicLink, nullptr});
    }

    for (const auto share : std::as_const(shares)) {
        _items.append({ItemType::Share, share});
    }

    endResetModel();
}
