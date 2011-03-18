

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include <QObject>
#include <QString>
#include <QMutex>

class INotify;

namespace Mirall {

class FolderWatcher : public QObject
{
Q_OBJECT
public:
    FolderWatcher(const QString &root, QObject *parent = 0L);
    ~FolderWatcher();

    QStringList folders() const;

signals:
    void folderChanged(const QString &path);
protected slots:
    void slotDirectoryChanged(int mask, const QString &path);
    void slotAddFolderRecursive(const QString &path);
private:

    QMutex _mutex;
    INotify *_inotify;
    QString _root;
};

}

#endif
