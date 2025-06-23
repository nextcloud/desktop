/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "declarativeuimodel.h"
#include "networkjobs.h"

namespace OCC {

DeclarativeUiModel::DeclarativeUiModel(const AccountPtr &account, QObject *parent)
    : QAbstractListModel(parent)
    , _account(account)
{
    fetchPage();
}

void DeclarativeUiModel::fetchPage()
{
    if (!_account) {
        return;
    }

    auto job = new JsonApiJob(_account,
                              QLatin1String("ocs/v2.php/apps/declarativetest/version1"),
                              this);
    connect(job, &JsonApiJob::jsonReceived,
            this, &DeclarativeUiModel::slotPageFetched);
    job->start();
}

void DeclarativeUiModel::slotPageFetched(const QJsonDocument &json)
{
    const auto root = json.object().value(QStringLiteral("root")).toObject();
    if (root.empty()) {
        return;
    }
    const auto orientation = root.value(QStringLiteral("orientation")).toString();
    const auto rows = root.value(QStringLiteral("rows")).toArray();
    if (rows.empty()) {
        return;
    }

    for (const auto &rowValue : rows) {
        const auto row = rowValue.toObject();
        const auto children = row.value("children").toArray();

        for (const auto &childValue : children) {
            const auto child = childValue.toObject();
            Element element;
            element.name = child.value(QStringLiteral("element")).toString();
            element.type = child.value(QStringLiteral("type")).toString();
            element.label = child.value(QStringLiteral("label")).toString();
            element.url = _account->url().toString() + child.value(QStringLiteral("url")).toString();
            element.text = child.value(QStringLiteral("text")).toString();
            _page.append(element);
        }
    }

    Q_EMIT pageFetched();
}

QVariant DeclarativeUiModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    switch (role) {
    case ElementNameRole:
        return _page.at(index.row()).name; // Button, Text, Image
    case ElementTypeRole:
        return _page.at(index.row()).type; // Primary, Secondarys
    case ElementLabelRole:
        return _page.at(index.row()).label; // Cancel, Submit
    case ElementUrlRole:
        return _page.at(index.row()).url; // /core/img/logo/log.png
    case ElementTextRole:
        return _page.at(index.row()).text; // String
    }

    return {};
}

int DeclarativeUiModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _page.size();
}

QHash<int, QByteArray> DeclarativeUiModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[ElementNameRole] = "name";
    roles[ElementTypeRole] = "type";
    roles[ElementLabelRole] = "label";
    roles[ElementUrlRole] = "url";
    roles[ElementTextRole] = "text";

    return roles;
}

QString DeclarativeUiModel::pageOrientation() const
{
    return _pageOrientation;
}

}
