
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>

#include "mirall/folderwatcher.h"

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
    _watcher = new QFileSystemWatcher(this);

    // watch the path and all subdirectories
    {
        QMutexLocker locker(&_mutex);

        QStringList subfolders(subFoldersList(path, SubFolderRecursive));
        qDebug() << "adding watchers for " << subfolders;

        QStringListIterator subfoldersIt(subfolders);
        while (subfoldersIt.hasNext()) {
            _watcher->addPath(subfoldersIt.next());
        }

    }
    QObject::connect(_watcher, SIGNAL(directoryChanged(const QString &)),
                     SLOT(slotDirectoryChanged(const QString &)));
}

FolderWatcher::~FolderWatcher()
{

}

void FolderWatcher::slotDirectoryChanged(const QString &path)
{
    QMutexLocker locker(&_mutex);

    qDebug() << "changed: " << path;

    qDebug() << "updating subdirectories";

    QStringList watchedFolders(_watcher->directories());
    QStringListIterator watchedFoldersIt(watchedFolders);

    while (watchedFoldersIt.hasNext()) {
        QDir folder (watchedFoldersIt.next());
        if (!folder.exists()){
            qDebug() << "Removing " << folder.path();
            _watcher->removePath(folder.path());
        }
    }

    QStringListIterator subfoldersIt(subFoldersList(path, SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QDir folder (subfoldersIt.next());
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            qDebug() << "Adding " << folder.path();
            _watcher->addPath(folder.path());
        }

        // Look if some of the subdirectories disappeared


    }


    emit folderChanged(path);
}

}

#include "folderwatcher.moc"
