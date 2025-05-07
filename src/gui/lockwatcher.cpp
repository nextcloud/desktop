/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lockwatcher.h"
#include "filesystem.h"

#include <QLoggingCategory>
#include <QTimer>

using namespace OCC;

Q_LOGGING_CATEGORY(lcLockWatcher, "nextcloud.gui.lockwatcher", QtInfoMsg)

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
    QSet<QString> unlocked;

    for (const auto &path : std::as_const(_watchedPaths)) {
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
