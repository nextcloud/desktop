/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TRAYACCOUNTAPPSMODEL_H
#define TRAYACCOUNTAPPSMODEL_H

#include "accountstate.h"

#include <QAbstractListModel>
#include <QHash>
#include <QUrl>

namespace OCC {

class TrayAccountAppsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    static TrayAccountAppsModel *instance();
    ~TrayAccountAppsModel() override = default;

    enum class Roles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        IconUrlRole,
    };

    Q_ENUM(Roles)

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

Q_SIGNALS:
    void countChanged();

public Q_SLOTS:
    void setUserId(int userId);

    void openAppUrl(const QUrl &url);

protected:
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    explicit TrayAccountAppsModel(QObject *parent = nullptr);

    [[nodiscard]] AccountAppList appsForUserId(int userId) const;

    static TrayAccountAppsModel *_instance;
    int _userId = -1;
    AccountAppList _apps;
};

} // namespace OCC

#endif // TRAYACCOUNTAPPSMODEL_H
