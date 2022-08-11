/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#include "sharee.h"
#include "networkjobs/jsonjob.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace OCC {

Q_LOGGING_CATEGORY(lcSharing, "gui.sharing", QtInfoMsg)

Sharee::Sharee(const QString shareWith,
    const QString displayName,
    const Type type)
    : _shareWith(shareWith)
    , _displayName(displayName)
    , _type(type)
{
}

QString Sharee::format() const
{
    QString formatted = _displayName;

    if (_type == Type::Group) {
        formatted += QLatin1String(" (group)");
    } else if (_type == Type::Federated) {
        formatted += QLatin1String(" (remote)");
    }

    return formatted;
}

QString Sharee::shareWith() const
{
    return _shareWith;
}

QString Sharee::displayName() const
{
    return _displayName;
}

Sharee::Type Sharee::type() const
{
    return _type;
}

ShareeModel::ShareeModel(const AccountPtr &account, const QString &type, QObject *parent)
    : QAbstractListModel(parent)
    , _account(account)
    , _type(type)
{
}

void ShareeModel::fetch(const QString &search, const ShareeSet &blacklist)
{
    _search = search;
    _shareeBlacklist = blacklist;
    const JsonApiJob::UrlQuery query {
        { QStringLiteral("search"), search },
        { QStringLiteral("itemType"), _type },
        { QStringLiteral("page"), QStringLiteral("1") },
        { QStringLiteral("perPage"), QStringLiteral("50") }

    };
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v1.php/apps/files_sharing/api/v1/sharees"), query, {}, this);
    connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        if (!job->ocsSuccess()) {
            Q_EMIT displayErrorMessage(job->ocsStatus(), job->ocsMessage());
        } else if (job->parseError().error == QJsonParseError::NoError) {
            auto data = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();

            QVector<QSharedPointer<Sharee>> newSharees;

            /*
             * Todo properly loop all of this
             */
            auto exact = data.value(QStringLiteral("exact")).toObject();
            {
                const auto &users = exact.value(QStringLiteral("users")).toArray();
                for (const auto &user : users) {
                    newSharees.append(parseSharee(user.toObject()));
                }
                const auto &groups = exact.value(QStringLiteral("groups")).toArray();
                for (const auto &group : groups) {
                    newSharees.append(parseSharee(group.toObject()));
                }
                const auto &remotes = exact.value(QStringLiteral("remotes")).toArray();
                for (const auto &remote : remotes) {
                    newSharees.append(parseSharee(remote.toObject()));
                }
            }

                    {
                        const auto &users = data.value(QStringLiteral("users")).toArray();
                        for (const auto &user : users) {
                            newSharees.append(parseSharee(user.toObject()));
                        }
                    }
                    {
                        const auto &groups = data.value(QStringLiteral("groups")).toArray();
                        for (const auto &group : groups) {
                            newSharees.append(parseSharee(group.toObject()));
                        }
                    }
                    {
                        const auto &remotes = data.value(QStringLiteral("remotes")).toArray();
                        for (const auto &remote : remotes) {
                            newSharees.append(parseSharee(remote.toObject()));
                        }
                    }

                    // Filter sharees that we have already shared with
                    QVector<QSharedPointer<Sharee>> filteredSharees;
                    for (const auto &sharee : qAsConst(newSharees)) {
                        bool found = false;
                        for (const auto &blacklistSharee : qAsConst(_shareeBlacklist)) {
                            if (sharee->type() == blacklistSharee->type() && sharee->shareWith() == blacklistSharee->shareWith()) {
                                found = true;
                                break;
                            }
                        }

                        if (found == false) {
                            filteredSharees.append(sharee);
                        }
                    }

                    setNewSharees(filteredSharees);
                    shareesReady();
                } });
    job->start();
}

QSharedPointer<Sharee> ShareeModel::parseSharee(const QJsonObject &data)
{
    QString displayName = data.value(QStringLiteral("label")).toString();
    const QString shareWith = data.value(QStringLiteral("value")).toObject().value(QStringLiteral("shareWith")).toString();
    Sharee::Type type = (Sharee::Type)data.value(QStringLiteral("value")).toObject().value(QStringLiteral("shareType")).toInt();
    const QString additionalInfo = data.value(QStringLiteral("value")).toObject().value(QStringLiteral("shareWithAdditionalInfo")).toString();
    if (!additionalInfo.isEmpty()) {
        displayName = tr("%1 (%2)", "sharee (shareWithAdditionalInfo)").arg(displayName, additionalInfo);
    }

    return QSharedPointer<Sharee>(new Sharee(shareWith, displayName, type));
}


// Helper function for setNewSharees   (could be a lambda when we can use them)
static QSharedPointer<Sharee> shareeFromModelIndex(const QModelIndex &idx)
{
    return idx.data(Qt::UserRole).value<QSharedPointer<Sharee>>();
}

struct FindShareeHelper
{
    const QSharedPointer<Sharee> &sharee;
    bool operator()(const QSharedPointer<Sharee> &s2) const
    {
        return s2->format() == sharee->format() && s2->displayName() == sharee->format();
    }
};

/* Set the new sharee

    Do that while preserving the model index so the selection stays
*/
void ShareeModel::setNewSharees(const QVector<QSharedPointer<Sharee>> &newSharees)
{
    layoutAboutToBeChanged();
    const auto persistent = persistentIndexList();
    QVector<QSharedPointer<Sharee>> oldPersistantSharee;
    oldPersistantSharee.reserve(persistent.size());

    std::transform(persistent.begin(), persistent.end(), std::back_inserter(oldPersistantSharee),
        shareeFromModelIndex);

    _sharees = newSharees;

    QModelIndexList newPersistant;
    newPersistant.reserve(persistent.size());
    for (const auto &sharee : qAsConst(oldPersistantSharee)) {
        FindShareeHelper helper = { sharee };
        auto it = std::find_if(_sharees.constBegin(), _sharees.constEnd(), helper);
        if (it == _sharees.constEnd()) {
            newPersistant << QModelIndex();
        } else {
            newPersistant << index(it - _sharees.constBegin());
        }
    }

    changePersistentIndexList(persistent, newPersistant);
    layoutChanged();
}


int ShareeModel::rowCount(const QModelIndex &) const
{
    return _sharees.size();
}

QVariant ShareeModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > _sharees.size()) {
        return QVariant();
    }

    const auto &sharee = _sharees.at(index.row());
    if (role == Qt::DisplayRole) {
        return sharee->format();

    } else if (role == Qt::EditRole) {
        // This role is used by the completer - it should match
        // the full name and the user name and thus we include both
        // in the output here. But we need to take care this string
        // doesn't leak to the user.
        return QStringLiteral("%1 (%2)").arg(sharee->displayName(), sharee->shareWith());

    } else if (role == Qt::UserRole) {
        return QVariant::fromValue(sharee);
    }

    return QVariant();
}

QSharedPointer<Sharee> ShareeModel::getSharee(int at)
{
    if (at < 0 || at > _sharees.size()) {
        return QSharedPointer<Sharee>(nullptr);
    }

    return _sharees.at(at);
}
}
