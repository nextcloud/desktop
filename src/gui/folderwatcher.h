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

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include "config.h"

#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QElapsedTimer>
#include <QHash>
#include <QScopedPointer>
#include <QSet>
#include <QDir>
#include <QTimer>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcFolderWatcher)

class FolderWatcherPrivate;
class Folder;

/**
 * @brief Monitors a directory recursively for changes
 *
 * Folder Watcher monitors a directory and its sub directories
 * for changes in the local file system. Changes are signalled
 * through the pathChanged() signal.
 *
 * @ingroup gui
 */

class FolderWatcher : public QObject
{
    Q_OBJECT

public:
    // Construct, connect signals, call init()
    explicit FolderWatcher(Folder *folder = nullptr);
    ~FolderWatcher() override;

    /**
     * @param root Path of the root of the folder
     */
    void init(const QString &root);

    /**
     * Returns false if the folder watcher can't be trusted to capture all
     * notifications.
     *
     * For example, this can happen on linux if the inotify user limit from
     * /proc/sys/fs/inotify/max_user_watches is exceeded.
     */
    [[nodiscard]] bool isReliable() const;

    /**
     * Triggers a change in the path and verifies a notification arrives.
     *
     * If no notification is seen, the folderwatcher marks itself as unreliable.
     * The path must be ignored by the watcher.
     */
    void startNotificatonTest(const QString &path);

    /// For testing linux behavior only
    [[nodiscard]] int testLinuxWatchCount() const;

    void slotLockFileDetectedExternally(const QString &lockFile);

    void setShouldWatchForFileUnlocking(bool shouldWatchForFileUnlocking);
    [[nodiscard]] int lockChangeDebouncingTimout() const;

signals:
    /** Emitted when one of the watched directories or one
     *  of the contained files is changed. */
    void pathChanged(const QString &path);

    /*
    * Emitted when lock files were removed
    */
    void filesLockReleased(const QSet<QString> &files);

    /*
     * Emitted when lock files were added
     */
    void filesLockImposed(const QSet<QString> &files);

    void lockedFilesFound(const QSet<QString> &files);

    /**
     * Emitted if some notifications were lost.
     *
     * Would happen, for example, if the number of pending notifications
     * exceeded the allocated buffer size on Windows. Note that the folder
     * watcher could still be able to capture all future notifications -
     * i.e. isReliable() is orthogonal to losing changes occasionally.
     */
    void lostChanges();

    /**
     * Signals when the watcher became unreliable. The string is a translated
     * message that can be shown to users.
     */
    void becameUnreliable(const QString &message);

protected slots:
    // called from the implementations to indicate a change in path
    void changeDetected(const QString &path);
    void changeDetected(const QStringList &paths);
    void folderAccountCapabilitiesChanged();

private slots:
    void startNotificationTestWhenReady();
    void lockChangeDebouncingTimerTimedOut();

protected:
    QHash<QString, int> _pendingPathes;

private:
    QScopedPointer<FolderWatcherPrivate> _d;
    QElapsedTimer _timer;
    QSet<QString> _lastPaths;
    Folder *_folder;
    bool _isReliable = true;

    bool _shouldWatchForFileUnlocking = false;

    void appendSubPaths(QDir dir, QStringList& subPaths);

    /* Check if the path should be ignored by the FolderWatcher. */
    [[nodiscard]] bool pathIsIgnored(const QString &path) const;

    /** Path of the expected test notification */
    QString _testNotificationPath;

    QSet<QString> _unlockedFiles;
    QSet<QString> _lockedFiles;

    QTimer _lockChangeDebouncingTimer;

    friend class FolderWatcherPrivate;
};
}

#endif
