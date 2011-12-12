#ifndef SYNCDEBUG_H
#define SYNCDEBUG_H

#include <QIODevice>
#include <QDebug>

class SyncDebug : public QIODevice
{
    Q_OBJECT
public:
    explicit SyncDebug( QObject *parent = 0 );

    qint64 readData(char *data, qint64 length );
    qint64 writeData(const char* data, qint64 length);

signals:
    void debugMessage(const QString);

public slots:

};

#if !defined(QT_NO_DEBUG_STREAM)
Q_GLOBAL_STATIC( SyncDebug, getSyncDebug)
Q_CORE_EXPORT_INLINE QDebug syncDebug() { return QDebug(getSyncDebug()); }

#else // QT_NO_DEBUG_STREAM

#endif

#endif // SYNCDEBUG_H
