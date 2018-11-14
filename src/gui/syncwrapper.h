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

    void openFileAtPath(const QString path);
    void releaseFileAtPath(const QString path);
    void writeFileAtPath(const QString path);
    bool syncDone(const QString path);

    void updateLocalFileTree(const QString &path, csync_instructions_e instruction = CSYNC_INSTRUCTION_NONE);
    void initSyncMode(const QString path);
    void initSync(const QString path, csync_instructions_e instruction = CSYNC_INSTRUCTION_SYNC);
    void startSync();

private:
    QDateTime lastAccess(const QString path);
    int updateLastAccess(const QString path);

    QString removeSlash(QString path);

    void sync(const QString path, csync_instructions_e instruction);
    int syncMode(const QString path);

    int syncModeDownload(const QString path);
    bool shouldSync(const QString path);

    QMap<QString, bool> _syncDone;

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
