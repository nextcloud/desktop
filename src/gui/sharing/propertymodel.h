/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include "share.h"

namespace OCC::Gui::Sharing {

class PropertyModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Share *share READ share WRITE setShare NOTIFY shareChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        PropertyRole,
        TypeRole,
        PlaceholderRole,
        ValueRole,
    };

    enum FieldTypes {
        Switch,
        TextField,
        TextArea,
        RecipientsField,
    };
    Q_ENUM(FieldTypes)

    explicit PropertyModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] Share* share() const;
    void setShare(Share* share);

Q_SIGNALS:
    void shareChanged();

private:
    Share *_share = nullptr;
    QVariantMap _fieldValues;
};

}
