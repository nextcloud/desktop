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

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/folderwatcher_mac.h"


#include <cerrno>
#include <QDebug>
#include <QStringList>



namespace Mirall {

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p)
    : parent(p)
{
    folder = parent->root();
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
    qDebug() << "FolderWatcherPrivate::startWatching()" << folder;
    CFStringRef folderCF = CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(folder.unicode()),
                                                        folder.length());
    CFArrayRef pathsToWatch = CFStringCreateArrayBySeparatingStrings (NULL, folderCF, CFSTR(":"));

            //CFStringCreateArrayBySeparatingStrings (NULL, folderCF, CFSTR(":"));

    FSEventStreamContext ctx =  {0, this, NULL, NULL, NULL};

    // TODO: Add kFSEventStreamCreateFlagFileEvents ?

    stream = FSEventStreamCreate(NULL,
                                 &callback,
                                 &ctx,
                                 pathsToWatch,
                                 kFSEventStreamEventIdSinceNow,
                                 0, // latency
                                 kFSEventStreamCreateFlagNone
                                 );

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);
}

void FolderWatcherPrivate::doNotifyParent() {
    parent->changeDetected(folder);
}



} // ns mirall
