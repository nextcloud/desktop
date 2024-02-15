/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef FOLDERSTATUSMODEL_H
#define FOLDERSTATUSMODEL_H

#include "accountfwd.h"
#include "progressdispatcher.h"

#include <QAbstractItemModel>
#include <QLoggingCategory>
#include <QVector>
#include <QElapsedTimer>
#include <QPointer>

class QNetworkReply;


namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcFolderStatus)

class Folder;
class PropfindJob;
namespace {
    class SubFolderInfo;
}
/**
 * @brief The FolderStatusModel class
 * @ingroup gui
 */
class FolderStatusModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum class Columns {
        HeaderRole, // must be 0 as it is also used from the default delegate
        FolderPathRole, // for a SubFolder it's the complete path
        FolderSecondPathRole,
        FolderConflictMsg,
        FolderErrorMsg,
        FolderSyncPaused,
        FolderStatusIconRole,
        FolderAccountConnected,
        FolderImage,

        SyncProgressOverallPercent,
        SyncProgressOverallString,
        SyncProgressItemString,
        WarningCount,
        SyncRunning,

        FolderSyncText,
        IsReady, // boolean
        IsUsingSpaces, // boolean

        Priority, // uint32_t
        IsDeployed, // bool

        QuotaUsed,
        QuotaTotal,

        ColumnCount
    };
    Q_ENUMS(Columns);

    FolderStatusModel(QObject *parent = nullptr);
    ~FolderStatusModel() override;
    void setAccountState(const AccountStatePtr &accountState);

    QVariant data(const QModelIndex &index, int role) const override;
    Folder *folder(const QModelIndex &index) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public slots:
    void slotUpdateFolderState(Folder *);
    void resetFolders();
    void slotSetProgress(const ProgressInfo &progress, Folder *f);

private slots:
    void slotFolderSyncStateChange(Folder *f);

private:
    int indexOf(Folder *f) const;

    AccountStatePtr _accountState;
    std::vector<std::unique_ptr<SubFolderInfo>> _folders;
};

} // namespace OCC

#endif // FOLDERSTATUSMODEL_H
