#ifndef SYNCWRAPPER_H
#define SYNCWRAPPER_H

#include <QObject>
#include <QMap>

#include "common/syncjournaldb.h"
#include "folderman.h"
#include "csync.h"

#include <QMap>
#include "syncfileitem.h"

namespace OCC {

class SyncWrapper : public QObject
{
    Q_OBJECT
public:
    static SyncWrapper *instance();
    ~SyncWrapper() {}

public slots:

signals:

private:
    SyncWrapper() {
       
    }

};
}

#endif // SYNCWRAPPER_H
