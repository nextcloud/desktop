

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include <QObject>
#include <QString>
#include <QMutex>

class INotify;

namespace Mirall {

/**
 * Watches a folder and sub folders for changes
 */
class FolderWatcher : public QObject
{
Q_OBJECT
public:
    /**
     * @param root Path of the root of the folder
     */
    FolderWatcher(const QString &root, QObject *parent = 0L);
    ~FolderWatcher();

    /**
     * All watched folders and subfolders
     */
    QStringList folders() const;

signals:
    /**
     * Emitted when one of the paths is changed
     */
    void folderChanged(const QString &path);

protected slots:
    void slotINotifyEvent(int mask, int cookie, const QString &path);
    void slotAddFolderRecursive(const QString &path);
private:
    QMutex _mutex;
    INotify *_inotify;
    QString _root;
};

}

#endif
