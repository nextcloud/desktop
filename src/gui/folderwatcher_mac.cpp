/*
 * Copyright (C) by Markus Goetz <markus@woboq.com>
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
#include "config.h"

#include "folder.h"
#include "folderwatcher.h"
#include "folderwatcher_mac.h"


#include <cerrno>
#include <QDirIterator>
#include <QStringList>


namespace OCC {

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : _parent(p)
    , _folder(path)
{
    this->startWatching();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    FSEventStreamStop(_stream);
    FSEventStreamInvalidate(_stream);
    FSEventStreamRelease(_stream);
}

static void callback(
    ConstFSEventStreamRef streamRef,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPathsVoid,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[])
{
    Q_UNUSED(streamRef)
    Q_UNUSED(eventIds)

    qCDebug(lcFolderWatcher) << "FolderWatcherPrivate::callback by OS X";

    // These flags indicate that either due to system error or for unknown reasons, the entire
    // directory structure must be rescanned (including files)
    const auto c_fullScanFlags = kFSEventStreamEventFlagMustScanSubDirs
        | kFSEventStreamEventFlagKernelDropped
        | kFSEventStreamEventFlagUserDropped;

    for (int i = 0; i < static_cast<int>(numEvents); ++i) {
        const auto flag = eventFlags[i];
        if (flag & c_fullScanFlags) {
            reinterpret_cast<FolderWatcherPrivate *>(clientCallBackInfo)->notifyAll();
            return;
        }
    }

    const auto c_interestingFlags = kFSEventStreamEventFlagItemCreated // for new folder/file
        | kFSEventStreamEventFlagItemRemoved // for rm
        | kFSEventStreamEventFlagItemInodeMetaMod // for mtime change
        | kFSEventStreamEventFlagItemRenamed // also coming for moves to trash in finder
        | kFSEventStreamEventFlagItemModified // for content change
        | kFSEventStreamEventFlagItemCloned; // for cloned items (since 10.13)
    //We ignore other flags, e.g. for owner change, xattr change, Finder label change etc

    QStringList paths;
    CFArrayRef eventPaths = (CFArrayRef)eventPathsVoid;
    for (int i = 0; i < static_cast<int>(numEvents); ++i) {
        CFStringRef path = reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(eventPaths, i));

        QString qstring;
        CFIndex pathLength = CFStringGetLength(path);
        qstring.resize(pathLength);
        CFStringGetCharacters(path, CFRangeMake(0, pathLength), reinterpret_cast<UniChar *>(qstring.data()));
        QString fn = qstring.normalized(QString::NormalizationForm_C);

        if (!(eventFlags[i] & c_interestingFlags)) {
            qCDebug(lcFolderWatcher) << "Ignoring non-content changes for" << fn << eventFlags[i];
            continue;
        }

        paths.append(fn);
    }

    reinterpret_cast<FolderWatcherPrivate *>(clientCallBackInfo)->doNotifyParent(paths);
}

void FolderWatcherPrivate::startWatching()
{
    qCDebug(lcFolderWatcher) << "FolderWatcherPrivate::startWatching()" << _folder;
    CFStringRef folderCF = CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(_folder.unicode()),
        _folder.length());
    CFArrayRef pathsToWatch = CFStringCreateArrayBySeparatingStrings(nullptr, folderCF, CFSTR(":"));

    FSEventStreamContext ctx = { 0, this, nullptr, nullptr, nullptr };

    _stream = FSEventStreamCreate(nullptr,
        &callback,
        &ctx,
        pathsToWatch,
        kFSEventStreamEventIdSinceNow,
        0, // latency
        kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagIgnoreSelf);

    CFRelease(pathsToWatch);
    CFRelease(folderCF);
    FSEventStreamSetDispatchQueue(_stream, dispatch_get_main_queue());
    FSEventStreamStart(_stream);
}

QStringList FolderWatcherPrivate::addCoalescedPaths(const QStringList &paths) const
{
    QStringList coalescedPaths;
    for (const auto &eventPath : paths) {
        if (QDir(eventPath).exists()) {
            QDirIterator it(eventPath, QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const auto path = it.next();
                if (!paths.contains(path)) {
                    coalescedPaths.append(path);
                }
            }
        }
    }
    return (paths + coalescedPaths);
}

void FolderWatcherPrivate::doNotifyParent(const QStringList &paths)
{
    const QStringList totalPaths = addCoalescedPaths(paths);
    _parent->changeDetected(totalPaths);
}

void FolderWatcherPrivate::notifyAll()
{
    QDirIterator dirIterator(_folder, QDirIterator::Subdirectories);
    QStringList allPaths;

    while(dirIterator.hasNext()) {
        const auto dirEntry = dirIterator.next();
        allPaths.append(dirEntry);
    }

    _parent->changeDetected(allPaths);
}


} // ns mirall
