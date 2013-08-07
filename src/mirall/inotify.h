/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 *
 * Originally based on example copyright (c) Ashish Shukla
 *
 * Ported to use QSocketNotifier later instead of a thread loop
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

class QSocketNotifier;

namespace Mirall
{

class INotify : public QObject
{
    Q_OBJECT

public:

    INotify(QObject *parent, int mask);
    ~INotify();

    bool addPath(const QString &name);
    void removePath(const QString &name);

    QStringList directories() const;

protected slots:
    void slotActivated(int);

signals:
    void notifyEvent(int mask, int cookie, const QString &name);

private:
    int _fd;
    QSocketNotifier *_notifier;
    // the mask is shared for all paths
    int _mask;
    QMap<QString, int> _wds;

    size_t _buffer_size;
    char *_buffer;
};

}

#endif
