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

#include "folder.h"
#include "folderwatcher.h"
#include "folderwatcher_mac.h"


#include <cerrno>
#include <QDebug>
#include <QStringList>



namespace OCC {

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString& path)
    : _parent(p),
      _folder(path)
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
    Q_UNUSED(eventFlags)
    Q_UNUSED(eventIds)

    qDebug() << "FolderWatcherPrivate::callback by OS X";

    QStringList paths;
    CFArrayRef eventPaths = (CFArrayRef)eventPathsVoid;
    for (int i = 0; i < static_cast<int>(numEvents); ++i) {
        CFStringRef path = reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(eventPaths, i));

        QString qstring;
        CFIndex pathLength = CFStringGetLength(path);
        qstring.resize(pathLength);
        CFStringGetCharacters(path, CFRangeMake(0, pathLength), reinterpret_cast<UniChar *>(qstring.data()));

        paths.append(qstring);
    }

    reinterpret_cast<FolderWatcherPrivate*>(clientCallBackInfo)->doNotifyParent(paths);
}

void FolderWatcherPrivate::startWatching()
{
    qDebug() << "FolderWatcherPrivate::startWatching()" << _folder;
    CFStringRef folderCF = CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(_folder.unicode()),
                                                        _folder.length());
    CFArrayRef pathsToWatch = CFStringCreateArrayBySeparatingStrings (NULL, folderCF, CFSTR(":"));

    FSEventStreamContext ctx =  {0, this, NULL, NULL, NULL};

    // TODO: Add kFSEventStreamCreateFlagFileEvents ?

    _stream = FSEventStreamCreate(NULL,
                                 &callback,
                                 &ctx,
                                 pathsToWatch,
                                 kFSEventStreamEventIdSinceNow,
                                 0, // latency
                                 kFSEventStreamCreateFlagUseCFTypes|kFSEventStreamCreateFlagFileEvents|kFSEventStreamCreateFlagIgnoreSelf
                                 );

    CFRelease(pathsToWatch);
    FSEventStreamScheduleWithRunLoop(_stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(_stream);
}

void FolderWatcherPrivate::doNotifyParent(const QStringList &paths) {

    _parent->changeDetected(paths);
}



} // ns mirall
