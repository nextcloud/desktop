/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

// event masks
#include "folderwatcher.h"

#include "accountstate.h"
#include "account.h"
#include "capabilities.h"

#include <cstdint>

#include <QFileInfo>
#include <QFlags>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#if defined(Q_OS_WIN)
#include "folderwatcher_win.h"
#elif defined(Q_OS_MAC)
#include "folderwatcher_mac.h"
#elif defined(Q_OS_UNIX)
#include "folderwatcher_linux.h"
#endif

#include "folder.h"
#include "filesystem.h"

namespace
{
const char *lockFilePatterns[] = {".~lock.", "~$"};
}

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderWatcher, "nextcloud.gui.folderwatcher", QtInfoMsg)

FolderWatcher::FolderWatcher(Folder *folder)
    : QObject(folder)
    , _folder(folder)
{
    if (_folder && _folder->accountState() && _folder->accountState()->account()) {
        connect(_folder->accountState()->account().data(), &Account::capabilitiesChanged, this, &FolderWatcher::folderAccountCapabilitiesChanged);
        folderAccountCapabilitiesChanged();
    }
}

FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::init(const QString &root)
{
    _d.reset(new FolderWatcherPrivate(this, root));
    _timer.start();
}

bool FolderWatcher::pathIsIgnored(const QString &path)
{
    if (path.isEmpty())
        return true;
    if (!_folder)
        return false;

#ifndef OWNCLOUD_TEST
    if (_folder->isFileExcludedAbsolute(path) && !Utility::isConflictFile(path)) {
        qCDebug(lcFolderWatcher) << "* Ignoring file" << path;
        return true;
    }
#endif
    return false;
}

bool FolderWatcher::isReliable() const
{
    return _isReliable;
}

void FolderWatcher::appendSubPaths(QDir dir, QStringList& subPaths) {
    QStringList newSubPaths = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    for (int i = 0; i < newSubPaths.size(); i++) {
        QString path = dir.path() + "/" + newSubPaths[i];
        QFileInfo fileInfo(path);
        subPaths.append(path);
        if (fileInfo.isDir()) {
            QDir dir(path);
            appendSubPaths(dir, subPaths);
        }
    }
}

void FolderWatcher::startNotificatonTest(const QString &path)
{
#ifdef Q_OS_MAC
    // Testing the folder watcher on OSX is harder because the watcher
    // automatically discards changes that were performed by our process.
    // It would still be useful to test but the OSX implementation
    // is deferred until later.
    return;
#endif

    Q_ASSERT(_testNotificationPath.isEmpty());
    _testNotificationPath = path;

    // Don't do the local file modification immediately:
    // wait for FolderWatchPrivate::_ready
    startNotificationTestWhenReady();
}

void FolderWatcher::startNotificationTestWhenReady()
{
    if (!_d->_ready) {
        QTimer::singleShot(1000, this, &FolderWatcher::startNotificationTestWhenReady);
        return;
    }

    auto path = _testNotificationPath;
    if (QFile::exists(path)) {
        auto mtime = FileSystem::getModTime(path);
        FileSystem::setModTime(path, mtime + 1);
    } else {
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Append);
    }
    FileSystem::setFileHidden(path, true);

    QTimer::singleShot(5000, this, [this]() {
        if (!_testNotificationPath.isEmpty())
            emit becameUnreliable(tr("The watcher did not receive a test notification."));
        _testNotificationPath.clear();
    });
}


int FolderWatcher::testLinuxWatchCount() const
{
#ifdef Q_OS_LINUX
    return _d->testWatchCount();
#else
    return -1;
#endif
}

void FolderWatcher::changeDetected(const QString &path)
{
    QFileInfo fileInfo(path);
    QStringList paths(path);
    if (fileInfo.isDir()) {
        QDir dir(path);
        appendSubPaths(dir, paths);
    }
    changeDetected(paths);
}

void FolderWatcher::changeDetected(const QStringList &paths)
{
    // TODO: this shortcut doesn't look very reliable:
    //   - why is the timeout only 1 second?
    //   - what if there is more than one file being updated frequently?
    //   - why do we skip the file altogether instead of e.g. reducing the upload frequency?

    // Check if the same path was reported within the last second.
    QSet<QString> pathsSet = paths.toSet();
    if (pathsSet == _lastPaths && _timer.elapsed() < 1000) {
        // the same path was reported within the last second. Skip.
        return;
    }
    _lastPaths = pathsSet;
    _timer.restart();

    QSet<QString> changedPaths;
    QSet<QString> unlockedFiles;

    for (int i = 0; i < paths.size(); ++i) {
        QString path = paths[i];
        if (!_testNotificationPath.isEmpty()
            && Utility::fileNamesEqual(path, _testNotificationPath)) {
            _testNotificationPath.clear();
        }
        
        if (_shouldWatchForFileUnlocking) {
            const auto unlockedFilePath = possiblyAddUnlockedFilePath(path);
            if (!unlockedFilePath.isEmpty()) {
                unlockedFiles.insert(unlockedFilePath);
            }

            qCDebug(lcFolderWatcher) << "Unlocked files:" << unlockedFiles.values();
        }

    // ------- handle ignores:
        if (pathIsIgnored(path)) {
            continue;
        }

        changedPaths.insert(path);
    }

    if (!unlockedFiles.isEmpty()) {
        emit filesLockReleased(unlockedFiles);
    }

    if (changedPaths.isEmpty()) {
        return;
    }

    qCInfo(lcFolderWatcher) << "Detected changes in paths:" << changedPaths;
    foreach (const QString &path, changedPaths) {
        emit pathChanged(path);
    }
}

void FolderWatcher::folderAccountCapabilitiesChanged()
{
    _shouldWatchForFileUnlocking = _folder->accountState()->account()->capabilities().filesLockAvailable();
}

QString FolderWatcher::possiblyAddUnlockedFilePath(const QString &path)
{
    qCDebug(lcFolderWatcher) << "Checking if it is a lock file:" << path;
    const auto pathSplit = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (pathSplit.isEmpty()) {
        return {};
    }
    QString lockFilePatternFound;
    for (const auto &lockFilePattern : lockFilePatterns) {
        if (pathSplit.last().startsWith(lockFilePattern)) {
            lockFilePatternFound = lockFilePattern;
            break;
        }
    }

    if (lockFilePatternFound.isEmpty() || QFileInfo::exists(path)) {
        return {};
    }

    qCDebug(lcFolderWatcher) << "Found a lock file with prefix:" << lockFilePatternFound << "in path:" << path;

    const auto lockFilePathWitoutPrefix = QString(path).replace(lockFilePatternFound, QStringLiteral(""));
    auto lockFilePathWithoutPrefixSplit = lockFilePathWitoutPrefix.split(QLatin1Char('.'));

    if (lockFilePathWithoutPrefixSplit.size() < 2) {
        return {};
    }

    auto extensionSanitized = lockFilePathWithoutPrefixSplit.takeLast().toStdString();
    // remove possible non-alphabetical characters at the end of the extension
    extensionSanitized.erase(
        std::remove_if(extensionSanitized.begin(), extensionSanitized.end(), [](const auto &ch) {
            return !std::isalnum(ch);
        }),
        extensionSanitized.end()
    );

    lockFilePathWithoutPrefixSplit.push_back(QString::fromStdString(extensionSanitized));
    auto unlockedFilePath = lockFilePathWithoutPrefixSplit.join(QLatin1Char('.'));

    if (!QFile::exists(unlockedFilePath)) {
        unlockedFilePath.clear();
        qCDebug(lcFolderWatcher) << "Assumed unlocked file path" << unlockedFilePath << "does not exist. Going to try to find matching file";
        auto splitFilePath = unlockedFilePath.split(QLatin1Char('/'));
        if (splitFilePath.size() > 1) {
            const auto lockFileName = splitFilePath.takeLast();
            // some software will modify lock file name such that it does not correspond to original file (removing some symbols from the name, so we will search for a matching file
            unlockedFilePath = findMatchingUnlockedFileInDir(splitFilePath.join(QLatin1Char('/')), lockFileName);
        }
    }

    if (unlockedFilePath.isEmpty() || !QFile::exists(unlockedFilePath)) {
        return {};
    }
    return unlockedFilePath;
}

QString FolderWatcher::findMatchingUnlockedFileInDir(const QString &dirPath, const QString &lockFileName)
{
    QString foundFilePath;
    const QDir dir(dirPath);
    for (const auto &candidateUnlockedFileInfo : dir.entryInfoList(QDir::Files)) {
        if (candidateUnlockedFileInfo.fileName().contains(lockFileName)) {
            foundFilePath = candidateUnlockedFileInfo.absoluteFilePath();
            break;
        }
    }
    return foundFilePath;
}

} // namespace OCC
