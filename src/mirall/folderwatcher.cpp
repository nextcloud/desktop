
// event masks
#include <sys/inotify.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"
#include "mirall/fileutils.h"

static const uint32_t standard_event_mask =
    IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW;

/* minimum amount of seconds between two
   events  to consider it a new event */
#define DEFAULT_EVENT_INTERVAL_SEC 5

namespace Mirall {


FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _eventsEnabled(true),
      _eventInterval(DEFAULT_EVENT_INTERVAL_SEC),
      _root(root),
      _processTimer(new QTimer(this)),
      _lastEventTime(QTime::currentTime()),
      _lastMask(0)
{
    _processTimer->setSingleShot(true);
    QObject::connect(_processTimer, SIGNAL(timeout()), this, SLOT(slotProcessTimerTimeout()));

    _inotify = new INotify(standard_event_mask);
    slotAddFolderRecursive(root);
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, int, const QString &)),
                     SLOT(slotINotifyEvent(int, int, const QString &)));
}

FolderWatcher::~FolderWatcher()
{

}

QString FolderWatcher::root() const
{
    return _root;
}

bool FolderWatcher::eventsEnabled() const
{
    return _eventsEnabled;
}

void FolderWatcher::setEventsEnabled(bool enabled)
{
    qDebug() << "    * event notification " << (enabled ? "enabled" : "disabled");
    _eventsEnabled = enabled;
    if (_eventsEnabled) {
        // schedule a queue cleanup for accumulated events
        if ( _pendingPaths.empty() )
            return;
        setProcessTimer();
    }
    else
    {
        // if we are disabling events, clear any ongoing timer
        if (_processTimer->isActive())
            _processTimer->stop();
    }
}

int FolderWatcher::eventInterval() const
{
    return _eventInterval;
}

void FolderWatcher::setEventInterval(int seconds)
{
    _eventInterval = seconds;
}

QStringList FolderWatcher::folders() const
{
    return _inotify->directories();
}

void FolderWatcher::slotAddFolderRecursive(const QString &path)
{
    qDebug() << "`-> adding " << path;
    _inotify->addPath(path);
    QStringList watchedFolders(_inotify->directories());
    //qDebug() << "currently watching " << watchedFolders;
    QStringListIterator subfoldersIt(FileUtils::subFoldersList(path, FileUtils::SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QDir folder (subfoldersIt.next());
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            qDebug() << "`-> adding " << folder.path();
            _inotify->addPath(folder.path());
        }
        else
            qDebug() << "`-> discarding " << folder.path();
    }
}

void FolderWatcher::slotINotifyEvent(int mask, int cookie, const QString &path)
{
    int lastMask = _lastMask;
    QString lastPath = _lastPath;

    _lastMask = mask;
    _lastPath = path;

    // cancel close write events that come after create
    //if (lastMask == IN_CREATE && mask == IN_CLOSE_WRITE
    //    && lastPath == path ) {
    //    return;
    //}

    if (IN_IGNORED & mask) {
        //qDebug() << "IGNORE event";
        return;
    }

    if (IN_Q_OVERFLOW & mask)
        //qDebug() << "OVERFLOW";

    if (mask & IN_CREATE) {
        qDebug() << cookie << " CREATE: " << path;
        if (QFileInfo(path).isDir()) {
            slotAddFolderRecursive(path);
        }
    }
    else if (mask & IN_DELETE) {
        //qDebug() << cookie << " DELETE: " << path;
        if (_inotify->directories().contains(path));
            qDebug() << "`-> removing " << path;
            _inotify->removePath(path);
    }
    else if (mask & IN_CLOSE_WRITE) {
        //qDebug() << cookie << " WRITABLE CLOSED: " << path;
    }
    else if (mask & IN_MOVE) {
        //qDebug() << cookie << " MOVE: " << path;
    }
    else {
        //qDebug() << cookie << " OTHER " << mask << " :" << path;
    }

    _pendingPaths.append(path);
    slotProcessPaths();
}

void FolderWatcher::slotProcessTimerTimeout()
{
    qDebug() << "* Scheduled processing of event queue";
    slotProcessPaths();
}

void FolderWatcher::setProcessTimer()
{
    if (!_processTimer->isActive()) {
        qDebug() << "* Pending events will be processed in" << eventInterval() << "seconds. (" << _pendingPaths.size() << "events until now )";
        _processTimer->start(eventInterval() * 1000);
    }
}

void FolderWatcher::slotProcessPaths()
{
    QTime eventTime = QTime::currentTime();

    if (!eventsEnabled())
        return;

    if (_lastEventTime.secsTo(eventTime) < eventInterval()) {
        //qDebug() << "`-> Last event happened less than " << eventInterval() << " seconds ago...";
        // schedule a forced queue cleanup later
        setProcessTimer();
        return;
    }

    // if the events will be processed because changed files and not
    // because a forced update, stop any timer.
    if (_processTimer->isActive())
        _processTimer->stop();

    _lastEventTime = eventTime;
    QStringList notifyPaths(_pendingPaths);
    _pendingPaths.clear();

    qDebug() << "  * Notify " << notifyPaths.size() << " changed items";
    emit folderChanged(notifyPaths);
}

}

#include "folderwatcher.moc"
