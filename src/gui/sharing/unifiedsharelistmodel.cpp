/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharelistmodel.h"

#include <algorithm>
#include <utility>

#include "recipient.h"
#include "unifiedshare.h"
#include "sharingconstants.h"
#include "sharingcontroller.h"

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

namespace
{
constexpr auto internalSection = "internal"_L1;
constexpr auto externalSection = "external"_L1;
constexpr auto additionalSection = "additional"_L1;
constexpr auto pendingSection = "pending"_L1;

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
    case SectionRole:
        return item.section;
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
        {SectionRole, "section"},
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
        return !share || share->state() == Share::ShareState::Deleted || share->state() == Share::ShareState::Unknown;
    });

    _shareConnections.reserve(shares.size() * 2);
    for (const auto share : std::as_const(shares)) {
        _shareConnections.append(connect(share, &Share::recipientsChanged, this, &UnifiedShareListModel::rebuild));
        _shareConnections.append(connect(share, &Share::stateChanged, this, &UnifiedShareListModel::rebuild));
    }

    const auto appendHeader = [this](const QString &section) {
        _items.append({ItemType::SectionHeader, section, nullptr});
    };
    const auto appendShares = [this, &shares](const QString &section) {
        for (const auto share : std::as_const(shares)) {
            if (sectionForShare(share) == section) {
                _items.append({ItemType::Share, section, share});
            }
        }
    };

    appendHeader(internalSection);
    appendShares(internalSection);
    _items.append({ItemType::InternalLink, internalSection, nullptr});

    appendHeader(externalSection);
    appendShares(externalSection);
    const auto hasPublicLink = std::ranges::any_of(shares, [](const Share *share) {
        return share && share->isPublicLink();
    });
    if (!hasPublicLink) {
        _items.append({ItemType::CreatePublicLink, externalSection, nullptr});
    }

    appendHeader(additionalSection);
    appendShares(additionalSection);

    if (std::ranges::any_of(shares, [](const Share *share) {
            return share && share->state() == Share::ShareState::Draft;
        })) {
        appendHeader(pendingSection);
        appendShares(pendingSection);
    }

    endResetModel();
}

QString UnifiedShareListModel::sectionForShare(const Share *share)
{
    if (share && share->state() == Share::ShareState::Draft) {
        return pendingSection;
    }

    if (isExternalShare(share)) {
        return externalSection;
    }
    return isInternalShare(share) ? internalSection : additionalSection;
}

bool UnifiedShareListModel::isInternalShare(const Share *share)
{
    if (!share || share->recipients().isEmpty()) {
        return false;
    }

    return std::ranges::all_of(share->recipients(), [](const QPointer<Recipient> &recipient) {
        if (!recipient) {
            return false;
        }

        const auto &className = recipient->className();
        return className == RecipientTypeClasses::user || className == RecipientTypeClasses::group || className == RecipientTypeClasses::team;
    });
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

        const auto &className = recipient->className();
        return recipient->instance().has_value() || className == RecipientTypeClasses::email || className == RecipientTypeClasses::token;
    });
}
