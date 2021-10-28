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

#include <chrono>

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcLockWatcher, "gui.lockwatcher", QtInfoMsg)

namespace {
const auto check_frequency = 20s;
}

LockWatcher::LockWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&_timer, &QTimer::timeout,
        this, &LockWatcher::checkFiles);
    _timer.start(check_frequency);
}

void LockWatcher::addFile(const QString &path, FileSystem::LockMode mode)
{
    qCInfo(lcLockWatcher) << "Watching for lock of" << path << "being released";
    _watchedPaths.insert(path, mode);
}

void LockWatcher::setCheckInterval(std::chrono::milliseconds interval)
{
    _timer.start(interval.count());
}

bool LockWatcher::contains(const QString &path)
{
    return _watchedPaths.contains(path);
}

void LockWatcher::checkFiles()
{
    // copy as emit fileUnlocked might trigger a new insert
    const auto watchedPathsCopy = _watchedPaths;
    QSet<QString> unlocked;
    for (auto it = watchedPathsCopy.cbegin(); it != watchedPathsCopy.cend(); ++it) {
        if (!FileSystem::isFileLocked(it.key(), it.value())) {
            qCInfo(lcLockWatcher) << "Lock of" << it.key() << "was released";
            emit fileUnlocked(it.key());
            unlocked.insert(it.key());
        }
    }
    for (const auto &removed : qAsConst(unlocked)) {
        _watchedPaths.remove(removed);
    }
}
