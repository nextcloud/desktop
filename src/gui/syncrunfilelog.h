/*
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

#ifndef SYNCRUNFILELOG_H
#define SYNCRUNFILELOG_H

#include <QFile>
#include <QTextStream>
#include <QScopedPointer>
#include <QElapsedTimer>

#include "syncfileitem.h"

namespace OCC {
class SyncFileItem;

/**
 * @brief The SyncRunFileLog class
 * @ingroup gui
 */
class SyncRunFileLog
{
public:
    SyncRunFileLog();
    void start(const QString &folderPath);
    void logItem(const SyncFileItem &item);
    void logLap(const QString &name);
    void finish();

protected:
private:
    QScopedPointer<QFile> _file;
    QElapsedTimer _totalDuration;
    QElapsedTimer _lapDuration;
    QScopedPointer<QTextStream> _out;
};
}

#endif // SYNCRUNFILELOG_H
