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

#include <QObject>
#include <QFlags>
#include <QAbstractTableModel>
#include <QModelIndex>
#include <QVariant>
#include <QSharedPointer>
#include <QVector>

#include "accountfwd.h"

namespace OCC {

class Sharee : public QObject {
public:
    
    enum Type {
        User = 0,
        Group = 1,
        Federated = 6
    };
    Q_DECLARE_FLAGS(Types, Type)

    explicit Sharee(const QString shareWith,
                    const QString displayName,
                    const Type type);
    
    QString shareWith() const;
    QString displayName() const;
    Type type() const;

    
private:
    QString _shareWith;
    QString _displayName;
    Type _type;
};

class ShareeModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ShareeModel(AccountPtr account,
                         const QString search,
                         const QString type,
                         QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole);

    QSharedPointer<Sharee> parseSharee(const QVariantMap &data);

private slots:
    void shareesFetched(const QVariantMap &reply);

private:
    AccountPtr _account;
    QString _search;
    QString _type;

    QVector<QSharedPointer<Sharee>> _sharees;
};

}
