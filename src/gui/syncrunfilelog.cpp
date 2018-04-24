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

#include <QRegExp>

#include "syncrunfilelog.h"
#include "common/utility.h"
#include "filesystem.h"
#include <qfileinfo.h>

namespace OCC {

SyncRunFileLog::SyncRunFileLog()
{
}

QString SyncRunFileLog::dateTimeStr(const QDateTime &dt)
{
    return dt.toString(Qt::ISODate);
}

QString SyncRunFileLog::directionToStr(SyncFileItem::Direction dir)
{
    QString re("N");
    if (dir == SyncFileItem::Up) {
        re = QLatin1String("Up");
    } else if (dir == SyncFileItem::Down) {
        re = QLatin1String("Down");
    }
    return re;
}

QString SyncRunFileLog::instructionToStr(csync_instructions_e inst)
{
    QString re;

    switch (inst) {
    case CSYNC_INSTRUCTION_NONE:
        re = "INST_NONE";
        break;
    case CSYNC_INSTRUCTION_EVAL:
        re = "INST_EVAL";
        break;
    case CSYNC_INSTRUCTION_REMOVE:
        re = "INST_REMOVE";
        break;
    case CSYNC_INSTRUCTION_RENAME:
        re = "INST_RENAME";
        break;
    case CSYNC_INSTRUCTION_EVAL_RENAME:
        re = "INST_EVAL_RENAME";
        break;
    case CSYNC_INSTRUCTION_NEW:
        re = "INST_NEW";
        break;
    case CSYNC_INSTRUCTION_CONFLICT:
        re = "INST_CONFLICT";
        break;
    case CSYNC_INSTRUCTION_IGNORE:
        re = "INST_IGNORE";
        break;
    case CSYNC_INSTRUCTION_SYNC:
        re = "INST_SYNC";
        break;
    case CSYNC_INSTRUCTION_STAT_ERROR:
        re = "INST_STAT_ERR";
        break;
    case CSYNC_INSTRUCTION_ERROR:
        re = "INST_ERROR";
        break;
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
        re = "INST_TYPE_CHANGE";
        break;
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        re = "INST_METADATA";
        break;
    }

    return re;
}


void SyncRunFileLog::start(const QString &folderPath)
{
    const qint64 logfileMaxSize = 1024 * 1024; // 1MiB

    const QString foldername = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QDir(foldername).exists()) {
        QDir().mkdir(foldername);
    }

    int length = folderPath.split(QString(QDir::separator())).length();
    QString filenameRaw = foldername + QString(QDir::separator())  ///
          + folderPath.split(QString(QDir::separator())).at(length - 2);
    QString filename = filenameRaw + QLatin1String("_sync.log");

    while(QFile::exists(filename)) {    
       
       QFile file(filename);
       file.open(QIODevice::ReadOnly| QIODevice::Text);
       QTextStream in(&file);
       QString line = in.readLine();

       if(QString::compare(folderPath,line,Qt::CaseSensitive)!=0) {
           filename = filenameRaw + QLatin1String("_1") + QLatin1String("_sync.log");
	   filenameRaw += QLatin1String("_1");
       }
       else break;
    }

    // When the file is too big, just rename it to an old name.
    QFileInfo info(filename);
    bool exists = info.exists();
    if (exists && info.size() > logfileMaxSize) {
        exists = false;
        QString newFilename = filename + QLatin1String(".1");
        QFile::remove(newFilename);
        QFile::rename(filename, newFilename);
    }
    _file.reset(new QFile(filename));

    _file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    _out.setDevice(_file.data());


    if (!exists) {
        _out << folderPath.toLatin1() << endl;
        // We are creating a new file, add the note.
        _out << "# timestamp | duration | file | instruction | dir | modtime | etag | "
                "size | fileId | status | errorString | http result code | "
                "other size | other modtime | other etag | other fileId | "
                "other instruction"
             << endl;

        FileSystem::setFileHidden(filename, true);
    }


    _totalDuration.start();
    _lapDuration.start();
    _out << "#=#=#=# Syncrun started " << dateTimeStr(QDateTime::currentDateTimeUtc()) << endl;
}

void SyncRunFileLog::logItem(const SyncFileItem &item)
{
    // don't log the directory items that are in the list
    if (item._direction == SyncFileItem::None) {
        return;
    }
    QString ts = QString::fromLatin1(item._responseTimeStamp);
    if (ts.length() > 6) {
        QRegExp rx("(\\d\\d:\\d\\d:\\d\\d)");
        if (ts.contains(rx)) {
            ts = rx.cap(0);
        }
    }

    const QChar L = QLatin1Char('|');
    _out << ts << L;
    _out << L;
    if (item._instruction != CSYNC_INSTRUCTION_RENAME) {
        _out << item._file << L;
    } else {
        _out << item._file << QLatin1String(" -> ") << item._renameTarget << L;
    }
    _out << instructionToStr(item._instruction) << L;
    _out << directionToStr(item._direction) << L;
    _out << QString::number(item._modtime) << L;
    _out << item._etag << L;
    _out << QString::number(item._size) << L;
    _out << item._fileId << L;
    _out << item._status << L;
    _out << item._errorString << L;
    _out << QString::number(item._httpErrorCode) << L;
    _out << QString::number(item._previousSize) << L;
    _out << QString::number(item._previousModtime) << L;
    _out /* << other etag (removed) */ << L;
    _out /* << other fileId (removed) */ << L;
    _out /* << other instruction (removed) */ << L;

    _out << endl;
}

void SyncRunFileLog::logLap(const QString &name)
{
    _out << "#=#=#=#=# " << name << " " << dateTimeStr(QDateTime::currentDateTimeUtc())
         << " (last step: " << _lapDuration.restart() << " msec"
         << ", total: " << _totalDuration.elapsed() << " msec)" << endl;
}

void SyncRunFileLog::finish()
{
    _out << "#=#=#=# Syncrun finished " << dateTimeStr(QDateTime::currentDateTimeUtc())
         << " (last step: " << _lapDuration.elapsed() << " msec"
         << ", total: " << _totalDuration.elapsed() << " msec)" << endl;
    _file->close();
}
}
