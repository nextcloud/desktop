/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "sharee.h"
#include "ocsshareejob.h"

namespace OCC {

Sharee::Sharee(const QString shareWith,
               const QString displayName,
               const Type type)
: _shareWith(shareWith),
  _displayName(displayName),
  _type(type)
{
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


ShareeModel::ShareeModel(AccountPtr account,
                         const QString search,
                         const QString type,
                         QObject *parent)
: QAbstractTableModel(parent),
  _account(account),
  _search(search),
  _type(type)
{
    OcsShareeJob *job = new OcsShareeJob(_account, this);
    connect(job, SIGNAL(shareeJobFinished(QVariantMap)), SLOT(shareesFetched(QVariantMap)));
    job->getSharees(_search, _type, 1, 50);
}

void ShareeModel::shareesFetched(const QVariantMap &reply)
{
    auto data = reply.value("ocs").toMap().value("data").toMap();

    QVector<QSharedPointer<Sharee>> newSharees;

    /*
     * Todo properly loop all of this
     */
    auto exact = data.value("exact").toMap();
    {
        auto user = exact.value("user").toMap();
        if (user.size() > 0) {
            newSharees.append(parseSharee(user));
        }
        auto group = exact.value("group").toMap();
        if (group.size() > 0) {
            newSharees.append(parseSharee(group));
        }
        auto remote = exact.value("remote").toMap();
        if (remote.size() > 0) {
            newSharees.append(parseSharee(remote));
        }
    }

    {
        auto users = data.value("users").toList();
        foreach(auto user, users) {
            newSharees.append(parseSharee(user.toMap()));
        }
    }
    {
        auto groups = data.value("groups").toList();
        foreach(auto group, groups) {
            newSharees.append(parseSharee(group.toMap()));
        }
    }
    {
        auto remotes = data.value("remotes").toList();
        foreach(auto remote, remotes) {
            newSharees.append(parseSharee(remote.toMap()));
        }
    }

    beginInsertRows(QModelIndex(), _sharees.size(), newSharees.size());
    _sharees += newSharees;
    endInsertRows();
}

QSharedPointer<Sharee> ShareeModel::parseSharee(const QVariantMap &data)
{
    const QString displayName = data.value("label").toString();
    const QString shareWith = data.value("value").toMap().value("shareWith").toString();
    Sharee::Type type = (Sharee::Type)data.value("value").toMap().value("shareType").toInt();

    return QSharedPointer<Sharee>(new Sharee(shareWith, shareWith, type));
}

int ShareeModel::rowCount(const QModelIndex &) const
{
    return _sharees.size();
}

int ShareeModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QVariant ShareeModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        auto sharee = _sharees.at(index.row());

        switch(index.column()) {
            case 0:
                return sharee->displayName();
            case 1:
                return sharee->type();
            case 2:
                return sharee->shareWith();
        }
    } 
    
    return QVariant();
}

QVariant ShareeModel::headerData(int section, Qt::Orientation orientation, int role)
{
    qDebug() << Q_FUNC_INFO << section << orientation << role;
    if (orientation == Qt::Horizontal) {
        switch(section) {
            case 0:
                return "Name";
            case 1:
                return "Type";
            case 2:
                return "Id";
        }
    }
}

}
