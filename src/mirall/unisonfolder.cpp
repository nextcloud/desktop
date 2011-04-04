#include <QDebug>
#include <QMutexLocker>
#include <QStringList>
#include <QDir>
#include "mirall/unisonfolder.h"

namespace Mirall {

UnisonFolder::UnisonFolder(const QString &path, const QString &secondPath, QObject *parent)
    : Folder(path, parent),
      _unison(new QProcess(this)),
      _secondPath(secondPath),
      _syncCount(0)
{
    QObject::connect(_unison, SIGNAL(readyReadStandardOutput()),
                     SLOT(slotReadyReadStandardOutput()));

    QObject::connect(_unison, SIGNAL(readyReadStandardError()),
                     SLOT(slotReadyReadStandardError()));

    QObject::connect(_unison, SIGNAL(stateChanged(QProcess::ProcessState)),
                     SLOT(slotStateChanged(QProcess::ProcessState)));

    QObject::connect(_unison, SIGNAL(error(QProcess::ProcessError)),
                     SLOT(slotError(QProcess::ProcessError)));

    QObject::connect(_unison, SIGNAL(started()),
                     SLOT(slotStarted()));

    QObject::connect(_unison, SIGNAL(finished(int, QProcess::ExitStatus)),
                     SLOT(slotFinished(int, QProcess::ExitStatus)));
}

UnisonFolder::~UnisonFolder()
{
}

bool UnisonFolder::isBusy() const
{
    return (_unison->state() != QProcess::NotRunning);
}

QString UnisonFolder::secondPath() const
{
    return _secondPath;
}

void UnisonFolder::startSync(const QStringList &pathList)
{
    QMutexLocker locker(&_syncMutex);

    emit syncStarted();

    QString program = "unison";
    QStringList args;
    args << "-ui" << "text";
    args << "-auto" << "-batch";

    //args << "-confirmbigdel";

    // only use -path in after a full synchronization
    // already happened, which we do only on the first
    // sync when the program is started
    if (_syncCount > 0 ) {
        // may be we should use a QDir in the API itself?
        QDir root(path());
        foreach(QString changedPath, pathList) {
            args << "-path" << root.relativeFilePath(changedPath);
        }
    }

    args  << path();
    args  << secondPath();

    _unison->start(program, args);
}

void UnisonFolder::slotStarted()
{
    qDebug() << "    * Unison process started ( PID " << _unison->pid() << ")";
    _syncCount++;

    //qDebug() << _unison->readAllStandardOutput();;
}

void UnisonFolder::slotFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "    * Unison process finished with status" << exitCode;
    emit syncFinished();
}

void UnisonFolder::slotReadyReadStandardOutput()
{
    qDebug() << _unison->readAllStandardOutput();;
}

void UnisonFolder::slotReadyReadStandardError()
{
    //qDebug() << _unison->readAllStandardError();;
}

void UnisonFolder::slotStateChanged(QProcess::ProcessState state)
{
    //qDebug() << "changed: " << state;
}

void UnisonFolder::slotError(QProcess::ProcessError error)
{
    //qDebug() << "error: " << error;
}

} // ns

#include "unisonfolder.moc"
