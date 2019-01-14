#ifndef SYNCWRAPPER_H
#define SYNCWRAPPER_H

#include <QObject>
#include <QMap>

#include "common/syncjournaldb.h"
#include "folderman.h"
#include "csync.h"

namespace OCC {

class SyncWrapper : public QObject
{
    Q_OBJECT
public:
    static SyncWrapper *instance();
    ~SyncWrapper() {}

    bool syncDone(const QString path);
    int initSyncMode(const QString path);

public slots:
    void updateSyncQueue(const QString path, bool syncing);
    void updateFileTree(const QString path);

    void createFileAtPath(const QString path);
    void deleteFileAtPath(const QString path); 
    void moveFileAtPath(const QString path);
    void openFileAtPath(const QString path);

	void releaseFileAtPath(const QString path);
    void writeFileAtPath(const QString path);

	void createDirectoryAtPath(const QString path);
    void moveDirectoryAtPath(const QString path);

signals:
    void syncFinish(const QString &, bool);
    void startSyncForFolder();

private:
    SyncWrapper() {
        connect(SyncJournalDb::instance(), &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::updateSyncQueue, Qt::DirectConnection);
        connect(SyncJournalDb::instance(), &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::syncFinish, Qt::DirectConnection);
        connect(this, &SyncWrapper::startSyncForFolder, FolderMan::instance()->currentSyncFolder(), &Folder::startSync, Qt::DirectConnection);
    }

    QString getRelativePath(QString path);
    bool shouldSync(const QString path);
    void sync(const QString path, csync_instructions_e instruction = CSYNC_INSTRUCTION_SYNC);

    QMap<QString, bool> _syncDone;
};
}

#endif // SYNCWRAPPER_H
