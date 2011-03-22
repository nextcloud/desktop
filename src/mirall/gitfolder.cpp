#include <QMutexLocker>
#include <QProcess>
#include "mirall/gitfolder.h"

namespace Mirall {

GitFolder::GitFolder(const QString &path, const QString &remote, QObject *parent)
    : Folder(path, parent)
    , _remote(remote)
{
    _syncProcess = new QProcess();
}

GitFolder::~GitFolder()
{
}

void GitFolder::startSync()
{
    QMutexLocker locker(&_syncMutex);
    emit syncStarted();
    emit syncFinished();
}

} // ns

#include "gitfolder.moc"
