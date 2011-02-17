#ifndef MIRALL_GITFOLDER_H
#define MIRALL_GITFOLDER_H

#include <QMutex>
#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class GitFolder : public Folder
{
Q_OBJECT
public:
    /**
     * path : Local folder to be keep in sync
     * remote: git repo url to sync from/to
     */
    GitFolder(const QString &path, const QString &remote, QObject *parent = 0L);
    virtual ~GitFolder();

    virtual void startSync();
private:
    QMutex _syncMutex;
    QProcess *_syncProcess;
    QString _remote;
};

}

#endif
