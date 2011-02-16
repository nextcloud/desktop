
#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include <QObject>
#include <QString>
#include <QMutex>

class QFileSystemWatcher;

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
    void slotDirectoryChanged(const QString &path);
private:
    QFileSystemWatcher *_watcher;
    QMutex _mutex;
};

}

#endif
