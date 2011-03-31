#ifndef MIRALL_UNISONFOLDER_H
#define MIRALL_UNISONFOLDER_H

#include <QMutex>
#include <QProcess>
#include <QStringList>

#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class UnisonFolder : public Folder
{
    Q_OBJECT
public:
    UnisonFolder(const QString &path, const QString &secondPath, QObject *parent = 0L);
    virtual ~UnisonFolder();

    QString secondPath() const;

    virtual void startSync(const QStringList &pathList);

    virtual bool isBusy() const;

protected slots:
    void slotReadyReadStandardOutput();
    void slotReadyReadStandardError();
    void slotStateChanged(QProcess::ProcessState);
    void slotFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotStarted();
    void slotError(QProcess::ProcessError);
private:
    QMutex _syncMutex;
    QProcess *_unison;
    QString _secondPath;
    int _syncCount;
};

}

#endif
