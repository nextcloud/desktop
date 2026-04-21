/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include "accountstate.h"

namespace OCC::Gui::Sharing {

class SharingModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountState *accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        PropertyRole,
        TypeRole,
        PlaceholderRole,
    };

    enum FieldTypes {
        Switch,
        TextField,
        TextArea,
    };
    Q_ENUM(FieldTypes)

    explicit SharingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] AccountState *accountState() const;
    void setAccountState(AccountState *accountState);

Q_SIGNALS:
    void accountStateChanged();

private:
    AccountState *_accountState = nullptr;
};

}
