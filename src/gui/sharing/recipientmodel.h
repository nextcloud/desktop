/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "abstractsharemodel.h"

#include <qqmlintegration.h>

namespace OCC::Gui::Sharing {

class RecipientModel : public AbstractShareModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        ClassNameRole,
        ValueRole,
    };

    explicit RecipientModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setShare(Share* share) override;
};

}
