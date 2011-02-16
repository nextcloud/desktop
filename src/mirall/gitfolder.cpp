#include <QMutexLocker>
#include "mirall/gitfolder.h"

namespace Mirall {

GitFolder::GitFolder(const QString &path, QObject *parent)
    : Folder(path, parent)
{
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
