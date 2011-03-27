

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
 *
 * Will notify changed files relative to the root()
 * directory.
 *
 * If too many changes happen in a short time interval,
 * it will accumulate and be fired together later.
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

    /**
     * If true, folderChanged() events are sent
     * at least as often as eventInterval() seconds.
     */
    bool eventsEnabled() const;

    /**
     * Enabled or disables folderChanged() events.
     * If disabled, events are accumulated and emptied
     * the next time a folderChanged() event happens.
     */
    void setEventsEnabled(bool enabled);

    /**
     * The minimum amounts of seconds that will separate
     * folderChanged() intervals
     */
    int eventInterval() const;

    /**
     * Sets minimum amounts of seconds that will separate
     * folderChanged() intervals
     */
    void setEventInterval(int seconds);

signals:
    /**
     * Emitted when one of the paths is changed
     */
    void folderChanged(const QStringList &pathList);

protected:
    void setProcessTimer();

protected slots:
    void slotINotifyEvent(int mask, int cookie, const QString &path);
    void slotAddFolderRecursive(const QString &path);
    // called when the manually process timer triggers
    void slotProcessTimerTimeout();
    void slotProcessPaths();
private:
    bool _eventsEnabled;
    int _eventInterval;
    INotify *_inotify;
    QString _root;
    // paths pending to notified
    QStringList _pendingPaths;
    QTimer *_processTimer;
    QTime _lastEventTime;

    // to cancel events that belong to the same action
    int _lastMask;
    QString _lastPath;
};

}

#endif
