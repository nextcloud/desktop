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
    void openFileAtPath(const QString path);
    void releaseFileAtPath(const QString path);
    void writeFileAtPath(const QString path);

signals:
    void syncFinish(const QString &, bool);
    void startSyncForFolder();

private:
    SyncWrapper() {
        _syncJournalDb = SyncJournalDb::instance();
        _folderMan = FolderMan::instance();
        _folder = FolderMan::instance()->currentSyncFolder();
        connect(_syncJournalDb, &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::updateSyncQueue, Qt::DirectConnection);
        connect(_syncJournalDb, &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::syncFinish, Qt::DirectConnection);
        connect(this, &SyncWrapper::startSyncForFolder, _folder, &Folder::startSync, Qt::DirectConnection);
    }


    QString removeSlash(QString path);
    bool shouldSync(const QString path);
    void sync(const QString path, csync_instructions_e instruction = CSYNC_INSTRUCTION_SYNC);

    QMap<QString, bool> _syncDone;

    SyncJournalDb *_syncJournalDb;
    FolderMan *_folderMan;
    Folder *_folder;
};
}

#endif // SYNCWRAPPER_H
