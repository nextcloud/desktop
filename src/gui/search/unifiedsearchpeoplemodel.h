/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountstate.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QTimer>

namespace OCC {

/** @brief Account-authenticated user suggestions for Unified Search. */
class UnifiedSearchPeopleModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(AccountState *accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum Role { UserIdRole = Qt::UserRole + 1, DisplayNameRole, AvatarUrlRole };

    explicit UnifiedSearchPeopleModel(QObject *parent = nullptr, int debounceInterval = 300);
    ~UnifiedSearchPeopleModel() override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] QString searchTerm() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString errorString() const;
    Q_INVOKABLE void retry();

public slots:
    void setAccountState(AccountState *accountState);
    void setSearchTerm(const QString &searchTerm);

signals:
    void accountStateChanged();
    void searchTermChanged();
    void busyChanged();
    void errorStringChanged();

private:
    struct Person { QString id; QString displayName; QString avatarUrl; };
    void startSearch();
    void cancel();
    void clearPeople();
    void replacePeople(QVector<Person> people);
    void setBusy(bool busy);
    void setErrorString(const QString &errorString);

    QPointer<AccountState> _accountState;
    QVector<Person> _people;
    QString _searchTerm;
    QString _errorString;
    QTimer _debounceTimer;
    QPointer<QObject> _job;
    quint64 _generation = 0;
    bool _busy = false;
};
}
