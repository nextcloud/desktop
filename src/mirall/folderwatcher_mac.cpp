/*
 * Copyright (C) by Markus Goetz <markus@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#include "config.h"

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/folderwatcher_mac.h"


#include <cerrno>
#include <QDebug>
#include <QStringList>



namespace Mirall {

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString& path)
    : _parent(p),
      _folder(path)
{
    this->startWatching();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    FSEventStreamStop(stream);
    FSEventStreamInvalidate(stream);
}

static void callback(
        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[])
{
    qDebug() << "FolderWatcherPrivate::callback by OS X";
    reinterpret_cast<FolderWatcherPrivate*>(clientCallBackInfo)->doNotifyParent();
}

void FolderWatcherPrivate::startWatching()
{
    qDebug() << "FolderWatcherPrivate::startWatching()" << _folder;
    CFStringRef folderCF = CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(_folder.unicode()),
                                                        _folder.length());
    CFArrayRef pathsToWatch = CFStringCreateArrayBySeparatingStrings (NULL, folderCF, CFSTR(":"));

    FSEventStreamContext ctx =  {0, this, NULL, NULL, NULL};

    // TODO: Add kFSEventStreamCreateFlagFileEvents ?

    stream = FSEventStreamCreate(NULL,
                                 &callback,
                                 &ctx,
                                 pathsToWatch,
                                 kFSEventStreamEventIdSinceNow,
                                 0, // latency
                                 kFSEventStreamCreateFlagNone+kFSEventStreamCreateFlagIgnoreSelf
                                 );

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);
}

void FolderWatcherPrivate::doNotifyParent() {
    _parent->changeDetected(_folder);
}



} // ns mirall
