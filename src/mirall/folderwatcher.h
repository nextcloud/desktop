/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>

class QTimer;
class INotify;

namespace Mirall {

/**
 * Watches a folder and sub folders for changes
 *
 * Will notify changed files relative to the root()
 * directory.
 *
 * If too many changes happen in a short time interval,
 * it will accumulate and be fired together later.
 */
class FolderWatcher : public QObject
{
Q_OBJECT
public:
    /**
     * @param root Path of the root of the folder
     */
    FolderWatcher(const QString &root, QObject *parent = 0L);
    ~FolderWatcher();

    /**
     * All watched folders and subfolders
     */
    QStringList folders() const;

    /**
     * Root path being monitored
     */
    QString root() const;

    /**
     * Add an ignore pattern that will not be
     * notified
     *
     * You can use wildcards
     */
    void addIgnore(const QString &pattern);

    /**
     * If true, folderChanged() events are sent
     * at least as often as eventInterval() seconds.
     */
    bool eventsEnabled() const;

    /**
     * Enabled or disables folderChanged() events.
     * If disabled, events are accumulated and emptied
     * the next time a folderChanged() event happens.
     */
    void setEventsEnabled(bool enabled);

    /**
     * Clear all pending events
     */
    void clearPendingEvents();

    /**
     * The minimum amounts of seconds that will separate
     * folderChanged() intervals
     */
    int eventInterval() const;

    /**
     * Sets minimum amounts of seconds that will separate
     * folderChanged() intervals
     */
    void setEventInterval(int seconds);

signals:
    /**
     * Emitted when one of the paths is changed
     */
    void folderChanged(const QStringList &pathList);

protected:
    void setProcessTimer();

protected slots:
    void slotINotifyEvent(int mask, int cookie, const QString &path);
    void slotAddFolderRecursive(const QString &path);
    // called when the manually process timer triggers
    void slotProcessTimerTimeout();

private:
    bool _eventsEnabled;
    int _eventInterval;

    INotify *_inotify;
    QString _root;
    // paths pending to notified
    QStringList _pendingPaths;

    QTimer *_processTimer;

    // to cancel events that belong to the same action
    int _lastMask;
    QString _lastPath;
    QStringList _ignores;

    // for the initial synchronization, without
    // any file changed
    bool _initialSyncDone;
};

}

#endif
