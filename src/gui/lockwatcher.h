/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "config.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QSet>
#include <QTimer>

namespace OCC {

/**
 * @brief Monitors files that are locked, signaling when they become unlocked
 *
 * Only relevant on Windows. Some high-profile applications like Microsoft
 * Word lock the document that is currently being edited. The synchronization
 * client will be unable to update them while they are locked.
 *
 * In this situation we do want to start a sync run as soon as the file
 * becomes available again. To do that, we need to regularly check whether
 * the file is still being locked.
 *
 * @ingroup gui
 */

class LockWatcher : public QObject
{
    Q_OBJECT
public:
    explicit LockWatcher(QObject *parent = 0);

    /** Start watching a file.
     *
     * If the file is not locked later on, the fileUnlocked signal will be
     * emitted once.
     */
    void addFile(const QString &path);

    /** Adjusts the default interval for checking whether the lock is still present */
    void setCheckInterval(std::chrono::milliseconds interval);

    /** Whether the path is being watched for lock-changes */
    bool contains(const QString &path);

signals:
    /** Emitted when one of the watched files is no longer
     *  being locked. */
    void fileUnlocked(const QString &path);

private slots:
    void checkFiles();

private:
    QSet<QString> _watchedPaths;
    QTimer _timer;
};
}
