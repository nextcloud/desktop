#include <QMutexLocker>
#include <QProcess>
#include <QStringList>
#include "mirall/unisonfolder.h"

namespace Mirall {

UnisonFolder::UnisonFolder(const QString &path, const QString &secondPath, QObject *parent)
    : Folder(path, parent),
      _unison(new QProcess(this)),
      _secondPath(secondPath)
{
}

UnisonFolder::~UnisonFolder()
{
}

bool UnisonFolder::isSyncing() const
{
    return false;
}

QString UnisonFolder::secondPath() const
{
    return _secondPath;
}

void UnisonFolder::startSync()
{
    QMutexLocker locker(&_syncMutex);

    QString program = "unison";
    QStringList args;
    args << "-ui" << "text";
    args << "-auto" << "-batch";
    args << "-confirmbigdel";
    //args << "-path";
    args  << path();
    args  << secondPath();

    _unison->start(program, args);

    emit syncStarted();
    emit syncFinished();
}

} // ns

#include "unisonfolder.moc"
