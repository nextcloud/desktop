/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "shareemodel.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#include "ocsshareejob.h"
#include "theme.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcShareeModel, "com.nextcloud.shareemodel")

ShareeModel::ShareeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    _searchRateLimitingTimer.setSingleShot(true);
    _searchRateLimitingTimer.setInterval(500);
    _searchGloballyPlaceholder.reset(new Sharee({}, tr("Search globally"), Sharee::LookupServerSearch, QStringLiteral("magnifying-glass.svg")));
    _searchGloballyPlaceholder->setIsIconColourful(true);
    connect(&_searchRateLimitingTimer, &QTimer::timeout, this, &ShareeModel::fetch);
    connect(Theme::instance(), &Theme::darkModeChanged, this, &ShareeModel::slotDarkModeChanged);
}

// ---------------------- QAbstractListModel methods ---------------------- //

int ShareeModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid() || !_accountState) {
        return 0;
    }

    return _sharees.count();
}

QHash<int, QByteArray> ShareeModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[ShareeRole] = "sharee";
    roles[AutoCompleterStringMatchRole] = "autoCompleterStringMatch";
    roles[TypeRole] = "type";
    roles[IconRole] = "icon";

    return roles;
}

QVariant ShareeModel::data(const QModelIndex &index, const int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid));

    const auto sharee = _sharees.at(index.row());

    if(sharee.isNull()) {
        return {};
    }

    switch(role) {
    case Qt::DisplayRole:
        return sharee->format();
    case AutoCompleterStringMatchRole:
        // Don't show this to the user
        return QString(sharee->displayName() + " (" + sharee->shareWith() + ")");
    case IconRole:
        return sharee->iconUrlColoured();
    case TypeRole:
        return sharee->type();
    case ShareeRole:
        return QVariant::fromValue(sharee);
    }

    qCWarning(lcShareeModel) << "Got unknown role" << role << "returning null value.";
    return {};
}

// --------------------------- QPROPERTY methods --------------------------- //

AccountState *ShareeModel::accountState() const
{
    return _accountState;
}

void ShareeModel::setAccountState(AccountState *accountState)
{
    if (accountState == _accountState) {
        return;
    }

    _accountState = accountState;
    Q_EMIT accountStateChanged();
}

bool ShareeModel::shareItemIsFolder() const
{
    return _shareItemIsFolder;
}

void ShareeModel::setShareItemIsFolder(const bool shareItemIsFolder)
{
    if (shareItemIsFolder == _shareItemIsFolder) {
        return;
    }

    _shareItemIsFolder = shareItemIsFolder;
    Q_EMIT shareItemIsFolderChanged();
}

QString ShareeModel::searchString() const
{
    return _searchString;
}

void ShareeModel::setSearchString(const QString &searchString)
{
    if (searchString == _searchString) {
        return;
    }

    beginResetModel();
    _sharees.clear();
    endResetModel();

    Q_EMIT shareesReady();

    _searchString = searchString;
    Q_EMIT searchStringChanged();

    _searchRateLimitingTimer.start();
}

bool ShareeModel::fetchOngoing() const
{
    return _fetchOngoing;
}

ShareeModel::LookupMode ShareeModel::lookupMode() const
{
    return _lookupMode;
}

void ShareeModel::setLookupMode(const ShareeModel::LookupMode lookupMode)
{
    if (lookupMode == _lookupMode) {
        return;
    }

    _lookupMode = lookupMode;
    Q_EMIT lookupModeChanged();
}

QVariantList ShareeModel::shareeBlocklist() const
{
    QVariantList returnSharees;
    for (const auto &sharee : _shareeBlocklist) {
        returnSharees.append(QVariant::fromValue(sharee));
    }
    return returnSharees;
}

void ShareeModel::setShareeBlocklist(const QVariantList shareeBlocklist)
{
    _shareeBlocklist.clear();
    for (const auto &sharee : shareeBlocklist) {
        _shareeBlocklist.append(sharee.value<ShareePtr>());
    }
    Q_EMIT shareeBlocklistChanged();

    filterSharees();
}

void ShareeModel::searchGlobally()
{
    setLookupMode(ShareeModel::LookupMode::GlobalSearch);
    beginResetModel();
    _sharees.clear();
    endResetModel();

    Q_EMIT shareesReady();
    fetch();
}

// ------------------------- Internal data methods ------------------------- //

void ShareeModel::fetch()
{
    if (!_accountState || !_accountState->account() || _searchString.isEmpty()) {
        qCInfo(lcShareeModel) << "Not fetching sharees for searchString: " << _searchString;
        return;
    }

    _fetchOngoing = true;

    Q_EMIT fetchOngoingChanged();

    const auto shareItemTypeString = _shareItemIsFolder ? QStringLiteral("folder") : QStringLiteral("file");

    auto *job = new OcsShareeJob(_accountState->account());

    connect(job, &OcsShareeJob::shareeJobFinished, this, &ShareeModel::shareesFetched);
    connect(job, &OcsJob::ocsError, this, [&](const int statusCode, const QString &message) {
        _fetchOngoing = false;
        Q_EMIT fetchOngoingChanged();
        Q_EMIT ShareeModel::displayErrorMessage(statusCode, message);
    });

    job->getSharees(_searchString, shareItemTypeString, 1, 50, _lookupMode == LookupMode::GlobalSearch ? true : false);
}

void ShareeModel::shareesFetched(const QJsonDocument &reply)
{
    _fetchOngoing = false;
    Q_EMIT fetchOngoingChanged();

    qCInfo(lcShareeModel) << "Reply: " << reply;

    QVector<ShareePtr> newSharees;

    const QStringList shareeTypes{"users", "groups", "emails", "remotes", "circles", "rooms", "lookup"};

    const auto appendSharees = [this, &shareeTypes, &newSharees](const QJsonObject &data) {
        for (const auto &shareeType : shareeTypes) {
            const auto category = data.value(shareeType).toArray();

            for (const auto &sharee : category) {
                const auto shareeJsonObject = sharee.toObject();
                const auto parsedSharee = parseSharee(shareeJsonObject);

                const auto shareeInBlacklistIt = std::find_if(_shareeBlocklist.cbegin(),
                                                              _shareeBlocklist.cend(),
                                                              [&parsedSharee](const ShareePtr &blacklistSharee) {
                    return parsedSharee->type() == blacklistSharee->type() &&
                           parsedSharee->shareWith() == blacklistSharee->shareWith();
                });

                if (shareeInBlacklistIt != _shareeBlocklist.cend()) {
                    continue;
                }

                newSharees.append(parsedSharee);
            }
        }
    };
    const auto replyDataObject = reply.object().value("ocs").toObject().value("data").toObject();
    const auto replyDataExactMatchObject = replyDataObject.value("exact").toObject();

    appendSharees(replyDataObject);
    appendSharees(replyDataExactMatchObject);

    beginResetModel();
    _sharees = newSharees;
    insertSearchGloballyItem(newSharees);
    endResetModel();

    Q_EMIT shareesReady();

    setLookupMode(LookupMode::LocalSearch);
}

void ShareeModel::insertSearchGloballyItem(const QVector<ShareePtr> &newShareesFetched)
{
    const auto foundIt = std::find_if(std::begin(_sharees), std::end(_sharees), [](const ShareePtr &sharee) {
        return sharee->type() == Sharee::LookupServerSearch || sharee->type() == Sharee::LookupServerSearchResults;
    });

    // remove it if it somehow appeared not at the end, to avoid writing complex proxy models for sorting
    if (foundIt != std::end(_sharees) && (foundIt + 1) != std::end(_sharees)) {
        _sharees.erase(foundIt);
    }

    _sharees.push_back(_searchGloballyPlaceholder);

    if (lookupMode() == LookupMode::GlobalSearch) {
        const auto displayName = newShareesFetched.isEmpty() ? tr("No results found") : tr("Global search results");
        _searchGloballyPlaceholder->setDisplayName(displayName);
        _searchGloballyPlaceholder->setType(Sharee::LookupServerSearchResults);
    } else {
        _searchGloballyPlaceholder->setDisplayName(tr("Search globally"));
        _searchGloballyPlaceholder->setType(Sharee::LookupServerSearch);
    }
}

ShareePtr ShareeModel::parseSharee(const QJsonObject &data) const
{
    auto displayName = data.value("label").toString();
    const auto shareWith = data.value("value").toObject().value("shareWith").toString();
    const auto type = (Sharee::Type)data.value("value").toObject().value("shareType").toInt();
    const auto additionalInfo = data.value("value").toObject().value("shareWithAdditionalInfo").toString();
    if (!additionalInfo.isEmpty()) {
        displayName = tr("%1 (%2)", "sharee (shareWithAdditionalInfo)").arg(displayName, additionalInfo);
    }

    return ShareePtr(new Sharee(shareWith, displayName, type));
}

void ShareeModel::filterSharees()
{
    auto it = _sharees.begin();

    while (it != _sharees.end()) {
        const auto sharee = *it;
        const auto shareeInBlacklistIt = std::find_if(_shareeBlocklist.cbegin(), _shareeBlocklist.cend(), [&sharee](const ShareePtr &blacklistSharee) {
            return sharee->type() == blacklistSharee->type() &&
                   sharee->shareWith() == blacklistSharee->shareWith();
        });

        if (shareeInBlacklistIt != _shareeBlocklist.end()) {
            const auto row = it - _sharees.begin();
            beginRemoveRows({}, row, row);
            it = _sharees.erase(it);
            endRemoveRows();
        } else {
            ++it;
        }
    }

    Q_EMIT shareesReady();
}

void ShareeModel::slotDarkModeChanged()
{
    for (int i = 0; i < _sharees.size(); ++i) {
        if (_sharees[i]->updateIconUrl()) {
            Q_EMIT dataChanged(index(i), index(i), {IconRole});
        }
    }
}

}
