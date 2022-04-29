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

#include <QDate>
#include <QRegExp>

#include "syncrunfilelog.h"
#include "common/utility.h"
#include "filesystem.h"
#include <qfileinfo.h>

namespace {
auto dateTimeStr(const QDateTime &dt = QDateTime::currentDateTimeUtc())
{
    return dt.toString(Qt::ISODate);
}

}
namespace OCC {

SyncRunFileLog::SyncRunFileLog()
{
}


void SyncRunFileLog::start(const QString &folderPath)
{
    const qint64 logfileMaxSize = 10 * 1024 * 1024; // 10MiB

    // Note; this name is ignored in csync_exclude.c
    const QString filename = folderPath + QStringLiteral(".owncloudsync.log");

    // When the file is too big, just rename it to an old name.
    QFileInfo info(filename);
    bool exists = info.exists();
    if (exists && info.size() > logfileMaxSize) {
        exists = false;
        QString newFilename = filename + QStringLiteral(".1");
        QFile::remove(newFilename);
        QFile::rename(filename, newFilename);
    }
    _file.reset(new QFile(filename));
    _file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);


    // we use a text stream to ensure the encoding is ok
    // when outputting info, we use QDebug to ensure we can use the debug operators
    _out.reset(new QTextStream(_file.data()));
    _out->setCodec("UTF-8");


    if (!exists) {
        // We are creating a new file, add the note.
        *_out << "Log for:" << folderPath << endl
              << "# timestamp | duration | file | instruction | dir | modtime | etag | "
                 "size | fileId | status | errorString | http result code | "
                 "other size | other modtime | X-Request-ID"
              << endl;

        FileSystem::setFileHidden(filename, true);
    }


    _totalDuration.start();
    _lapDuration.start();
    *_out << "#=#=#=# Syncrun started " << dateTimeStr() << endl;
}

void SyncRunFileLog::logItem(const SyncFileItem &item)
{
    // don't log the directory items that are in the list
    if (item._direction == SyncFileItem::None
        || item._instruction == CSYNC_INSTRUCTION_IGNORE) {
        return;
    }
    const QChar L = QLatin1Char('|');
    QString tmp;
    {
        QDebug(&tmp).noquote() << dateTimeStr(QDateTime::fromString(QString::fromUtf8(item._responseTimeStamp), Qt::RFC2822Date)) << L
                               << ((item._instruction != CSYNC_INSTRUCTION_RENAME) ? item.destination() : item._file + QStringLiteral(" -> ") + item._renameTarget) << L
                               << item._instruction << L
                               << item._direction << L
                               << L
                               << item._modtime << L
                               << item._etag << L
                               << item._size << L
                               << item._fileId << L
                               << item._status << L
                               << item._errorString << L
                               << item._httpErrorCode << L
                               << item._previousSize << L
                               << item._previousModtime << L
                               << item._requestId << L
                               << endl;
    }
    *_out << tmp;
}

void SyncRunFileLog::logLap(const QString &name)
{
    QString tmp;
    {
        QDebug(&tmp).noquote() << "#=#=#=#=#" << name << dateTimeStr()
                               << "(last step:" << _lapDuration.restart() << "msec"
                               << ", total:" << _totalDuration.elapsed() << "msec)"
                               << endl;
    }
    *_out << tmp;
}

void SyncRunFileLog::finish()
{
    QString tmp;
    {
        QDebug(&tmp).noquote() << "#=#=#=# Syncrun finished" << dateTimeStr()
                               << "(last step:" << _lapDuration.elapsed() << "msec"
                               << ", total:" << _totalDuration.elapsed() << "msec)"
                               << endl;
    }
    *_out << tmp;
    _file->close();
}
}
