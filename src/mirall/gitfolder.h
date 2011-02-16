#ifndef MIRALL_GITFOLDER_H
#define MIRALL_GITFOLDER_H

#include <QMutex>
#include "mirall/folder.h"

namespace Mirall {

class GitFolder : public Folder
{
public:
    GitFolder(const QString &path, QObject *parent = 0L);
    virtual ~GitFolder();

    virtual void startSync();
private:
    QMutex _syncMutex;
};

}

#endif
