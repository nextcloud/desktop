/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "folder.h"
#include "folderwatcher.h"
#include "folderwatcher_mac.h"


#include <cerrno>
#include <QDirIterator>
#include <QSet>
#include <QStringList>


namespace OCC {

namespace {
/* Upper bound on the paths one event batch may be expanded into by addCoalescedPaths().
 *
 * Every reported path costs a journal lookup and a few stats on the GUI thread later on, so for a
 * large tree it is both faster and gentler on the UI to stop enumerating and let the next sync
 * rediscover the tree instead.
 */
constexpr auto maxCoalescedPaths = 1000;
}

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : _parent(p)
    , _folder(path)
{
    this->startWatching();
}

FolderWatcherPrivate::~FolderWatcherPrivate()
{
    if (_stream) {
        FSEventStreamStop(_stream);
        FSEventStreamInvalidate(_stream);
        FSEventStreamRelease(_stream);
        _stream = nullptr;
    }

    if (_queue) {
        // A callback block may already be running on the queue and still touching this object,
        // so let it drain before the members it reads are destroyed.
        dispatch_sync(_queue, ^{});
        dispatch_release(_queue);
        _queue = nullptr;
    }
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

    // Deliver events on a private queue rather than the main one: dropping a large tree into the
    // sync folder produces event batches big enough that handling them on the main queue blocks
    // the GUI. This mirrors what the Windows watcher does with its own WatcherThread.
    _queue = dispatch_queue_create("com.nextcloud.desktopclient.folderwatcher", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(_stream, _queue);
    FSEventStreamStart(_stream);
}

void FolderWatcherPrivate::addCoalescedPaths(const QStringList &paths, QStringList &coalesced) const
{
    // Only the expansion below is bounded, not `paths` itself: those are events FSEvents actually
    // reported, they are capped by the kernel's own buffer, and reporting them is what drives
    // things a rediscovery cannot redo, such as releasing office file locks.
    //
    // FSEvents reports a renamed or moved directory, but not the items below it, so the tree is
    // walked to report those too.
    coalesced = paths;
    QSet<QString> seen{paths.begin(), paths.end()};

    for (const auto &eventPath : paths) {
        if (!QDir(eventPath).exists()) {
            continue;
        }
        QDirIterator it(eventPath, QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto path = it.next();
            if (coalesced.size() >= maxCoalescedPaths) {
                // Give up on expanding this batch rather than report a whole tree path by path.
                // Nothing is lost by stopping: the directory that is being expanded was itself
                // reported, and SyncEngine::shouldDiscoverLocally() descends into a touched
                // directory's entire subtree, so the entries below it are discovered anyway.
                return;
            }
            if (!seen.contains(path)) {
                seen.insert(path);
                coalesced.append(path);
            }
        }
    }
}

void FolderWatcherPrivate::doNotifyParent(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }

    QStringList totalPaths;
    addCoalescedPaths(paths, totalPaths);

    // We are on the private dispatch queue here, but everything FolderWatcher touches from
    // changeDetected() (its timers, the lock-file sets, the pathChanged() receivers) may only be
    // used on the thread it lives on, so hand the batch over instead of processing it here.
    QMetaObject::invokeMethod(
        _parent, [parent = _parent, totalPaths] { parent->changeDetected(totalPaths); }, Qt::QueuedConnection);
}

void FolderWatcherPrivate::notifyAll()
{
    // FSEvents either dropped events or asked for a full rescan. Enumerating the whole tree to
    // report every path would take minutes on a large folder, and a full local discovery re-reads
    // it anyway, so ask for one of those instead of reporting anything.
    QMetaObject::invokeMethod(
        _parent, [parent = _parent, folder = _folder] { parent->changesLost(folder); }, Qt::QueuedConnection);
}


} // ns mirall
