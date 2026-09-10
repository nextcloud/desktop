/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "sharedetailslistmodel.h"

#include <QPointer>
#include <QtQmlIntegration>

namespace OCC::Gui::Sharing {

class Recipient;

class PermissionModel : public ShareDetailsListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Recipient *recipient READ recipient WRITE setRecipient NOTIFY recipientChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        ClassNameRole,
        PlaceholderRole,
        EnabledRole,
        AvailableRole,
    };

    explicit PermissionModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setShare(Share* share) override;
    [[nodiscard]] Recipient *recipient() const;
    void setRecipient(Recipient *recipient);

Q_SIGNALS:
    void recipientChanged();

private:
    QMetaObject::Connection _permissionsChangedConnection;
    QMetaObject::Connection _recipientPermissionsChangedConnection;
    QMetaObject::Connection _recipientDestroyedConnection;
    QPointer<Recipient> _recipient;
};

}
