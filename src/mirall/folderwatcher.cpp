
// event masks
#include <sys/inotify.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"

static const uint32_t standard_event_mask =
    IN_MODIFY | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ISDIR | IN_DONT_FOLLOW;

// IN_ONESHOT
//    IN_ATTRIB   | IN_CLOSE_WRITE | IN_CREATE     |
//    IN_DELETE   | IN_DELETE_SELF | IN_MOVED_FROM |
//    IN_MOVED_TO | IN_DONT_FOLLOW | IN_ONLYDIR;

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

FolderWatcher::FolderWatcher(const QString &path, QObject *parent)
    : QObject(parent)
{
    _inotify = new INotify(standard_event_mask);
    _inotify->addPath(path);

    // watch the path and all subdirectories
    {
        QMutexLocker locker(&_mutex);

        QStringList subfolders(subFoldersList(path, SubFolderRecursive));
        if (! subfolders.empty() ) {
            qDebug() << "adding watchers for " << subfolders;

            QStringListIterator subfoldersIt(subfolders);
            while (subfoldersIt.hasNext()) {
                _inotify->addPath(subfoldersIt.next());
            }
        }
    }
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, const QString &)),
                     SLOT(slotDirectoryChanged(int, const QString &)));
}

FolderWatcher::~FolderWatcher()
{

}

QStringList FolderWatcher::folders() const
{
    return _inotify->directories();
}

void FolderWatcher::slotDirectoryChanged(int mask, const QString &path)
{
    QMutexLocker locker(&_mutex);

    qDebug() << mask << " : changed: " << path;

    qDebug() << "updating subdirectories";

    QStringList watchedFolders(_inotify->directories());
    QStringListIterator watchedFoldersIt(watchedFolders);

    while (watchedFoldersIt.hasNext()) {
        QDir folder (watchedFoldersIt.next());
        if (!folder.exists()){
            qDebug() << "Removing " << folder.path();
            _inotify->removePath(folder.path());
        }
    }

    //qDebug() << "Removing " << folder.path();

    QStringListIterator subfoldersIt(subFoldersList(path, SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QDir folder (subfoldersIt.next());
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            qDebug() << "Adding " << folder.path();
            _inotify->addPath(folder.path());
        }
        else
            qDebug() << "discarding " << folder.path();


        // Look if some of the subdirectories disappeared


    }


    emit folderChanged(path);
}

}

#include "folderwatcher.moc"
