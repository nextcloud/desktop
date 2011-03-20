#ifndef MIRALL_UNISONFOLDER_H
#define MIRALL_UNISONFOLDER_H

#include <QMutex>
#include "mirall/folder.h"

class QProcess;

namespace Mirall {

class UnisonFolder : public Folder
{
public:
    UnisonFolder(const QString &path, const QString &secondPath, QObject *parent = 0L);
    virtual ~UnisonFolder();

    QString secondPath() const;

    virtual void startSync();

private:
    QMutex _syncMutex;
    QProcess *_unison;
    QString _secondPath;
};

}

#endif
