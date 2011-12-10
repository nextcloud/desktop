#include "SyncDebug.h"

SyncDebug::SyncDebug(QObject *parent):
    QIODevice(parent)
{
    open(ReadWrite);
}

qint64 SyncDebug::readData(char *data, qint64 length )
{
    return 0;
}

qint64 SyncDebug::writeData(const char* data, qint64 length)
{
    qDebug() << data;
    emit debugMessage(QString(data));
}
