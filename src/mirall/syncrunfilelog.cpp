/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "mirall/syncrunfilelog.h"
#include "mirall/utility.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall {

SyncRunFileLog::SyncRunFileLog()
{
}

QString SyncRunFileLog::dateTimeStr( const QDateTime& dt )
{
    return dt.toString(Qt::ISODate);
}

QString SyncRunFileLog::instructionToStr( csync_instructions_e inst )
{
    QString re;

    switch( inst ) {
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
    case CSYNC_INSTRUCTION_DELETED:
        re = "INST_DELETED";
        break;
    case CSYNC_INSTRUCTION_UPDATED:
        re = "INST_UPDATED";
        break;
    }

    return re;
}


void SyncRunFileLog::start( Utility::StopWatch stopWatch )
{
    MirallConfigFile cfg;
    _file.reset(new QFile(cfg.configPath() + QLatin1String("sync_log") ));

    _file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    _out.setDevice( _file.data() );

    QDateTime dt = stopWatch.startTime();
    QDateTime de = stopWatch.timeOfLap(QLatin1String("Sync Finished"));

    _out << "#=#=#=# Syncrun started " << dateTimeStr(dt) << " until " << dateTimeStr(de) << " ("
            << stopWatch.durationOfLap(QLatin1String("Sync Finished")) << " msec)" << endl;
    _start = true;
}

void SyncRunFileLog::logItem( const SyncFileItem& item )
{
    // don't log the directory items that are in the list
    if( item._direction == SyncFileItem::None ) {
        return;
    }

    // log a list of fields if the log really adds lines.
    if( _start ) {
        _out << "# timestamp | duration | file | instruction | modtime | etag | "
                "size | fileId | status | errorString | http result code | "
                "other size | other modtime | other etag | other fileId | "
                "other instruction" << endl;
        _start = false;
    }

    const QChar L = QLatin1Char('|');
    _out << item._responseTimeStamp << L;
    _out << QString::number(item._requestDuration) << L;
    _out << item._file << L;
    _out << instructionToStr( item._instruction ) << L;
    _out << QString::number(item._modtime) << L;
    _out << item._etag << L;
    _out << QString::number(item._size) << L;
    _out << item._fileId << L;
    _out << item._status << L;
    _out << item._errorString << L;
    _out << QString::number(item._httpErrorCode) << L;
    _out << QString::number(item.other._size) << L;
    _out << QString::number(item.other._modtime) << L;
    _out << item.other._etag << L;
    _out << item.other._fileId << L;
    _out << instructionToStr(item.other._instruction) << L;

    _out << endl;
}

void SyncRunFileLog::close()
{
    _file->close();
}

}
