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

#include "mirall/logger.h"

namespace Mirall {

Logger* Logger::_instance=0;

Logger::Logger( QObject* parent)
: QObject(parent),
  _showTime(true)
{
}

Logger *Logger::instance()
{
    if( !Logger::_instance ) Logger::_instance = new Logger;
    return Logger::_instance;
}

void Logger::destroy()
{
    if( Logger::_instance ) {
        delete Logger::_instance;
        Logger::_instance = 0;
    }
}

void Logger::postGuiLog(const QString &title, const QString &message)
{
    emit guiLog(title, message);
}

void Logger::postOptionalGuiLog(const QString &title, const QString &message)
{
    emit optionalGuiLog(title, message);
}

void Logger::postGuiMessage(const QString &title, const QString &message)
{
    emit postGuiMessage(title, message);
}

void Logger::log(Log log)
{
    QString msg;
    if( _showTime ) {
        msg = log.timeStamp.toString(QLatin1String("MM-dd hh:mm:ss:zzz")) + QLatin1Char(' ');
    }

    if( log.source == Log::CSync ) {
        // msg += "csync - ";
    } else {
        // msg += "ownCloud - ";
    }
    msg += log.message;
    // _logs.append(log);
    // std::cout << qPrintable(log.message) << std::endl;
    emit newLog(msg);
}

void Logger::csyncLog( const QString& message )
{
    Log log;
    log.source = Log::CSync;
    log.timeStamp = QDateTime::currentDateTime();
    log.message = message;

    Logger::instance()->log(log);
}

void Logger::mirallLog( const QString& message )
{
    Log log_;
    log_.source = Log::Mirall;
    log_.timeStamp = QDateTime::currentDateTime();
    log_.message = message;

    Logger::instance()->log( log_ );
}

} // namespace Mirall
