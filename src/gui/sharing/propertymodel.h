/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "sharedetailslistmodel.h"
#include "property.h"

#include <QPointer>
#include <QtQmlIntegration>

namespace OCC::Gui::Sharing {

/**
 * @brief Exposes the normal or advanced properties of one share to QML.
 *
 * Properties are ordered from highest to lowest server-provided priority.
 */
class PropertyModel : public ShareDetailsListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool advanced READ advanced WRITE setAdvanced NOTIFY advancedChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole,
        PropertyRole,
        TypeRole,
        PlaceholderRole,
        ValueRole,
        RequiredRole,
        AdvancedRole,
        ValidValuesRole,
        MinimumRole,
        MaximumRole,
    };

    enum FieldTypes {
        Unknown,
        Boolean,
        Date,
        Enum,
        Password,
        String,
    };
    Q_ENUM(FieldTypes)

    explicit PropertyModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** @brief Returns whether the model exposes advanced instead of normal properties. */
    [[nodiscard]] bool advanced() const;
    /** @brief Selects whether the model exposes advanced instead of normal properties. */
    void setAdvanced(bool advanced);
    /** @brief Sets the share whose properties are exposed. */
    void setShare(Share* share) override;

Q_SIGNALS:
    /** @brief Emitted when the advanced-property filter changes. */
    void advancedChanged();

private:
    void resetProperties();

    QList<QPointer<Property>> _properties;
    bool _advanced = false;
};

}
