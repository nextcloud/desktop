/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QDateTime>

#include "accountstate.h"
#include "filedetails.h"
#include "folderman.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFileDetails, "nextcloud.gui.filedetails", QtInfoMsg)

FileDetails::FileDetails(QObject *parent)
    : QObject(parent)
{
    _filelockStateUpdateTimer.setInterval(6000);
    _filelockStateUpdateTimer.setSingleShot(false);
    connect(&_filelockStateUpdateTimer, &QTimer::timeout, this, &FileDetails::updateLockExpireString);
}

void FileDetails::refreshFileDetails()
{
    _fileInfo.refresh();
    Q_EMIT fileChanged();
}

QString FileDetails::localPath() const
{
    return _localPath;
}

void FileDetails::setLocalPath(const QString &localPath)
{
    if(localPath.isEmpty()) {
        return;
    }

    if(!_localPath.isEmpty()) {
        _fileWatcher.removePath(_localPath);
    }

    if(_fileInfo.exists()) {
        disconnect(&_fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileDetails::refreshFileDetails);
    }

    _localPath = localPath;
    _fileInfo = QFileInfo(localPath);

    _fileWatcher.addPath(localPath);
    connect(&_fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileDetails::refreshFileDetails);

    _folder = FolderMan::instance()->folderForPath(_localPath);
    Q_ASSERT(_folder);
    if (!_folder) {
        qCWarning(lcFileDetails) << "No folder found for path:" << _localPath << "will not load file details.";
        return;
    }

    const auto file = _localPath.mid(_folder->cleanPath().length() + 1);

    if (!_folder->journalDb()->getFileRecord(file, &_fileRecord)) {
        qCWarning(lcFileDetails) << "Invalid file record for path:"
                                 << _localPath
                                 << "will not load file details.";
    }

    _filelockState = _fileRecord._lockstate;
    updateLockExpireString();

    const auto accountState = _folder->accountState();
    Q_ASSERT(accountState);
    if (!accountState) {
        qCWarning(lcFileDetails) << "No account state found for path:" << _localPath << "will not correctly load file details.";
        return;
    }

    const auto account = accountState->account();
    Q_ASSERT(account);
    if (!account) {
        qCWarning(lcFileDetails) << "No account found for path:" << _localPath << "will not correctly load file details.";
        return;
    }

    _sharingAvailable = account->capabilities().shareAPI();

    updateFileTagModel();

    Q_EMIT fileChanged();
}

QString FileDetails::name() const
{
    return _fileInfo.fileName();
}

QString FileDetails::sizeString() const
{
    return _locale.formattedDataSize(_fileInfo.size());
}

QString FileDetails::lastChangedString() const
{
    static constexpr auto secsInMinute = 60;
    static constexpr auto secsInHour = secsInMinute * 60;
    static constexpr auto secsInDay = secsInHour * 24;
    static constexpr auto secsInMonth = secsInDay * 30;
    static constexpr auto secsInYear = secsInMonth * 12;

    const auto elapsedSecs = _fileInfo.lastModified().secsTo(QDateTime::currentDateTime());

    if(elapsedSecs < 60) {
        const auto elapsedSecsAsInt = static_cast<int>(elapsedSecs);
        return tr("%1 second(s) ago", "seconds elapsed since file last modified", elapsedSecsAsInt).arg(elapsedSecsAsInt);
    } else if (elapsedSecs < secsInHour) {
        const auto elapsedMinutes = static_cast<int>(elapsedSecs / secsInMinute);
        return tr("%1 minute(s) ago", "minutes elapsed since file last modified", elapsedMinutes).arg(elapsedMinutes);
    } else if (elapsedSecs < secsInDay) {
        const auto elapsedHours = static_cast<int>(elapsedSecs / secsInHour);
        return tr("%1 hour(s) ago", "hours elapsed since file last modified", elapsedHours).arg(elapsedHours);
    } else if (elapsedSecs < secsInMonth) {
        const auto elapsedDays = static_cast<int>(elapsedSecs / secsInDay);
        return tr("%1 day(s) ago", "days elapsed since file last modified", elapsedDays).arg(elapsedDays);
    } else if (elapsedSecs < secsInYear) {
        const auto elapsedMonths = static_cast<int>(elapsedSecs / secsInMonth);
        return tr("%1 month(s) ago", "months elapsed since file last modified", elapsedMonths).arg(elapsedMonths);
    } else {
        const auto elapsedYears = static_cast<int>(elapsedSecs / secsInYear);
        return tr("%1 year(s) ago", "years elapsed since file last modified", elapsedYears).arg(elapsedYears);
    }
}

QString FileDetails::iconUrl() const
{
    return QStringLiteral("image://tray-image-provider/:/fileicon") + _localPath;
}

QString FileDetails::lockExpireString() const
{
    return _lockExpireString;
}

void FileDetails::updateLockExpireString()
{
    if(!_filelockState._locked) {
        _filelockStateUpdateTimer.stop();
        _lockExpireString = QString();
        Q_EMIT lockExpireStringChanged();
        return;
    }

    if(!_filelockStateUpdateTimer.isActive()) {
        _filelockStateUpdateTimer.start();
    }

    static constexpr auto SECONDS_PER_MINUTE = 60;
    const auto lockExpirationTime = _filelockState._lockTime + _filelockState._lockTimeout;
    const auto remainingTime = QDateTime::currentDateTime().secsTo(QDateTime::fromSecsSinceEpoch(lockExpirationTime));
    const auto remainingTimeInMinutes = static_cast<int>(remainingTime > 0 ? remainingTime / SECONDS_PER_MINUTE : 0);

    _lockExpireString = tr("Locked by %1 - Expires in %2 minute(s)", "remaining time before lock expires", remainingTimeInMinutes).arg(_filelockState._lockOwnerDisplayName).arg(remainingTimeInMinutes);
    Q_EMIT lockExpireStringChanged();
}

bool FileDetails::isFolder() const
{
    return _fileInfo.isDir();
}

FileTagModel *FileDetails::fileTagModel() const
{
    return _fileTagModel.get();
}

void FileDetails::updateFileTagModel()
{
    const auto localPath = _fileRecord.path();
    const auto relPath = localPath.mid(_folder->cleanPath().length() + 1);
    QString serverPath = _folder->remotePathTrailingSlash() + _fileRecord.path();
 
    if (const auto vfsMode = _folder->vfs().mode(); _fileRecord.isVirtualFile() && vfsMode == Vfs::WithSuffix) {
        if (const auto suffix = _folder->vfs().fileSuffix(); !suffix.isEmpty() && serverPath.endsWith(suffix)) {
            serverPath.chop(suffix.length());
        }
    }

    _fileTagModel = std::make_unique<FileTagModel>(relPath, _folder->accountState()->account());
    Q_EMIT fileTagModelChanged();
}

bool FileDetails::sharingAvailable() const
{
    return _sharingAvailable;
}

} // namespace OCC
