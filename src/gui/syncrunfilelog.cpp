/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QRegularExpression>

#include "syncrunfilelog.h"
#include "libsync/configfile.h"
#include "filesystem.h"
#include <qfileinfo.h>

namespace OCC {

SyncRunFileLog::SyncRunFileLog() = default;

QString SyncRunFileLog::dateTimeStr(const QDateTime &dt)
{
    return dt.toString(Qt::ISODate);
}

void SyncRunFileLog::start(const QString &folderPath)
{
    const qint64 logfileMaxSize = 10 * 1024 * 1024; // 10MiB

    const QString logpath = ConfigFile().logDir();
    if(!QDir(logpath).exists()) {
        QDir().mkdir(logpath);
    }

    int length = folderPath.split(QLatin1String("/")).length();
    QString filenameSingle = folderPath.split(QLatin1String("/")).at(length - 2);
    QString filename = logpath + QLatin1String("/") + filenameSingle + QLatin1String("_sync.log");

    int depthIndex = 2;
    while (FileSystem::fileExists(filename)) {

        QFile file(filename);
        file.open(QIODevice::ReadOnly| QIODevice::Text);
        QTextStream in(&file);
        QString line = in.readLine();

        if(QString::compare(folderPath,line,Qt::CaseSensitive)!=0) {
            depthIndex++;
            if(depthIndex <= length) {
                filenameSingle = folderPath.split(QLatin1String("/")).at(length - depthIndex) + QStringLiteral("_") ///
                        + filenameSingle;
                filename = logpath+ QLatin1String("/") + filenameSingle + QLatin1String("_sync.log");
            }
            else {
                filenameSingle = filenameSingle + QLatin1String("_1");
                filename = logpath + QLatin1String("/") + filenameSingle + QLatin1String("_sync.log");
            }
        }
        else break;
    }

    // When the file is too big, just rename it to an old name.
    bool exists = FileSystem::fileExists(filename);
    if (exists && FileSystem::getSize(filename) > logfileMaxSize) {
        exists = false;
        QString newFilename = filename + QLatin1String(".1");
        QFile::remove(newFilename);
        QFile::rename(filename, newFilename);
    }
    _file.reset(new QFile(filename));

    _file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    _out.setDevice(_file.data());


    if (!exists) {
        _out << folderPath << Qt::endl;
        // We are creating a new file, add the note.
        _out << "# timestamp | duration | file | instruction | dir | modtime | etag | "
                "size | fileId | status | errorString | http result code | "
                "other size | other modtime | X-Request-ID"
             << Qt::endl;

        FileSystem::setFileHidden(filename, true);
    }


    _totalDuration.start();
    _lapDuration.start();
    _out << "#=#=#=# Syncrun started " << dateTimeStr(QDateTime::currentDateTimeUtc()) << Qt::endl;
}
void SyncRunFileLog::logItem(const SyncFileItem &item)
{
    // don't log the directory items that are in the list
    if (item._direction == SyncFileItem::None
        || item._instruction == CSYNC_INSTRUCTION_IGNORE) {
        return;
    }
    QString ts = QString::fromLatin1(item._responseTimeStamp);
    if (ts.length() > 6) {
        static const QRegularExpression rx(R"((\d\d:\d\d:\d\d))");
        const auto rxMatch = rx.match(ts);
        if (rxMatch.hasMatch()) {
            ts = rxMatch.captured(0);
        }
    }

    const QChar L = QLatin1Char('|');
    _out << ts << L;
    _out << L;
    if (item._instruction != CSYNC_INSTRUCTION_RENAME) {
        _out << item.destination() << L;
    } else {
        _out << item._file << QLatin1String(" -> ") << item._renameTarget << L;
    }
    _out << item._instruction << L;
    _out << item._direction << L;
    _out << QString::number(item._modtime) << L;
    _out << item._etag << L;
    _out << QString::number(item._size) << L;
    _out << item._fileId << L;
    _out << item._status << L;
    _out << item._errorString << L;
    _out << QString::number(item._httpErrorCode) << L;
    _out << QString::number(item._previousSize) << L;
    _out << QString::number(item._previousModtime) << L;
    _out << item._requestId << L;

    _out << Qt::endl;
}

void SyncRunFileLog::logLap(const QString &name)
{
    _out << "#=#=#=#=# " << name << " " << dateTimeStr(QDateTime::currentDateTimeUtc())
         << " (last step: " << _lapDuration.restart() << " msec"
         << ", total: " << _totalDuration.elapsed() << " msec)" << Qt::endl;
}

void SyncRunFileLog::finish()
{
    _out << "#=#=#=# Syncrun finished " << dateTimeStr(QDateTime::currentDateTimeUtc())
         << " (last step: " << _lapDuration.elapsed() << " msec"
         << ", total: " << _totalDuration.elapsed() << " msec)" << Qt::endl;
    _file->close();
}
}
