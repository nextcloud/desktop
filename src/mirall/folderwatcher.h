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

#include "config.h"

#include "mirall/folder.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QHash>

class QTimer;

namespace Mirall {

class FolderWatcherPrivate;

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
     * Root path being monitored
     */
    QString root() const;

    /**
      * Set a file name to load a file with ignore patterns.
      */
    void setIgnoreListFile( const QString& );

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

    QStringList ignores() const;
public slots:
    /**
     * Enabled or disables folderChanged() events.
     * If disabled, events are accumulated and emptied
     * the next time a folderChanged() event happens.
     */
    void setEventsEnabled(bool enabled=true);

    /**
     * @brief setEventsEnabledDelayed - start event logging after a while
     * @param delay     - delay time in milliseconds
     * @param enabled   - enable the events.
     */
    void setEventsEnabledDelayed( int );

signals:
    /**
     * Emitted when one of the paths is changed
     */
    void folderChanged(const QStringList &pathList);

protected:
    void setProcessTimer();

protected slots:
    // called when the manually process timer triggers
    void slotProcessTimerTimeout();
    void changeDetected(const QString &f);

protected:
    QHash<QString, int> _pendingPathes;

private:
    bool _eventsEnabled;
    int _eventInterval;
    FolderWatcherPrivate *_d;
    QString _root;
    // paths pending to notified
    // QStringList _pendingPaths;
    QTimer *_processTimer;
    QStringList _ignores;

    friend class FolderWatcherPrivate;
};

}

#endif
