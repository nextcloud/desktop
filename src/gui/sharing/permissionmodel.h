/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "sharedetailslistmodel.h"

#include <QtQmlIntegration>

namespace OCC::Gui::Sharing {

class PermissionModel : public ShareDetailsListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        ClassNameRole,
        PlaceholderRole,
        EnabledRole,
    };

    explicit PermissionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setShare(Share* share) override;

private:
    QMetaObject::Connection _permissionsChangedConnection;
};

}
