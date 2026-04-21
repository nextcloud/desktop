/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSortFilterProxyModel>
#include <qqmlintegration.h>

namespace OCC::Gui::Sharing {

class SharingFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(FilterType filterType READ filterType WRITE setFilterType NOTIFY filterTypeChanged)
    Q_PROPERTY(QStringList recipientTypes READ recipientTypes WRITE setRecipientTypes NOTIFY recipientTypesChanged)

public:
    enum FilterType {
        General,
        Settings,
    };
    Q_ENUM(FilterType)

    explicit SharingFilterModel(QObject *parent = nullptr);

    [[nodiscard]] FilterType filterType() const;
    void setFilterType(FilterType filterType);

    [[nodiscard]] QStringList recipientTypes() const;
    void setRecipientTypes(const QStringList &recipientTypes);

Q_SIGNALS:
    void filterTypeChanged();
    void recipientTypesChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    FilterType _filterType = FilterType::General;
    QStringList _recipientTypes = {};
};

}
