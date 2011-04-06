/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 *
 * Originally based on example copyright (c) Ashish Shukla
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

#ifndef MIRALL_INOTIFY_H
#define MIRALL_INOTIFY_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QThread>

namespace Mirall
{

class INotify : public QObject
{
    Q_OBJECT

public:

    INotify(int mask);
    ~INotify();

    static void initialize();
    static void cleanup();

    void addPath(const QString &name);
    void removePath(const QString &name);

    QStringList directories() const;
signals:

    void notifyEvent(int mask, int cookie, const QString &name);

private:
    class INotifyThread : public QThread
    {
        int _fd;
        QMap<int, INotify*> _map;
    public:
        INotifyThread(int fd);
        ~INotifyThread();
        void registerForNotification(INotify*, int);
        void unregisterForNotification(INotify*);
        // fireEvent happens from the inotify thread
        // but addPath comes from the main thread
    protected:
        void run();
    private:
        size_t _buffer_size;
        char *_buffer;
    };

    //INotify(int wd);
    void fireEvent(int mask, int cookie, int wd, char *name);
    static int s_fd;
    static INotifyThread* s_thread;

    // the mask is shared for all paths
    int _mask;
    QMap<QString, int> _wds;
};

}

#endif
