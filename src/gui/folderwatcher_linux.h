/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_FOLDERWATCHER_LINUX_H
#define MIRALL_FOLDERWATCHER_LINUX_H

#include <QObject>
#include <QString>
#include <QSocketNotifier>
#include <QHash>
#include <QDir>

#include "folderwatcher.h"

class QTimer;

namespace OCC {

/**
 * @brief Linux (inotify) API implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate : public QObject
{
    Q_OBJECT
public:
    FolderWatcherPrivate() = default;
    FolderWatcherPrivate(FolderWatcher *p, const QString &path);
    ~FolderWatcherPrivate() override;

    [[nodiscard]] int testWatchCount() const { return _pathToWatch.size(); }

    /// On linux the watcher is ready when the ctor finished.
    bool _ready = true;

protected slots:
    void slotReceivedNotification(int fd);
    void slotAddFolderRecursive(const QString &path);

protected:
    bool findFoldersBelow(const QDir &dir, QStringList &fullList);
    void inotifyRegisterPath(const QString &path);
    void removeFoldersBelow(const QString &path);

private:
    FolderWatcher *_parent = nullptr;

    QString _folder;
    QHash<int, QString> _watchToPath;
    QMap<QString, int> _pathToWatch;
    QScopedPointer<QSocketNotifier> _socket;
    int _fd = 0;
};
}

#endif
