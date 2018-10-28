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

QString SyncWrapper::fixPath(QString path){
    if(path.startsWith('/'))
        path.remove(0, 1);

    return path;
}

void SyncWrapper::updateSyncQueue(const QString path, bool syncing) {
    emit syncDone(path, syncing);
}

void SyncWrapper::openFileAtPath(const QString path){
    if(!_syncQueue.contains(fixPath(path)) || _syncQueue.value(fixPath(path)) != CSYNC_INSTRUCTION_SYNC){
        _syncQueue.insert(fixPath(path), CSYNC_INSTRUCTION_SYNC);
        //sync(fixPath(path));
    }
}

void SyncWrapper::releaseFileAtPath(const QString path){
    if(!_syncQueue.contains(fixPath(path)) || _syncQueue.value(fixPath(path)) != CSYNC_INSTRUCTION_SYNC){
        _syncQueue.insert(fixPath(path), CSYNC_INSTRUCTION_SYNC);
        //sync(fixPath(path));
    }
}

void SyncWrapper::writeFileAtPath(const QString path){
    if(!_syncQueue.contains(fixPath(path)) || _syncQueue.value(fixPath(path)) != CSYNC_INSTRUCTION_NEW){
        _syncQueue.insert(fixPath(path), CSYNC_INSTRUCTION_NEW);
        //sync(fixPath(path));
    }
}

void SyncWrapper::sync(const QString path){
//    SyncJournalDb::instance()->setSyncMode(path, SyncJournalDb::SyncMode::SYNCMODE_OFFLINE);
//    SyncJournalDb::instance()->setSyncModeDownload(path, SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NO);
    qCInfo(lcSyncWrapper) << "Path: " << path << "has SyncMode"<< SyncJournalDb::instance()->getSyncMode(path);
    qCInfo(lcSyncWrapper) << "Path: " << path << "has SyncModeDownload"<< SyncJournalDb::instance()->getSyncModeDownload(path);

    FolderMan *folderMan = OCC::FolderMan::instance();

    qCInfo(lcSyncWrapper) << "Current sync folder: " << folderMan->currentSyncFolder()->path();
    folderMan->currentSyncFolder()->updateFuseDiscoveryPaths(path, _syncQueue.value(path));
    //folderMan->currentSyncFolder()->startFuseSync();
    if(!folderMan->currentSyncFolder()->isBusy())
        folderMan->currentSyncFolder()->startSync();

    qDebug() << Q_FUNC_INFO << "Trying to download file..." << path;
}

QDateTime SyncWrapper::lastAccess(const QString path){
    return SyncJournalDb::instance()->getLastAccess(fixPath(path));
}

int SyncWrapper::updateLastAccess(const QString path){
    return SyncJournalDb::instance()->updateLastAccess(fixPath(path));
}

int SyncWrapper::syncMode(const QString path){
    return SyncJournalDb::instance()->getSyncMode(fixPath(path));
}

void SyncWrapper::initSyncMode(const QString path){
    QString fixedPath = fixPath(path);

    // Let's try to download them all
    //if(SyncJournalDb::instance()->getSyncMode(fixedPath) == SyncJournalDb::SyncMode::SYNCMODE_NONE)
    int result = SyncJournalDb::instance()->setSyncMode(fixedPath, SyncJournalDb::SyncMode::SYNCMODE_OFFLINE);
    if(result == 0)
        qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_OFFLINE.";

    if(SyncJournalDb::instance()->getSyncModeDownload(fixedPath) == SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NONE){
        result = SyncJournalDb::instance()->setSyncModeDownload(fixedPath, SyncJournalDb::SyncModeDownload::SYNCMODE_DOWNLOADED_NO);
        if(result == 0)
            qCWarning(lcSyncWrapper) << "Couldn't set file to SYNCMODE_DOWNLOADED_NO.";
    }
}

int SyncWrapper::syncModeDownload(const QString path){
    return SyncJournalDb::instance()->getSyncModeDownload(fixPath(path));
}

bool SyncWrapper::shouldSync(const QString path){
    if(syncMode(path) == SyncJournalDb::SyncMode::SYNCMODE_NONE)
        return false;

    return true;
}

}
