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

#include "logger.h"

#include <QDir>
#include <QStringList>
#include <QThread>
#include <qmetaobject.h>

#include "csync.h"

namespace OCC {

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// logging handler.
static void mirallLogCatcher(QtMsgType type, const char *msg)
{
  Q_UNUSED(type)
  // qDebug() exports to local8Bit, which is not always UTF-8
  Logger::instance()->mirallLog( QString::fromLocal8Bit(msg) );
}
static void qInstallMessageHandler(QtMsgHandler h) {
    qInstallMsgHandler(h);
}
#elif QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
static void mirallLogCatcher(QtMsgType, const QMessageLogContext &ctx, const QString &message) {
    QByteArray file = ctx.file;
    file = file.mid(file.lastIndexOf('/') + 1);
    Logger::instance()->mirallLog( QString::fromLocal8Bit(file) + QLatin1Char(':') + QString::number(ctx.line)
                                    + QLatin1Char(' ')  + message) ;
}
#else
static void mirallLogCatcher(QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    auto logger = Logger::instance();
    if (!logger->isNoop()) {
        logger->doLog( qFormatLogMessage(type, ctx, message) ) ;
    }
}
#endif

static void csyncLogCatcher(int /*verbosity*/,
                        const char * /*function*/,
                        const char *buffer,
                        void * /*userdata*/)
{
    Logger::instance()->csyncLog( QString::fromUtf8(buffer) );
}

Logger *Logger::instance()
{
    static Logger log;
    return &log;
}

Logger::Logger( QObject* parent) : QObject(parent),
  _showTime(true), _logWindowActivated(false), _doFileFlush(false), _logExpire(0)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    qSetMessagePattern("%{time MM-dd hh:mm:ss:zzz} [ %{type} %{category} ]%{if-debug}\t[ %{function} ]%{endif}:\t%{message}");
#endif
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(mirallLogCatcher);
#else
    Q_UNUSED(mirallLogCatcher)
#endif
}

Logger::~Logger() {
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(0);
#endif
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
    emit guiMessage(title, message);
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
    msg += QString().sprintf("%p ", (void*)QThread::currentThread());
    msg += log.message;
    // _logs.append(log);
    // std::cout << qPrintable(log.message) << std::endl;

    doLog(msg);
}

/**
 * Returns true if doLog does nothing and need not to be called
 */
bool Logger::isNoop() const
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    return false;
#else
    QMutexLocker lock(const_cast<QMutex *>(&_mutex));
    return !_logstream && !_logWindowActivated;
#endif
}


void Logger::doLog(const QString& msg)
{
    {
        QMutexLocker lock(&_mutex);
        if( _logstream ) {
            (*_logstream) << msg << endl;
            if( _doFileFlush ) _logstream->flush();
        }
    }
    emit logWindowLog(msg);
}

void Logger::csyncLog( const QString& message )
{
    auto logger = Logger::instance();
    if (logger->isNoop()) {
        return;
    }

    Log log;
    log.source = Log::CSync;
    log.timeStamp = QDateTime::currentDateTime();
    log.message = message;

    logger->log(log);
}

void Logger::mirallLog( const QString& message )
{
    Log log_;
    log_.source = Log::Occ;
    log_.timeStamp = QDateTime::currentDateTime();
    log_.message = message;

    Logger::instance()->log( log_ );
}

void Logger::setLogWindowActivated(bool activated)
{
    QMutexLocker locker(&_mutex);

    // Setup CSYNC logging to forward to our own logger
    csync_set_log_callback(csyncLogCatcher);
    csync_set_log_level(11);

    _logWindowActivated = activated;
}

void Logger::setLogFile(const QString & name)
{
    QMutexLocker locker(&_mutex);

    // Setup CSYNC logging to forward to our own logger
    csync_set_log_callback(csyncLogCatcher);
    csync_set_log_level(11);

    if( _logstream ) {
        _logstream.reset(0);
        _logFile.close();
    }

    if( name.isEmpty() ) {
        return;
    }

    bool openSucceeded = false;
    if (name == QLatin1String("-")) {
        openSucceeded = _logFile.open(1, QIODevice::WriteOnly);
    } else {
        _logFile.setFileName( name );
        openSucceeded = _logFile.open(QIODevice::WriteOnly);
    }

    if(!openSucceeded) {
        locker.unlock(); // Just in case postGuiMessage has a qDebug()
        postGuiMessage( tr("Error"),
                        QString(tr("<nobr>File '%1'<br/>cannot be opened for writing.<br/><br/>"
                                   "The log output can <b>not</b> be saved!</nobr>"))
                        .arg(name));
        return;
    }

    _logstream.reset(new QTextStream( &_logFile ));
}

void Logger::setLogExpire( int expire )
{
    _logExpire = expire;
}

void Logger::setLogDir( const QString& dir )
{
    _logDirectory = dir;
}

void Logger::setLogFlush( bool flush )
{
    _doFileFlush = flush;
}

void Logger::enterNextLogFile()
{
    if (!_logDirectory.isEmpty()) {
        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Find out what is the file with the highest number if any
        QStringList files = dir.entryList(QStringList("owncloud.log.*"),
                                    QDir::Files);
        QRegExp rx("owncloud.log.(\\d+)");
        uint maxNumber = 0;
        QDateTime now = QDateTime::currentDateTime();
        foreach(const QString &s, files) {
            if (rx.exactMatch(s)) {
                maxNumber = qMax(maxNumber, rx.cap(1).toUInt());
                if (_logExpire > 0) {
                    QFileInfo fileInfo = dir.absoluteFilePath(s);
                    if (fileInfo.lastModified().addSecs(60*60 * _logExpire) < now) {
                        dir.remove(s);
                    }
                }
            }
        }

        QString filename = _logDirectory + "/owncloud.log." + QString::number(maxNumber+1);
        setLogFile(filename);
    }
}

} // namespace OCC
