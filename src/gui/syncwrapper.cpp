#include "syncwrapper.h"
#include "socketapi.h"

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
    _syncDone.insert(path, syncing);
}

void SyncWrapper::openFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::deleteFileAtPath(const QString path)
{
    sync(removeSlash(path), CSYNC_INSTRUCTION_REMOVE);
}

void SyncWrapper::releaseFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_EVAL);
}

void SyncWrapper::writeFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_NEW);
}

void SyncWrapper::updateFileTree(const QString path){
    FolderMan::instance()->currentSyncFolder()->updateLocalFileTree(removeSlash(path), CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::sync(const QString path, csync_instructions_e instruction){
    int result = 1;
    if (SyncJournalDb::instance()->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_ONLINE) {
        result = SyncJournalDb::instance()->setSyncMode(path, SyncJournalDb::SyncMode::SYNCMODE_OFFLINE);
    } else if (SyncJournalDb::instance()->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE) {
        result = SyncJournalDb::instance()->setSyncMode(path, SyncJournalDb::SyncMode::SYNCMODE_ONLINE);
    }

    if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't change file SYNCMODE.";

   result = SyncJournalDb::instance()->setSyncModeDownload(path, SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NO);
   if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_DOWNLOADED_NO.";

   if(result == 1){
       FolderMan::instance()->currentSyncFolder()->updateLocalFileTree(path, instruction);
       _syncDone.insert(path, false);
       SyncJournalDb::instance()->updateLastAccess(path);

       if(shouldSync(path)){
           //_folderMan->terminateSyncProcess();
           FolderMan::instance()->scheduleFolder();
           //_folderMan->scheduleFolderNext();
           //emit startSyncForFolder();
       } else {
           emit syncFinish(path, true);
       }
   }
}

bool SyncWrapper::shouldSync(const QString path){
    // Checks sync mode
    if (SyncJournalDb::instance()->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE)
        return false;

    // checks if file is cached
    // checks last access

    return true;
}

}
