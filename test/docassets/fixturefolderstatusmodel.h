/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "folderstatusmodel.h"
namespace OCC::DocAssets
{
class FixtureFolderStatusModel : public FolderStatusModel
{
public:
    explicit FixtureFolderStatusModel(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &) const override
    {
        return false;
    }
    bool hasChildren(const QModelIndex &parent = {}) const override
    {
        return !parent.isValid();
    }
};
}
