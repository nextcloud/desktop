#include <QDebug>
#include <QMutexLocker>
#include <QStringList>
#include <QDir>
#include "mirall/unisonfolder.h"

namespace Mirall {

UnisonFolder::UnisonFolder(const QString &path, const QString &secondPath, QObject *parent)
    : Folder(path, parent),
      _unison(new QProcess(this)),
      _secondPath(secondPath)
{
    QObject::connect(_unison, SIGNAL(readyReadStandardOutput()),
                     SLOT(slotReadyReadStandardOutput()));

    QObject::connect(_unison, SIGNAL(readyReadStandardError()),
                     SLOT(slotReadyReadStandardError()));

    QObject::connect(_unison, SIGNAL(stateChanged(QProcess::ProcessState)),
                     SLOT(slotStateChanged(QProcess::ProcessState)));

    QObject::connect(_unison, SIGNAL(error(QProcess::ProcessError)),
                     SLOT(slotError(QProcess::ProcessError)));
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

void UnisonFolder::startSync(const QStringList &pathList)
{
    QMutexLocker locker(&_syncMutex);

    QString program = "unison";
    QStringList args;
    args << "-ui" << "text";
    args << "-auto" << "-batch";
    args << "-confirmbigdel";

    // may be we should use a QDir in the API itself?
    QDir root(path());
    foreach(QString changedPath, pathList) {
        args << "-path" << root.relativeFilePath(changedPath);
    }

    args  << path();
    args  << secondPath();

    emit syncStarted();

    _unison->start(program, args);

    emit syncFinished();
}

void UnisonFolder::slotReadyReadStandardOutput()
{
    qDebug() << _unison->readAllStandardOutput();;

}

void UnisonFolder::slotReadyReadStandardError()
{
    qDebug() << _unison->readAllStandardError();;

}

void UnisonFolder::slotStateChanged(QProcess::ProcessState state)
{
    qDebug() << "changed: " << state;
}

void UnisonFolder::slotError(QProcess::ProcessError error)
{
    qDebug() << "error: " << error;
}

} // ns

#include "unisonfolder.moc"
