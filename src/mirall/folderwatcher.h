

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include <QObject>
#include <QString>
#include <QMutex>

#include "mirall/inotify.h"

class QFileSystemWatcher;
class INotify;

namespace Mirall {

class FolderWatcher : public QObject
{
Q_OBJECT
public:
    FolderWatcher(const QString &path, QObject *parent = 0L);
    ~FolderWatcher();
signals:
    void folderChanged(const QString &path);
protected slots:
    //void slotDirectoryChanged(const QString &path);
    void slotDirectoryChanged(int mask, const QString &path);
private:
    QFileSystemWatcher *_watcher;
    QMutex _mutex;
    INotify *_inotify;
};

}

#endif
