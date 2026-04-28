/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include <QJsonArray>
#include "accountstate.h"

namespace OCC::Gui::Sharing {

class RecipientSearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountState *accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    enum Roles {
        TypeRole = Qt::UserRole,
        ValueRole,
        DisplayNameRole,
    };

    explicit RecipientSearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] AccountState *accountState() const;
    void setAccountState(AccountState *accountState);

    [[nodiscard]] QString query() const;
    void setQuery(const QString &query);

Q_SIGNALS:
    void accountStateChanged();
    void queryChanged();

private:
    AccountState *_accountState = nullptr;
    QJsonArray _searchResults;
    QString _query;
};

}
