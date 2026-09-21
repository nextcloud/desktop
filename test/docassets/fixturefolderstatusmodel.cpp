/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fixturefolderstatusmodel.h"
#include "folderstatusdelegate.h"
#include "theme.h"
namespace OCC::DocAssets
{
FixtureFolderStatusModel::FixtureFolderStatusModel(QObject *parent)
    : FolderStatusModel(parent)
{
    _folders.resize(1);
    _folders[0]._fetched = true;
}
Qt::ItemFlags FixtureFolderStatusModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
}
QVariant FixtureFolderStatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    if (classify(index) == AddButton) {
        return role == FolderStatusDelegate::AddButton ? QVariant(1) : QVariant{};
    }
    switch (role) {
    case FolderStatusDelegate::HeaderRole:
        return QStringLiteral("Nextcloud");
    case FolderStatusDelegate::FolderAliasRole:
        return QStringLiteral("Nextcloud");
    case FolderStatusDelegate::FolderPathRole:
        return QStringLiteral("/Users/alex/Nextcloud");
    case FolderStatusDelegate::FolderSecondPathRole:
        return QStringLiteral("/");
    case FolderStatusDelegate::FolderAccountConnected:
        return true;
    case FolderStatusDelegate::FolderStatusIconRole:
        return Theme::instance()->syncStateIcon(SyncResult::Success);
    case FolderStatusDelegate::FolderSyncText:
        return QStringLiteral("All synced!");
    default:
        return {};
    }
}
}
