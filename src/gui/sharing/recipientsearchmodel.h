/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QTimer>

#include <QJsonArray>
#include <qqmlintegration.h>

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

class RecipientSearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString shareId READ shareId WRITE setShareId NOTIFY shareIdChanged)
    Q_PROPERTY(bool fetchOngoing READ fetchOngoing NOTIFY fetchOngoingChanged)

public:
    enum Roles {
        TypeRole = Qt::UserRole,
        ValueRole,
        DisplayNameRole,
        InstanceRole,
        IconSvgUrlRole,
        IconLightRole,
        IconDarkRole,
    };

    explicit RecipientSearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    [[nodiscard]] QString query() const;
    void setQuery(const QString &query);

    [[nodiscard]] QString shareId() const;
    void setShareId(const QString &shareId);

    /** @brief Returns whether the current query is waiting for server results. */
    [[nodiscard]] bool fetchOngoing() const;

Q_SIGNALS:
    void accountChanged();
    void queryChanged();
    void shareIdChanged();
    void fetchOngoingChanged();

private:
    AccountPtr _account = nullptr;
    QJsonArray _searchResults;
    QString _query;
    QString _shareId;
    QTimer _searchTimer;
    bool _fetchOngoing = false;

    void search();
    void setFetchOngoing(bool fetchOngoing);
};

}
