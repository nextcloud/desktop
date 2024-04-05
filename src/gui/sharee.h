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

#ifndef SHAREE_H
#define SHAREE_H

#include <QObject>
#include <QFlags>
#include <QAbstractListModel>
#include <QLoggingCategory>
#include <QModelIndex>
#include <QVariant>
#include <QSharedPointer>
#include <QVector>

#include "accountfwd.h"

class QJsonDocument;
class QJsonObject;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcSharing)

class Sharee
{
public:
    // Keep in sync with Share::ShareType
    enum Type {
        User = 0,
        Group = 1,
        Federated = 6
    };

    explicit Sharee(const QString &shareWith,
        const QString &displayName,
        const Type type);

    QString format() const;
    QString shareWith() const;
    QString displayName() const;
    Type type() const;

private:
    QString _shareWith;
    QString _displayName;
    Type _type;
};


class ShareeModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ShareeModel(const AccountPtr &account, const QString &type, QObject *parent = nullptr);

    typedef QVector<QSharedPointer<Sharee>> ShareeSet; // FIXME: make it a QSet<Sharee> when Sharee can be compared
    void fetch(const QString &search, const ShareeSet &blacklist);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QSharedPointer<Sharee> getSharee(int at);

    QString currentSearch() const { return _search; }

Q_SIGNALS:
    void shareesReady();
    void displayErrorMessage(int code, const QString &);

private:
    QSharedPointer<Sharee> parseSharee(const QJsonObject &data);
    void setNewSharees(const QVector<QSharedPointer<Sharee>> &newSharees);

    AccountPtr _account;
    QString _search;
    QString _type;

    QVector<QSharedPointer<Sharee>> _sharees;
    QVector<QSharedPointer<Sharee>> _shareeBlacklist;
};
}

Q_DECLARE_METATYPE(QSharedPointer<OCC::Sharee>)

#endif //SHAREE_H
