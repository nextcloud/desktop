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

#include "lockwatcher.h"
#include "filesystem.h"

#include <QLoggingCategory>
#include <QTimer>

using namespace OCC;

Q_LOGGING_CATEGORY(lcLockWatcher, "gui.lockwatcher", QtInfoMsg)

static const int check_frequency = 20 * 1000; // ms

LockWatcher::LockWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&_timer, &QTimer::timeout,
        this, &LockWatcher::checkFiles);
    _timer.start(check_frequency);
}

void LockWatcher::addFile(const QString &path)
{
    qCInfo(lcLockWatcher) << "Watching for lock of" << path << "being released";
    _watchedPaths.insert(path);
}

void LockWatcher::checkFiles()
{
    QSet<QString> unlocked;

    foreach (const QString &path, _watchedPaths) {
        if (!FileSystem::isFileLocked(path)) {
            qCInfo(lcLockWatcher) << "Lock of" << path << "was released";
            emit fileUnlocked(path);
            unlocked.insert(path);
        }
    }

    // Doing it this way instead of with a QMutableSetIterator
    // ensures that calling back into addFile from connected
    // slots isn't a problem.
    _watchedPaths.subtract(unlocked);
}
