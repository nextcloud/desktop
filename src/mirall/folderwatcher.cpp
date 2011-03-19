
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

static const uint32_t standard_event_mask =
    IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW;

namespace Mirall {

enum SubFolderListOption {
    SubFolderNoOptions = 0x0,
    SubFolderRecursive = 0x1,
};
Q_DECLARE_FLAGS(SubFolderListOptions, SubFolderListOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(SubFolderListOptions)

// Forgive me using a bool as a flag
static QStringList subFoldersList(QString folder,
                                  SubFolderListOptions options = SubFolderNoOptions )
{
    QDir dir(folder);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    QFileInfoList list = dir.entryInfoList();
    QStringList dirList;

    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        dirList << fileInfo.absoluteFilePath();
        if (options & SubFolderRecursive )
            dirList << subFoldersList(fileInfo.absoluteFilePath(), options);
    }
    return dirList;
}

FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _root(root)
{
    _inotify = new INotify(standard_event_mask);
    slotAddFolderRecursive(root);
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, int, const QString &)),
                     SLOT(slotINotifyEvent(int, int, const QString &)));
}

FolderWatcher::~FolderWatcher()
{

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
    QStringListIterator subfoldersIt(subFoldersList(path, SubFolderRecursive));
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
    QMutexLocker locker(&_mutex);

    if (mask & IN_CREATE) {
        qDebug() << cookie << " CREATE: " << path;
        if (QFileInfo(path).isDir()) {
            slotAddFolderRecursive(path);
        }
    }
    else if (mask & IN_DELETE) {
        qDebug() << cookie << " DELETE: " << path;
        if (_inotify->directories().contains(path));
            qDebug() << "`-> removing " << path;
            _inotify->removePath(path);
    }
    else if (mask & IN_CLOSE_WRITE) {
        qDebug() << cookie << " WRITABLE CLOSED: " << path;
    }
    else if (mask & IN_MOVE) {
        qDebug() << cookie << " MOVE: " << path;
    }
    else {
        qDebug() << cookie << " OTHER " << mask << " :" << path;
    }
    emit folderChanged(path);
}

}

#include "folderwatcher.moc"
