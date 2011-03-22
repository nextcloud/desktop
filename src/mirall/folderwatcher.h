

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>

class QTimer;
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

    /**
     * Root path being monitored
     */
    QString root() const;

signals:
    /**
     * Emitted when one of the paths is changed
     */
    void folderChanged(const QStringList &pathList);

protected slots:
    void slotINotifyEvent(int mask, int cookie, const QString &path);
    void slotAddFolderRecursive(const QString &path);
    void slotProcessPaths();
private:
    INotify *_inotify;
    QString _root;
    // paths pending to notified
    QStringList _pendingPathList;
    QTimer *_processTimer;
    QTime _lastEventTime;
};

}

#endif
