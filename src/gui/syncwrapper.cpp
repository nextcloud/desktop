#include "syncwrapper.h"
#include "folderman.h"
#include "socketapi.h"
#include "vfs_mac.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcSyncWrapper, "nextcloud.gui.wrapper", QtInfoMsg)

SyncWrapper *SyncWrapper::instance()
{
    static SyncWrapper instance;
    return &instance;
}

QString SyncWrapper::removeSlash(QString path){
    if(path.startsWith('/'))
        path.remove(0, 1);

    return path;
}

void SyncWrapper::updateSyncQueue(const QString path, bool syncing) {
    emit syncDone(path, syncing);
}

void SyncWrapper::updateLocalFileTree(const QString &path, csync_instructions_e instruction){
    OCC::FolderMan::instance()->currentSyncFolder()->updateLocalFileTree(removeSlash(path), instruction);
}

void SyncWrapper::openFileAtPath(const QString path){
    updateLocalFileTree(path, CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::releaseFileAtPath(const QString path){
    updateLocalFileTree(path, CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::writeFileAtPath(const QString path){
    updateLocalFileTree(path, CSYNC_INSTRUCTION_NEW);
}

void SyncWrapper::sync(const QString path){
    OCC::FolderMan::instance()->currentSyncFolder()->startSync();
    qDebug() << Q_FUNC_INFO << "Trying to sync file..." << path;
}

QDateTime SyncWrapper::lastAccess(const QString path){
    return SyncJournalDb::instance()->getLastAccess(removeSlash(path));
}

int SyncWrapper::updateLastAccess(const QString path){
    return SyncJournalDb::instance()->updateLastAccess(removeSlash(path));
}

int SyncWrapper::syncMode(const QString path){
    return SyncJournalDb::instance()->getSyncMode(removeSlash(path));
}

QString SyncWrapper::initSyncMode(const QString path){
    QString fixedPath = removeSlash(path);

    int result = 1;
    if(SyncJournalDb::instance()->getSyncMode(fixedPath) != SyncJournalDb::SyncMode::SYNCMODE_OFFLINE){
        result = SyncJournalDb::instance()->setSyncMode(fixedPath, SyncJournalDb::SyncMode::SYNCMODE_OFFLINE);
        if(result == 0)
            qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_OFFLINE.";
    }

   result = SyncJournalDb::instance()->setSyncModeDownload(fixedPath, SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NO);
   if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_DOWNLOADED_NO.";

    return fixedPath;
}

int SyncWrapper::syncModeDownload(const QString path){
    return SyncJournalDb::instance()->getSyncModeDownload(removeSlash(path));
}

bool SyncWrapper::shouldSync(const QString path){
    // Check last access
    // check if it is downloaded
    // etc

    if(syncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE)
        return false;

    return true;
}

}
