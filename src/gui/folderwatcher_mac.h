/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_FOLDERWATCHER_MAC_H
#define MIRALL_FOLDERWATCHER_MAC_H

#include <QObject>
#include <QString>

#include <CoreServices/CoreServices.h>


namespace OCC {

/**
 * @brief Mac OS X API implementation of FolderWatcher
 * @ingroup gui
 */
class FolderWatcherPrivate
{
public:
    FolderWatcherPrivate(FolderWatcher *p, const QString &path);
    ~FolderWatcherPrivate();

    void startWatching();
    QStringList addCoalescedPaths(const QStringList &) const;
    void doNotifyParent(const QStringList &);
    void notifyAll();

    /// On OSX the watcher is ready when the ctor finished.
    bool _ready = true;

private:
    FolderWatcher *_parent;

    QString _folder;

    FSEventStreamRef _stream;
};
}

#endif
