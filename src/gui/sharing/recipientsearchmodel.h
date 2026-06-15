/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>

#include <qqmlintegration.h>
#include <QJsonArray>

#include "accountfwd.h"

namespace OCC::Gui::Sharing {

class RecipientSearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    enum Roles {
        TypeRole = Qt::UserRole,
        ValueRole,
        DisplayNameRole,
        IconRole,
    };

    explicit RecipientSearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    [[nodiscard]] QString query() const;
    void setQuery(const QString &query);

Q_SIGNALS:
    void accountChanged();
    void queryChanged();

private:
    AccountPtr _account = nullptr;
    QJsonArray _searchResults;
    QString _query;
};

}
