#include "syncwrapper.h"
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
    _syncDone.insert(path, syncing);
}

void SyncWrapper::openFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::releaseFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_EVAL);
}

void SyncWrapper::writeFileAtPath(const QString path){
    sync(removeSlash(path), CSYNC_INSTRUCTION_NEW);
}

void SyncWrapper::updateFileTree(const QString path){
    _folder->updateLocalFileTree(removeSlash(path), CSYNC_INSTRUCTION_SYNC);
}

void SyncWrapper::sync(const QString path, csync_instructions_e instruction){
    int result = 1;
    if(_syncJournalDb->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_ONLINE){
        result = _syncJournalDb->setSyncMode(path, SyncJournalDb::SyncMode::SYNCMODE_OFFLINE);
    } else if(_syncJournalDb->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE) {
       result = _syncJournalDb->setSyncMode(path, SyncJournalDb::SyncMode::SYNCMODE_ONLINE);
    }

    if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't change file SYNCMODE.";

   result = _syncJournalDb->setSyncModeDownload(path, SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NO);
   if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_DOWNLOADED_NO.";

   if(result == 1){
       _folder->updateLocalFileTree(path, instruction);
       _syncDone.insert(path, false);
       _syncJournalDb->updateLastAccess(path);

       if(shouldSync(path)){
           emit startSyncForFolder();
       } else {
           emit syncFinish(path, true);
       }
   }
}

bool SyncWrapper::shouldSync(const QString path){
    // Checks sync mode
    if(_syncJournalDb->getSyncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE)
        return false;

    // checks if file is cached
    // checks last access

    return true;
}

}
