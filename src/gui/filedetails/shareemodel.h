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

#pragma once

#include <QAbstractListModel>
#include <QTimer>

#include "accountstate.h"
#include "sharee.h"

class QJsonDocument;
class QJsonObject;

namespace OCC {

class ShareeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(bool shareItemIsFolder READ shareItemIsFolder WRITE setShareItemIsFolder NOTIFY shareItemIsFolderChanged)
    Q_PROPERTY(QString searchString READ searchString WRITE setSearchString NOTIFY searchStringChanged)
    Q_PROPERTY(bool fetchOngoing READ fetchOngoing NOTIFY fetchOngoingChanged)
    Q_PROPERTY(LookupMode lookupMode READ lookupMode WRITE setLookupMode NOTIFY lookupModeChanged)
    Q_PROPERTY(QVariantList shareeBlocklist READ shareeBlocklist WRITE setShareeBlocklist NOTIFY shareeBlocklistChanged)

public:
    enum class LookupMode {
        LocalSearch = 0,
        GlobalSearch = 1,
    };
    Q_ENUM(LookupMode);

    enum Roles {
        ShareeRole = Qt::UserRole + 1,
        AutoCompleterStringMatchRole,
        TypeRole,
        IconRole,
    };
    Q_ENUM(Roles);

    explicit ShareeModel(QObject *parent = nullptr);

    using ShareeSet = QVector<ShareePtr>; // FIXME: make it a QSet<Sharee> when Sharee can be compared

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, const int role) const override;

    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] bool shareItemIsFolder() const;
    [[nodiscard]] QString searchString() const;
    [[nodiscard]] bool fetchOngoing() const;
    [[nodiscard]] LookupMode lookupMode() const;
    [[nodiscard]] QVariantList shareeBlocklist() const;

signals:
    void accountStateChanged();
    void shareItemIsFolderChanged();
    void searchStringChanged();
    void fetchOngoingChanged();
    void lookupModeChanged();
    void shareeBlocklistChanged();

    void shareesReady();
    void displayErrorMessage(const int code, const QString &message);

public slots:
    void setAccountState(OCC::AccountState *accountState);
    void setShareItemIsFolder(const bool shareItemIsFolder);
    void setSearchString(const QString &searchString);
    void setLookupMode(const OCC::ShareeModel::LookupMode lookupMode);
    void setShareeBlocklist(const QVariantList shareeBlocklist);
    void searchGlobally();

    void fetch();

private slots:
    void shareesFetched(const QJsonDocument &reply);
    void insertSearchGloballyItem(const QVector<OCC::ShareePtr> &newShareesFetched);
    void filterSharees();
    void slotDarkModeChanged();

private:
    [[nodiscard]] ShareePtr parseSharee(const QJsonObject &data) const;

    QTimer _searchRateLimitingTimer;

    AccountState *_accountState = nullptr;
    QString _searchString;
    bool _shareItemIsFolder = false;
    bool _fetchOngoing = false;
    LookupMode _lookupMode = LookupMode::LocalSearch;

    QVector<ShareePtr> _sharees;
    QVector<ShareePtr> _shareeBlocklist;

    ShareePtr _searchGloballyPlaceholder;
};

}
