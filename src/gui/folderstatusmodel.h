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

#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <accountfwd.h>

#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE
#endif

namespace OCC {

class Folder;

class FolderStatusModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    FolderStatusModel(QObject * parent = 0);
    ~FolderStatusModel();
    void setAccount(const OCC::AccountPtr& account);

    Qt::ItemFlags flags( const QModelIndex& ) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex parent(const QModelIndex& child) const Q_DECL_OVERRIDE;
    bool canFetchMore(const QModelIndex& parent) const Q_DECL_OVERRIDE;
    void fetchMore(const QModelIndex& parent) Q_DECL_OVERRIDE;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;


    struct SubFolderInfo {
        QString _name;
        QString _path;
        QVector<int> _pathIdx;
        int _size = 0;
        bool _fetched = false; // If we did the LSCOL for this folder already
        bool _fetching = false;
        QVector<SubFolderInfo> _subs;
        Qt::CheckState _checked = Qt::Checked;
        Folder *_folder;
    };

    mutable QVector<SubFolderInfo> _folders;

    enum ItemType { RootFolder, SubFolder, AddButton, SelectiveSyncText };
    ItemType classify(const QModelIndex &index) const;
    SubFolderInfo *infoForIndex(const QModelIndex &index) const;

    bool isDirty() { return _dirty; }

    struct ProgressInfo {
        bool isNull() const
        { return _progressString.isEmpty() && _warningCount == 0 && _overallSyncString.isEmpty(); }
        QString _progressString;
        QString _overallSyncString;
        int _warningCount = 0;
        int _overallPercent = 0;
    };
    QVector<ProgressInfo> _progresses;


public slots:
    void slotApplySelectiveSync();
    void resetFolders();

private slots:
    void slotUpdateDirectories(const QStringList &);

private:
    QStringList createBlackList(OCC::FolderStatusModel::SubFolderInfo* root,
                                const QStringList& oldBlackList) const;
    AccountPtr _account;
    bool _dirty = false;

signals:
    void dirtyChanged();
};

class FolderStatusDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    public:
    FolderStatusDelegate();
    virtual ~FolderStatusDelegate();

    enum datarole { FolderAliasRole = Qt::UserRole + 100,
                    FolderPathRole,
                    FolderSecondPathRole,
                    FolderRemotePath,
                    FolderStatus,
                    FolderErrorMsg,
                    FolderSyncPaused,
                    FolderStatusIconRole,
                    FolderAccountConnected,

                    SyncProgressOverallPercent,
                    SyncProgressOverallString,
                    SyncProgressItemString,
                    AddProgressSpace,
                    WarningCount,
                    SyncRunning,

                    AddButton
                  };
    void paint( QPainter*, const QStyleOptionViewItem&, const QModelIndex& ) const Q_DECL_OVERRIDE;
    QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& ) const Q_DECL_OVERRIDE;
    bool editorEvent( QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                      const QModelIndex& index ) Q_DECL_OVERRIDE;
};

} // namespace OCC

#endif // FOLDERSTATUSMODEL_H
