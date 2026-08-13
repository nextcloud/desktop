/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <limits.h>
#include <sys/inotify.h>

#include "folder.h"
#include "folderwatcher_linux.h"

#include <cerrno>
#include <QStringList>
#include <QObject>

namespace OCC {

namespace {

// The inotify ABI guarantees that this is sufficient for one event,
// including a maximum-length filename, its terminating NUL, and record
// padding.
constexpr size_t kWorstCaseInotifyEventSize = sizeof(struct inotify_event) + NAME_MAX + 1;

// The inotify ABI guarantees that this is sufficient for one event,
// including a maximum-length filename, its terminating NUL, and record
// padding.
constexpr size_t kInotifyRecordsPerRead = 64;

constexpr size_t kInotifyReadBufferSize = kInotifyRecordsPerRead * kWorstCaseInotifyEventSize;

static_assert(kInotifyRecordsPerRead >= 1);
static_assert(kInotifyReadBufferSize >= kWorstCaseInotifyEventSize);

} // namespace

FolderWatcherPrivate::FolderWatcherPrivate(FolderWatcher *p, const QString &path)
    : QObject()
    , _parent(p)
    , _folder(path)
{
    // The buffer is allocated once per watcher and reused for every read.
    _inotifyBuffer.resize(static_cast<qsizetype>(kInotifyReadBufferSize));

    // Keep this descriptor blocking for now. The notification handler performs
    // one read per activation and therefore does not attempt to read until
    // EAGAIN.
    _fd = inotify_init();
    if (_fd != -1) {
        _socket.reset(new QSocketNotifier(_fd, QSocketNotifier::Read));
        connect(_socket.data(), &QSocketNotifier::activated, this, &FolderWatcherPrivate::slotReceivedNotification);
    } else {
        qCWarning(lcFolderWatcher) << "notify_init() failed: " << strerror(errno);
    }

    QMetaObject::invokeMethod(this, "slotAddFolderRecursive", Q_ARG(QString, path));
}

FolderWatcherPrivate::~FolderWatcherPrivate() = default;

// attention: result list passed by reference!
bool FolderWatcherPrivate::findFoldersBelow(const QDir &dir, QStringList &fullList)
{
    bool ok = true;
    if (!(dir.exists() && dir.isReadable())) {
        qCDebug(lcFolderWatcher) << "Non existing path coming in: " << dir.absolutePath();
        ok = false;
    } else {
        QStringList nameFilter;
        nameFilter << QLatin1String("*");
        
        const QDir::Filters filter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden;
        const QStringList paths = dir.entryList(nameFilter, filter);

        QStringList::const_iterator constIterator;
        for (constIterator = paths.constBegin(); constIterator != paths.constEnd();
             ++constIterator) {
            const QString fullPath(dir.path() + QLatin1String("/") + (*constIterator));

            fullList.append(fullPath);

            // Preserve failures from earlier recursive calls.
            ok = findFoldersBelow(QDir(fullPath), fullList) && ok;
        }
    }

    return ok;
}

void FolderWatcherPrivate::inotifyRegisterPath(const QString &path)
{
    if (path.isEmpty())
        return;

    int wd = inotify_add_watch(_fd, path.toUtf8().constData(),
        IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR);
    if (wd > -1) {
        _watchToPath.insert(wd, path);
        _pathToWatch.insert(path, wd);
    } else {
        // If we're running out of memory or inotify watches, become
        // unreliable.
        if (_parent->_isReliable && (errno == ENOMEM || errno == ENOSPC)) {
            _parent->_isReliable = false;
            emit _parent->becameUnreliable(
                tr("This problem usually happens when the inotify watches are exhausted. "
                   "Check the FAQ for details."));
        }
    }
}

void FolderWatcherPrivate::slotAddFolderRecursive(const QString &path)
{
    if (_pathToWatch.contains(path))
        return;

    int subdirs = 0;
    qCDebug(lcFolderWatcher) << "(+) Watcher:" << path;

    QDir inPath(path);
    inotifyRegisterPath(inPath.absolutePath());

    QStringList allSubfolders;
    if (!findFoldersBelow(QDir(path), allSubfolders)) {
        qCWarning(lcFolderWatcher) << "Could not traverse all sub folders";
    }
    QStringListIterator subfoldersIt(allSubfolders);
    while (subfoldersIt.hasNext()) {
        QString subfolder = subfoldersIt.next();
        QDir folder(subfolder);
        if (folder.exists() && !_pathToWatch.contains(folder.absolutePath())) {
            subdirs++;
            if (_parent->pathIsIgnored(subfolder)) {
                qCDebug(lcFolderWatcher) << "* Not adding" << folder.path();
                continue;
            }
            inotifyRegisterPath(folder.absolutePath());
        } else {
            qCDebug(lcFolderWatcher) << "    `-> discarded:" << folder.path();
        }
    }

    if (subdirs > 0) {
        qCDebug(lcFolderWatcher) << "    `-> and" << subdirs << "subdirectories";
    }
}

// Reads and processes pending inotify events for this watcher, updating
// watches recursively for created/removed subfolders as needed.
void FolderWatcherPrivate::slotReceivedNotification(int fd)
{
    ssize_t len;

    for (;;) {
        len = read(fd, _inotifyBuffer.data(), static_cast<size_t>(_inotifyBuffer.size()));
        if (len >= 0) {
            // Process the events returned by this read below.
            break;
        }

        if (errno == EINTR) {
            // Interrupted by a signal; just retry.
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now (if fd becomes non-blocking).
            return;
        }

        if (errno == EINVAL) {
            // The buffer is sized for at least one maximum-sized event.
            // Keep this as a diagnostic if the ABI or sizing changes.
            qCWarning(lcFolderWatcher)
                << "Inotify read buffer is too small for an event";

            // We cannot rely on the notification stream after an unexpected
            // sizing failure, so request a full local discovery and do not
            // process later records from this read.
            emit _parent->lostChanges();
            return;
        } else {
            qCWarning(lcFolderWatcher)
                << "Failed to read inotify events:" << strerror(errno);
        }

        return;
    }

    if (len == 0) {
        qCWarning(lcFolderWatcher)
            << "Inotify read returned zero bytes";
        return;
    }

    // iterate events in buffer
    bool needsRescan = false;
    size_t offset = 0;

    while (offset < static_cast<size_t>(len)) {
        const size_t remaining = static_cast<size_t>(len) - offset;

        if (remaining < sizeof(struct inotify_event)) {
            // Inotify should return complete records. Treat unexpected
            // truncation as loss of reliable incremental state.
            qCWarning(lcFolderWatcher)
                << "Incomplete inotify event header";
            needsRescan = true;
            break;
        }

        const char *eventData = _inotifyBuffer.constData() + offset;

        // Copy the fixed-size header into an aligned local object rather
        // than dereferencing a potentially unaligned buffer pointer.
        struct inotify_event eventHeader;
        std::memcpy(&eventHeader, eventData, sizeof(eventHeader));

        if (eventHeader.len > remaining - sizeof(struct inotify_event)) {
            // Defensive check against a truncated or malformed record.
            qCWarning(lcFolderWatcher)
                << "Incomplete inotify event";
            needsRescan = true;
            break;
        }

        const size_t eventSize = sizeof(struct inotify_event) + eventHeader.len;

        offset += eventSize;

        if (eventHeader.mask & IN_Q_OVERFLOW) {
            qCWarning(lcFolderWatcher)
                << "The inotify event queue overflowed; triggering a full local discovery";
            needsRescan = true;
            // Incremental processing is no longer reliable after queue
            // overflow, so do not process later records from this read.
            break;
        }

        // Events without a name are handled only through their mask.
        if (eventHeader.len == 0 || eventHeader.wd <= -1)
            continue;
        
        const char *nameData = eventData + sizeof(struct inotify_event);

        // eventHeader.len includes padding, so bound the search to the
        // current record and do not assume a valid NUL terminator blindly.
        const size_t nameLength = ::strnlen(nameData, eventHeader.len);

        if (nameLength == eventHeader.len) {
            qCWarning(lcFolderWatcher)
                << "Inotify event name is not NUL-terminated";
            needsRescan = true;
            break;
        }

        const QByteArray fileName(nameData, static_cast<qsizetype>(nameLength));

        // Filter out journal changes. This is redundant with filtering in
        // FolderWatcher::pathIsIgnored(), but avoids unnecessary processing.
        if (fileName.startsWith("._sync_")
            || fileName.startsWith(".csync_journal.db")
            || fileName.startsWith(".sync_")) {
            continue;
        }

        const auto watchPathIt = _watchToPath.constFind(eventHeader.wd);
        if (watchPathIt == _watchToPath.cend()) {
            qCDebug(lcFolderWatcher)
                << "Ignoring event for unknown watch descriptor"
                << eventHeader.wd << fileName;
            continue;
        }

        const QString p = *watchPathIt + '/' + fileName;

        _parent->changeDetected(p);

        if ((eventHeader.mask & (IN_MOVED_TO | IN_CREATE))
            && QFileInfo(p).isDir()
            && !_parent->pathIsIgnored(p)) {
            slotAddFolderRecursive(p);
        }

        if (eventHeader.mask & (IN_MOVED_FROM | IN_DELETE)) {
            removeFoldersBelow(p);
        }
    }
    
    if (needsRescan) {
        emit _parent->lostChanges();
    }
}

void FolderWatcherPrivate::removeFoldersBelow(const QString &path)
{
    auto it = _pathToWatch.find(path);

    if (it == _pathToWatch.end())
        return;

    const QString pathSlash = path + '/';

    // Remove the entry and all subentries.
    while (it != _pathToWatch.end()) {
        const auto itPath = it.key();

        if (!itPath.startsWith(path))
            break;

        if (itPath != path && !itPath.startsWith(pathSlash)) {
            // order is 'foo', 'foo bar', 'foo/bar'
            ++it;
            continue;
        }

        const auto wid = it.value();

        inotify_rm_watch(_fd, wid);
        _watchToPath.remove(wid);
        it = _pathToWatch.erase(it);

        qCDebug(lcFolderWatcher) << "Removed watch for" << itPath;
    }
}

} // ns mirall
