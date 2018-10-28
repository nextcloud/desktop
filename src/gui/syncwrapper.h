#ifndef SYNCWRAPPER_H
#define SYNCWRAPPER_H

#include <QObject>
#include <QMap>
#include "common/syncjournaldb.h"

#include <csync.h>

namespace OCC {

class SyncWrapper : public QObject
{
    Q_OBJECT
public:
    static SyncWrapper *instance();
    ~SyncWrapper() {}

    void initSyncMode(const QString path);
    void openFileAtPath(const QString path);
    void releaseFileAtPath(const QString path);
    void writeFileAtPath(const QString path);

private:
    void sync(const QString path);
    bool shouldSync(const QString path);
    QString fixPath(QString path);
    QDateTime lastAccess(const QString path);
    int updateLastAccess(const QString path);

    int syncMode(const QString path);
    int syncModeDownload(const QString path);

    QMap<QString, csync_instructions_e> _syncQueue;

public slots:
    void updateSyncQueue(const QString path, bool syncing);

signals:
    void syncDone(QString, bool);

private:
    SyncWrapper() {
        connect(SyncJournalDb::instance(), &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::updateSyncQueue);
    }
};
}

#endif // SYNCWRAPPER_H
