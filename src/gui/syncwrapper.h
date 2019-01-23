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

public slots:
    //void updateSyncQueue();
    void updateFileTree(int type, const QString path);

	void createItemAtPath(const QString path);
    void openFileAtPath(const QString path);
    void writeFileAtPath(const QString path);
	void releaseFileAtPath(const QString path);
    void deleteItemAtPath(const QString path);
    void moveItemAtPath(const QString path);

signals:
    void syncFinish();
	//void startSyncForFolder();

private:
    SyncWrapper() {
        connect(SyncJournalDb::instance(), &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::syncFinish, Qt::DirectConnection);
        //connect(SyncJournalDb::instance(), &SyncJournalDb::syncStatusChanged, this, &SyncWrapper::updateSyncQueue, Qt::DirectConnection);
        //connect(this, &SyncWrapper::startSyncForFolder, FolderMan::instance()->currentSyncFolder(), &Folder::startSync, Qt::DirectConnection);
    }

    QString getRelativePath(QString path);
    bool shouldSync(const QString path);
    void sync(const QString path, bool is_fuse_created_file, csync_instructions_e instruction = CSYNC_INSTRUCTION_NEW);

    //QMap<QString, bool> _syncDone;
};
}

#endif // SYNCWRAPPER_H
