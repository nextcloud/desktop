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

#include <accountfwd.h>
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
    enum {FileIdRole = Qt::UserRole+1};

    FolderStatusModel(QObject *parent = nullptr);
    ~FolderStatusModel() override;
    void setAccountState(const AccountState *accountState);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    void resetAndFetch(const QModelIndex &parent);
    [[nodiscard]] bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    struct SubFolderInfo
    {
        Folder *_folder = nullptr;
        QString _name; // Folder name to be displayed in the UI
        QString _path; // Sub-folder path that should always point to a local filesystem's folder
        QString _e2eMangledName; // Mangled name that needs to be used when making fetch requests and should not be used for displaying in the UI
        QVector<int> _pathIdx;
        QVector<SubFolderInfo> _subs;
        qint64 _size = 0;
        bool _isExternal = false;
        bool _isEncrypted = false;

        bool _fetched = false; // If we did the LSCOL for this folder already
        QPointer<LsColJob> _fetchingJob; // Currently running LsColJob
        bool _hasError = false; // If the last fetching job ended in an error
        QString _lastErrorString;
        bool _fetchingLabel = false; // Whether a 'fetching in progress' label is shown.
        // undecided folders are the big folders that the user has not accepted yet
        bool _isUndecided = false;
        QByteArray _fileId; // the file id for this folder on the server.

        Qt::CheckState _checked = Qt::Checked;

        bool _isNonDecryptable = false;

        // Whether this has a FetchLabel subrow
        [[nodiscard]] bool hasLabel() const;

        [[nodiscard]] bool isEncrypted() const { return _isEncrypted; }

        // Reset all subfolders and fetch status
        void resetSubs(FolderStatusModel *model, QModelIndex index);

        struct Progress
        {
            [[nodiscard]] bool isNull() const
            {
                return _progressString.isEmpty() && _warningCount == 0 && _overallSyncString.isEmpty();
            }
            QString _progressString;
            QString _overallSyncString;
            int _warningCount = 0;
            int _overallPercent = 0;
        };
        Progress _progress;
    };

    QVector<SubFolderInfo> _folders;

    enum ItemType { RootFolder,
        SubFolder,
        AddButton,
        FetchLabel };
    [[nodiscard]] ItemType classify(const QModelIndex &index) const;
    [[nodiscard]] SubFolderInfo *infoForIndex(const QModelIndex &index) const;
    [[nodiscard]] bool isAnyAncestorEncrypted(const QModelIndex &index) const;
    // If the selective sync check boxes were changed
    bool isDirty() { return _dirty; }

    /**
     * return a QModelIndex for the given path within the given folder.
     * Note: this method returns an invalid index if the path was not fetched from the server before
     */
    QModelIndex indexForPath(Folder *f, const QString &path) const;

public slots:
    void slotUpdateFolderState(OCC::Folder *);
    void slotApplySelectiveSync();
    void resetFolders();
    void slotSyncAllPendingBigFolders();
    void slotSyncNoPendingBigFolders();
    void slotSetProgress(const OCC::ProgressInfo &progress);
    void e2eInitializationFinished(bool isNewMnemonicGenerated);

private slots:
    void slotUpdateDirectories(const QStringList &);
    void slotGatherPermissions(const QString &name, const QMap<QString, QString> &properties);
    void slotGatherEncryptionStatus(const QString &href, const QMap<QString, QString> &properties);
    void slotLscolFinishedWithError(QNetworkReply *r);
    void slotFolderSyncStateChange(OCC::Folder *f);
    void slotFolderScheduleQueueChanged();
    void slotNewBigFolder();

    /**
     * "In progress" labels for fetching data from the server are only
     * added after some time to avoid popping.
     */
    void slotShowFetchProgress();

private:
    [[nodiscard]] QStringList createBlackList(const OCC::FolderStatusModel::SubFolderInfo &root,
        const QStringList &oldBlackList) const;
    const AccountState *_accountState = nullptr;
    bool _dirty = false; // If the selective sync checkboxes were changed

    bool _isSyncRunningForAwhile = false;

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

Q_DECLARE_METATYPE(OCC::FolderStatusModel::SubFolderInfo*)

#endif // FOLDERSTATUSMODEL_H
