/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLoggingCategory>
#include <QtCore>

namespace OCC {

class EndpointModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit EndpointModel(QObject *const parent = nullptr);

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    enum DataRole {
        EndpointTypeRole = Qt::UserRole + 1,
        EndpointNameRole,
        EndpointUrlRole
    };
    Q_ENUM(DataRole)

    enum EndpointType {
        ContextMenuRole,
        CreateMenuRole
    };
    Q_ENUM(EndpointType)

    struct Endpoint {
        EndpointType type;
        QString name;
        QString url;
    };

    using Endpoints = QList<Endpoint>;

    void parseElements(const QVariantList &elementsList);

private:
    EndpointType stringToEnum(const QString &type);
    Endpoints _endpoints;
};

}
