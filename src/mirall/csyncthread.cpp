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

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>

#include <csync.h>
#include "mirall/csyncthread.h"

namespace Mirall {

CSyncThread::CSyncThread(const QString &source, const QString &target)
    : _source(source)
    , _target(target)
    , _error(0)
{

}

CSyncThread::~CSyncThread()
{

}

bool CSyncThread::error() const
{
    return _error != 0;
}

QMutex CSyncThread::_mutex;

void CSyncThread::run()
{
    QMutexLocker locker(&_mutex);

    CSYNC *csync;

    _error = csync_create(&csync,
                          _source.toLocal8Bit().data(),
                          _target.toLocal8Bit().data());
    if (error())
        return;

#if LIBCSYNC_VERSION_INT >= CSYNC_VERSION_INT(0, 45, 0)
    csync_enable_conflictcopys(csync);
#endif

    _error = csync_init(csync);

    if (error())
        goto cleanup;

    _error = csync_update(csync);
    if (error())
        goto cleanup;

    _error = csync_reconcile(csync);
    if (error())
        goto cleanup;

    _error = csync_propagate(csync);
cleanup:
    csync_destroy(csync);
}

}
