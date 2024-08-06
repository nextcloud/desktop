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

#include "config.h"

#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>
#include <QTextCodec>
#include <qmetaobject.h>

#include <iostream>

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

#ifdef Q_OS_WIN
#include <io.h> // for stdout
#endif

namespace {

constexpr int CrashLogSize = 20;
constexpr auto MaxLogLinesCount = 50000;

static bool compressLog(const QString &originalName, const QString &targetName)
{
#ifdef ZLIB_FOUND
    QFile original(originalName);
    if (!original.open(QIODevice::ReadOnly))
        return false;
    auto compressed = gzopen(targetName.toUtf8(), "wb");
    if (!compressed) {
        return false;
    }

    while (!original.atEnd()) {
        auto data = original.read(1024 * 1024);
        auto written = gzwrite(compressed, data.data(), data.size());
        if (written != data.size()) {
            gzclose(compressed);
            return false;
        }
    }
    gzclose(compressed);
    return true;
#else
    return false;
#endif
}

}

namespace OCC {

Logger *Logger::instance()
{
    static Logger log;
    return &log;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss:zzz} [ %{type} %{category} %{file}:%{line} "
                                      "]%{if-debug}\t[ %{function} ]%{endif}:\t%{message}"));
    _crashLog.resize(CrashLogSize);
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
        Logger::instance()->doLog(type, ctx, message);
    });
#endif
}

Logger::~Logger()
{
    if (_logstream) {
        _logstream->flush();
    }
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(nullptr);
#endif
}


void Logger::postGuiLog(const QString &title, const QString &message)
{
    emit guiLog(title, message);
}

void Logger::postGuiMessage(const QString &title, const QString &message)
{
    emit guiMessage(title, message);
}

bool Logger::isLoggingToFile() const
{
    QMutexLocker lock(&_mutex);
    return !_logstream.isNull();
}

void Logger::doLog(QtMsgType type, const QMessageLogContext &ctx, const QString &message)
{
    static long long int linesCounter = 0;
    const auto &msg = qFormatLogMessage(type, ctx, message);
#if defined Q_OS_WIN && (defined NEXTCLOUD_DEV || defined QT_DEBUG)
    // write logs to Output window of Visual Studio
    {
        QString prefix;
        switch (type) {
        case QtDebugMsg:
            break;
        case QtInfoMsg:
            break;
        case QtWarningMsg:
            prefix = QStringLiteral("[WARNING] ");
            break;
        case QtCriticalMsg:
            prefix = QStringLiteral("[CRITICAL ERROR] ");
            break;
        case QtFatalMsg:
            prefix = QStringLiteral("[FATAL ERROR] ");
            break;
        }
        auto msgW = QString(prefix + message).toStdWString();
        msgW.append(L"\n");
        OutputDebugString(msgW.c_str());
    }
#endif
    {
        QMutexLocker lock(&_mutex);

        if (linesCounter >= MaxLogLinesCount) {
            linesCounter = 0;
            if (_logstream) {
                _logstream->flush();
            }
            closeNoLock();
            enterNextLogFileNoLock();
        }
        ++linesCounter;

        _crashLogIndex = (_crashLogIndex + 1) % CrashLogSize;
        _crashLog[_crashLogIndex] = msg;

        if (_logstream) {
            (*_logstream) << msg << "\n";
            if (_doFileFlush)
                _logstream->flush();
        }
        if (type == QtFatalMsg) {
            closeNoLock();
#if defined(Q_OS_WIN)
            // Make application terminate in a way that can be caught by the crash reporter
            Utility::crash();
#endif
        }
    }
    emit logWindowLog(msg);
}

void Logger::closeNoLock()
{
    dumpCrashLog();
    if (_logstream)
    {
        _logstream->flush();
        _logFile.close();
        _logstream.reset();
    }
}

QString Logger::logFile() const
{
    QMutexLocker locker(&_mutex);
    return _logFile.fileName();
}

void Logger::setLogFile(const QString &name)
{
    QMutexLocker locker(&_mutex);
    setLogFileNoLock(name);
}

void Logger::setLogExpire(int expire)
{
    _logExpire = expire;
}

QString Logger::logDir() const
{
    return _logDirectory;
}

void Logger::setLogDir(const QString &dir)
{
    _logDirectory = dir;
}

void Logger::setLogFlush(bool flush)
{
    _doFileFlush = flush;
}

void Logger::setLogDebug(bool debug)
{
    const QSet<QString> rules = {debug ? QStringLiteral("nextcloud.*.debug=true") : QString()};
    if (debug) {
        addLogRule(rules);
    } else {
        removeLogRule(rules);
    }
    _logDebug = debug;
}

QString Logger::temporaryFolderLogDirPath() const
{
    return QDir::temp().filePath(QStringLiteral(APPLICATION_SHORTNAME "-logdir"));
}

void Logger::setupTemporaryFolderLogDir()
{
    auto dir = temporaryFolderLogDirPath();
    if (!QDir().mkpath(dir))
        return;
    setLogDebug(true);
    setLogExpire(4 /*hours*/);
    setLogDir(dir);
    _temporaryFolderLogDir = true;
}

void Logger::disableTemporaryFolderLogDir()
{
    if (!_temporaryFolderLogDir)
        return;

    enterNextLogFile();
    setLogDir(QString());
    setLogDebug(false);
    setLogFile(QString());
    _temporaryFolderLogDir = false;
}

void Logger::setLogRules(const QSet<QString> &rules)
{
    _logRules = rules;
    QString tmp;
    QTextStream out(&tmp);
    for (const auto &p : rules) {
        out << p << QLatin1Char('\n');
    }
    qDebug() << tmp;
    QLoggingCategory::setFilterRules(tmp);
}

void Logger::dumpCrashLog()
{
    QFile logFile(QDir::tempPath() + QStringLiteral("/" APPLICATION_NAME "-crash.log"));
    if (logFile.open(QFile::WriteOnly)) {
        QTextStream out(&logFile);
        for (int i = 1; i <= CrashLogSize; ++i) {
            out << _crashLog[(_crashLogIndex + i) % CrashLogSize] << QLatin1Char('\n');
        }
    }
}

void Logger::enterNextLogFileNoLock()
{
    if (!_logDirectory.isEmpty()) {

        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Tentative new log name, will be adjusted if one like this already exists
        const auto now = QDateTime::currentDateTime();
        const auto cLocale = QLocale::c(); // Some system locales generate strings that are incompatible with filesystem
        QString newLogName = cLocale.toString(now, QStringLiteral("yyyyMMdd_HHmm")) + QStringLiteral("_nextcloud.log");

        // Expire old log files and deal with conflicts
        QStringList files = dir.entryList(QStringList("*owncloud.log.*"), QDir::Files, QDir::Name) +
            dir.entryList(QStringList("*nextcloud.log.*"), QDir::Files, QDir::Name);
        static const QRegularExpression rx(QRegularExpression::anchoredPattern(R"(.*(next|own)cloud\.log\.(\d+).*)"));
        int maxNumber = -1;
        foreach (const QString &s, files) {
            if (_logExpire > 0) {
                QFileInfo fileInfo(dir.absoluteFilePath(s));
                if (fileInfo.lastModified().addSecs(60 * 60 * _logExpire) < now) {
                    dir.remove(s);
                }
            }
            const auto rxMatch = rx.match(s);
            if (s.startsWith(newLogName) && rxMatch.hasMatch()) {
                maxNumber = qMax(maxNumber, rxMatch.captured(2).toInt());
            }
        }
        newLogName.append("." + QString::number(maxNumber + 1));

        auto previousLog = _logFile.fileName();
        setLogFileNoLock(dir.filePath(newLogName));

        // Compress the previous log file. On a restart this can be the most recent
        // log file.
        auto logToCompress = previousLog;
        if (logToCompress.isEmpty() && files.size() > 0 && !files.last().endsWith(".gz"))
            logToCompress = dir.absoluteFilePath(files.last());
        if (!logToCompress.isEmpty()) {
            QString compressedName = logToCompress + ".gz";
            if (compressLog(logToCompress, compressedName)) {
                QFile::remove(logToCompress);
            } else {
                QFile::remove(compressedName);
            }
        }
    }
}

void Logger::setLogFileNoLock(const QString &name)
{
    if (_logstream) {
        _logstream.reset(nullptr);
        _logFile.close();
    }

    if (name.isEmpty()) {
        return;
    }

    bool openSucceeded = false;
    if (name == QLatin1String("-")) {
        openSucceeded = _logFile.open(stdout, QIODevice::WriteOnly);
    } else {
        _logFile.setFileName(name);
        openSucceeded = _logFile.open(QIODevice::WriteOnly);
    }

    if (!openSucceeded) {
        postGuiMessage(tr("Error"),
                       QString(tr("<nobr>File \"%1\"<br/>cannot be opened for writing.<br/><br/>"
                                  "The log output <b>cannot</b> be saved!</nobr>"))
                           .arg(name));
        return;
    }

    _logstream.reset(new QTextStream(&_logFile));
}

void Logger::enterNextLogFile()
{
    QMutexLocker locker(&_mutex);
    enterNextLogFileNoLock();
}

} // namespace OCC
