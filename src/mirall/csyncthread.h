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

#include <QMutex>
#include <QThread>
#include <QString>

class QProcess;

namespace Mirall {

class CSyncThread : public QThread
{
public:
    CSyncThread(const QString &source, const QString &target, bool = false);
    ~CSyncThread();

    virtual void run();

    /**
     * true if last operation ended with error
     */
    bool error() const;

    bool hasLocalChanges( int64_t ) const;
    int64_t walkedFiles();

private:
    static QMutex _mutex;
    QString _source;
    QString _target;
    int _error;
    bool _localCheckOnly;
    bool _localChanges;
    int64_t _walkedFiles;
};
}

#endif // CSYNCTHREAD_H
