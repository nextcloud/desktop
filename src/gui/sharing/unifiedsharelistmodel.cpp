/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharelistmodel.h"

#include "recipient.h"
#include "share.h"
#include "sharingcontroller.h"

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

namespace
{
constexpr auto internalSection = "internal"_L1;
constexpr auto externalSection = "external"_L1;
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
    return parent.isValid() ? 0 : _shares.size();
}

QVariant UnifiedShareListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto share = _shares.at(index.row());
    switch (role) {
    case ShareRole:
        return QVariant::fromValue(share);
    case SectionRole:
        return isExternalShare(share) ? externalSection : internalSection;
    default:
        return {};
    }
}

QHash<int, QByteArray> UnifiedShareListModel::roleNames() const
{
    return {
        {ShareRole, "share"},
        {SectionRole, "section"},
    };
}

void UnifiedShareListModel::rebuild()
{
    beginResetModel();

    for (const auto &connection : std::as_const(_shareConnections)) {
        disconnect(connection);
    }
    _shareConnections.clear();

    _shares = _sharingController ? _sharingController->shares() : QList<Share *>{};
    std::stable_sort(_shares.begin(), _shares.end(), [](const Share *left, const Share *right) {
        return isExternalShare(left) < isExternalShare(right);
    });

    _shareConnections.reserve(_shares.size());
    for (const auto share : std::as_const(_shares)) {
        if (share) {
            _shareConnections.append(connect(share, &Share::recipientsChanged, this, &UnifiedShareListModel::rebuild));
        }
    }

    endResetModel();
}

bool UnifiedShareListModel::isExternalShare(const Share *share)
{
    if (!share) {
        return false;
    }

    return std::ranges::any_of(share->recipients(), [](const QPointer<Recipient> &recipient) {
        if (!recipient) {
            return false;
        }

        const auto className = recipient->className().toLower();
        return recipient->instance().has_value() || recipient->secretUrl().has_value() || className.contains("federated"_L1) || className.contains("remote"_L1)
            || className.contains("email"_L1) || className.contains("link"_L1);
    });
}
