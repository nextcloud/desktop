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

#pragma once

#include "gui/owncloudguilib.h"


#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
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

class OWNCLOUDGUI_EXPORT FolderWatcher : public QObject
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

    /* Check if the path is ignored. */
    virtual bool pathIsIgnored(const QString &path) const;

    /**
     * Returns false if the folder watcher can't be trusted to capture all
     * notifications.
     *
     * For example, this can happen on linux if the inotify user limit from
     * /proc/sys/fs/inotify/max_user_watches is exceeded.
     */
    bool isReliable() const;

    /**
     * Triggers a change in the path and verifies a notification arrives.
     *
     * If no notification is seen, the folderwatcher marks itself as unreliable.
     * The path must be ignored by the watcher.
     */
    void startNotificatonTest(const QString &path);

    /// For testing linux behavior only
    int testLinuxWatchCount() const;

    // pop the accumulated changes
    QSet<QString> popChangeSet();


Q_SIGNALS:
    /** Emitted when one of the watched directories or one
     *  of the contained files is changed. */
    void pathChanged(const QSet<QString> &path);

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

protected Q_SLOTS:
    // called from the implementations to indicate a change in path
    void changeDetected(const QSet<QString> &paths);

private Q_SLOTS:
    void startNotificationTestWhenReady();

private:
    QScopedPointer<FolderWatcherPrivate> _d;
    QTimer _timer;
    QSet<QString> _changeSet;
    Folder *_folder;
    bool _isReliable = true;

    /** Path of the expected test notification */
    QString _testNotificationPath;

    friend class FolderWatcherPrivate;
};
}
