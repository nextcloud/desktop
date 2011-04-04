#ifndef MIRALL_SYNCRESULT_H
#define MIRALL_SYNCRESULT_H

#include <QStringList>

namespace Mirall
{

class SyncResult
{
public:
    SyncResult();
    ~SyncResult();
private:
    QStringList _deletedSource;
    QStringList _deletedDestination;
};

}

#endif
