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

#include <QAbstractItemModel>
#include <QLoggingCategory>
#include <QVector>
#include <QElapsedTimer>
#include <QPointer>

class QNetworkReply;
namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcFolderStatus)

class Folder;
class ProgressInfo;
class LsColJob;

/**
 * @brief The FolderStatusModel class
 * @ingroup gui
 */
class FolderStatusModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    FolderStatusModel(QObject *parent = nullptr);
    ~FolderStatusModel() override;
    void setAccountState(const AccountState *accountState);

    Qt::ItemFlags flags(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    void resetAndFetch(const QModelIndex &parent);
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    struct SubFolderInfo
    {
        SubFolderInfo()
            : _folder(nullptr)
            , _size(0)
            , _isExternal(false)
            , _fetched(false)
            , _hasError(false)
            , _fetchingLabel(false)
            , _isUndecided(false)
            , _checked(Qt::Checked)
        {
        }
        Folder *_folder;
        QString _name;
        QString _path;
        QVector<int> _pathIdx;
        QVector<SubFolderInfo> _subs;
        qint64 _size;
        bool _isExternal;

        bool _fetched; // If we did the LSCOL for this folder already
        QPointer<LsColJob> _fetchingJob; // Currently running LsColJob
        bool _hasError; // If the last fetching job ended in an error
        QString _lastErrorString;
        bool _fetchingLabel; // Whether a 'fetching in progress' label is shown.

        // undecided folders are the big folders that the user has not accepted yet
        bool _isUndecided;

        Qt::CheckState _checked;

        // Whether this has a FetchLabel subrow
        bool hasLabel() const;

        // Reset all subfolders and fetch status
        void resetSubs(FolderStatusModel *model, QModelIndex index);

        struct Progress
        {
            Progress()
                : _warningCount(0)
                , _overallPercent(0)
            {
            }
            bool isNull() const
            {
                return _progressString.isEmpty() && _warningCount == 0 && _overallSyncString.isEmpty();
            }
            QString _progressString;
            QString _overallSyncString;
            int _warningCount;
            int _overallPercent;
        };
        Progress _progress;

        std::chrono::steady_clock::time_point _lastProgressUpdated = std::chrono::steady_clock::now();
    };


    enum ItemType { RootFolder,
        SubFolder,
        AddButton,
        FetchLabel };
    ItemType classify(const QModelIndex &index) const;
    SubFolderInfo *infoForIndex(const QModelIndex &index) const;

    // If the selective sync check boxes were changed
    bool isDirty() { return _dirty; }

    /**
     * return a QModelIndex for the given path within the given folder.
     * Note: this method returns an invalid index if the path was not fetched from the server before
     */
    QModelIndex indexForPath(Folder *f, const QString &path) const;

public slots:
    void slotUpdateFolderState(Folder *);
    void slotApplySelectiveSync();
    void resetFolders();
    void slotSyncAllPendingBigFolders();
    void slotSyncNoPendingBigFolders();
    void slotSetProgress(const ProgressInfo &progress, Folder *f);

private slots:
    void slotUpdateDirectories(const QStringList &);
    void slotGatherPermissions(const QString &name, const QMap<QString, QString> &properties);
    void slotLscolFinishedWithError(QNetworkReply *r);
    void slotFolderSyncStateChange(Folder *f);
    void slotFolderScheduleQueueChanged();
    void slotNewBigFolder();

    /**
     * "In progress" labels for fetching data from the server are only
     * added after some time to avoid popping.
     */
    void slotShowFetchProgress();

private:
    QStringList createBlackList(const OCC::FolderStatusModel::SubFolderInfo &root,
        const QStringList &oldBlackList) const;

    void computeProgress(const ProgressInfo &progress, SubFolderInfo::Progress *pi);
    int indexOf(Folder *f) const;

    const AccountState *_accountState;
    bool _dirty; // If the selective sync checkboxes were changed
    QVector<SubFolderInfo> _folders;

    /**
     * Keeps track of items that are fetching data from the server.
     *
     * See slotShowPendingFetchProgress()
     */
    QMap<QPersistentModelIndex, QElapsedTimer> _fetchingItems;

signals:
    void dirtyChanged();

    // Tell the view that this item should be expanded because it has an undecided item
    void suggestExpand(const QModelIndex &);

    friend struct SubFolderInfo;
};

} // namespace OCC

#endif // FOLDERSTATUSMODEL_H
