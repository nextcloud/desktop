// SPDX-FileCopyrightText: 2022 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLocale>
#include <QTimer>

#include "common/syncjournalfilerecord.h"

#include "gui/filetagmodel.h"

namespace OCC {

class Folder;

class FileDetails : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY localPathChanged)
    Q_PROPERTY(QString name READ name NOTIFY fileChanged)
    Q_PROPERTY(QString sizeString READ sizeString NOTIFY fileChanged)
    Q_PROPERTY(QString lastChangedString READ lastChangedString NOTIFY fileChanged)
    Q_PROPERTY(QString iconUrl READ iconUrl NOTIFY fileChanged)
    Q_PROPERTY(QString lockExpireString READ lockExpireString NOTIFY lockExpireStringChanged)
    Q_PROPERTY(bool isFolder READ isFolder NOTIFY isFolderChanged)
    Q_PROPERTY(FileTagModel* fileTagModel READ fileTagModel NOTIFY fileTagModelChanged)
    Q_PROPERTY(bool sharingAvailable READ sharingAvailable NOTIFY fileChanged)

public:
    explicit FileDetails(QObject *parent = nullptr);

    [[nodiscard]] QString localPath() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString sizeString() const;
    [[nodiscard]] QString lastChangedString() const;
    [[nodiscard]] QString iconUrl() const;
    [[nodiscard]] QString lockExpireString() const;
    [[nodiscard]] bool isFolder() const;
    [[nodiscard]] FileTagModel *fileTagModel() const;
    [[nodiscard]] bool sharingAvailable() const;

public slots:
    void setLocalPath(const QString &localPath);

signals:
    void localPathChanged();
    void fileChanged();
    void lockExpireStringChanged();
    void isFolderChanged();
    void fileTagModelChanged();

private slots:
    void refreshFileDetails();
    void updateLockExpireString();
    void updateFileTagModel();

private:
    QString _localPath;

    QFileInfo _fileInfo;
    QFileSystemWatcher _fileWatcher;
    Folder *_folder = nullptr;
    SyncJournalFileRecord _fileRecord;
    SyncJournalFileLockInfo _filelockState;
    QByteArray _numericFileId;
    QString _lockExpireString;
    QTimer _filelockStateUpdateTimer;

    QLocale _locale;

    std::unique_ptr<FileTagModel> _fileTagModel;
    bool _sharingAvailable = true;
};

} // namespace OCC
