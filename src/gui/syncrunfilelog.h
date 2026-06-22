/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNCRUNFILELOG_H
#define SYNCRUNFILELOG_H

#include <QFile>
#include <QTextStream>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDir>

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
    QString dateTimeStr(const QDateTime &dt);

    QScopedPointer<QFile> _file;
    QTextStream _out;
    QElapsedTimer _totalDuration;
    QElapsedTimer _lapDuration;
};
}

#endif // SYNCRUNFILELOG_H
