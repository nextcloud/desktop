/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtCore>
#include "libsync/account.h"

namespace OCC {

class JsonApiJob;

class DeclarativeUiModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit DeclarativeUiModel(const AccountPtr &accountState, QObject *parent = nullptr);
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    enum DataRole {
        ElementNameRole = Qt::UserRole + 1,
        ElementTypeRole,
        ElementLabelRole,
        ElementTextRole,
        ElementUrlRole
    };
    Q_ENUM(DataRole)

    [[nodiscard]] QString pageOrientation() const;
    void fetchPage();

signals:
    void pageFetched();

public slots:
    void slotPageFetched(const QJsonDocument &json);

private:
    struct Element {
        QString name;
        QString type;
        QString label;
        QString url;
        QString text;
    };
    QList<Element> _page;
    QString _pageOrientation;

    AccountPtr _account;
};

}
