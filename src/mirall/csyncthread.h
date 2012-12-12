#ifndef CSYNCTHREAD_H
#define CSYNCTHREAD_H

/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <stdint.h>

#include <QMutex>
#include <QThread>
#include <QString>
#include <QNetworkProxy>

#include <csync.h>

class QProcess;

namespace Mirall {

class CSyncThread : public QObject
{
    Q_OBJECT
public:
    CSyncThread(const QString &source, const QString &target);
    ~CSyncThread();

    static void setConnectionDetails( const QString&, const QString&, const QNetworkProxy& );
    static QString csyncConfigDir();

    const char* proxyTypeToCStr(QNetworkProxy::ProxyType);

    Q_INVOKABLE void startSync();

signals:
    void fileReceived( const QString& );
    void fileRemoved( const QString& );
    void csyncError( const QString& );

    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

private:
    static void progress(const char *remote_url,
                    enum csync_notify_type_e kind,
                    long long o1, long long o2,
                    void *userdata);
    static int getauth(const char *prompt,
                char *buf,
                size_t len,
                int echo,
                int verify,
                void *userdata
    );

    static QMutex _mutex;
    static QString _user;
    static QString _passwd;
    static QNetworkProxy _proxy;

    static QString _csyncConfigDir;

    QString _source;
    QString _target;
};
}

#endif // CSYNCTHREAD_H
