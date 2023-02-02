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
    qCInfo(lcLockWatcher) << "Watching for lock of" << path << mode << "being released";
    _watchedPaths.insert({ path, mode });
}

void LockWatcher::setCheckInterval(std::chrono::milliseconds interval)
{
    _timer.start(interval.count());
}

bool LockWatcher::contains(const QString &path, OCC::FileSystem::LockMode mode) const
{
    return _watchedPaths.find({ path, mode }) != _watchedPaths.cend();
}

void LockWatcher::checkFiles()
{
    // copy as emit fileUnlocked might trigger a new insert
    const auto watchedPathsCopy = _watchedPaths;
    decltype(_watchedPaths) unlocked;
    for (const auto &p : watchedPathsCopy) {
        if (!FileSystem::isFileLocked(p.first, p.second)) {
            qCInfo(lcLockWatcher) << "Lock of" << p.first << p.second << "was released";
            emit fileUnlocked(p.first, p.second);
            unlocked.insert(p);
        }
    }
    for (const auto &removed : qAsConst(unlocked)) {
        _watchedPaths.erase(removed);
    }
}
