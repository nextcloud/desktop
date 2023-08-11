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
#include "configfile.h"
#include "theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QtConcurrent>
#include <QtGlobal>

#include <iostream>

#include <zlib.h>

#ifdef Q_OS_WIN
#include <io.h> // for stdout
#include <fcntl.h>
#include <comdef.h>
#endif

namespace {
constexpr int crashLogSizeC = 20;
constexpr int maxLogSizeC = 1024 * 1024 * 100; // 100 MiB
constexpr int minLogsToKeepC = 5;

#ifdef Q_OS_WIN
bool isDebuggerPresent()
{
    BOOL debugged;
    if (!CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged)) {
        const auto error = GetLastError();
        qDebug() << "Failed to detect debugger" << QString::fromWCharArray(_com_error(error).ErrorMessage());
    }
    return debugged;
}
#endif
}
namespace OCC {

Logger *Logger::instance()
{
    static auto *log = [] {
        auto _log = new Logger;
        qAddPostRoutine([] {
            Logger::instance()->close();
            delete Logger::instance();
        });
        return _log;
    }();
    return log;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
    , _maxLogFiles(std::max(ConfigFile().automaticDeleteOldLogs(), minLogsToKeepC))
{
    qSetMessagePattern(loggerPattern());
    _crashLog.resize(crashLogSizeC);
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
            Logger::instance()->doLog(type, ctx, message);
        });
#endif
}

Logger::~Logger()
{
#ifndef NO_MSG_HANDLER
    qInstallMessageHandler(0);
#endif
}

QString Logger::loggerPattern()
{
    return QStringLiteral("%{time yy-MM-dd hh:mm:ss:zzz} [ %{type} %{category} ]%{if-debug}\t[ %{function} ]%{endif}:\t%{message}");
}

bool Logger::isLoggingToFile() const
{
    QMutexLocker lock(&_mutex);
    return !_logstream.isNull();
}

void Logger::doLog(QtMsgType type, const QMessageLogContext &ctx, const QString &message)
{
    const QString msg = qFormatLogMessage(type, ctx, message) + QLatin1Char('\n');
    {
        QMutexLocker lock(&_mutex);
        _crashLogIndex = (_crashLogIndex + 1) % crashLogSizeC;
        _crashLog[_crashLogIndex] = msg;
        if (_logstream) {
            (*_logstream) << msg;
            if (_doFileFlush)
                _logstream->flush();
        }
#if defined(Q_OS_WIN)
        if (isDebuggerPresent()) {
            OutputDebugStringW(reinterpret_cast<const wchar_t *>(msg.utf16()));
        }
#endif
        if (type == QtFatalMsg) {
            dumpCrashLog();
            close();
#if defined(Q_OS_WIN)
            // Make application terminate in a way that can be caught by the crash reporter
            Utility::crash();
#endif
        }
        if (!_logDirectory.isEmpty()) {
            if (_logFile.size() > maxLogSizeC) {
                rotateLog();
            }
        }
    }
}

void Logger::open(const QString &name)
{
    bool openSucceeded = false;
    if (name == QLatin1Char('-')) {
        attacheToConsole();
        setLogFlush(true);
        openSucceeded = _logFile.open(stdout, QIODevice::WriteOnly);
    } else {
        _logFile.setFileName(name);
        openSucceeded = _logFile.open(QIODevice::WriteOnly);
    }

    if (!openSucceeded) {
        std::cerr << "Failed to open the log file" << std::endl;
        return;
    }
    _logstream.reset(new QTextStream(&_logFile));
    _logstream->setEncoding(QStringConverter::Utf8);
    (*_logstream) << Theme::instance()->aboutVersions(Theme::VersionFormat::OneLiner) << " " << qApp->applicationName() << Qt::endl;
}

void Logger::close()
{
    if (_logstream)
    {
        _logstream->flush();
        _logFile.close();
        _logstream.reset();
    }
}

void Logger::setLogFile(const QString &name)
{
    QMutexLocker locker(&_mutex);
    if (_logstream) {
        _logstream.reset(nullptr);
        _logFile.close();
    }

    if (name.isEmpty()) {
        return;
    }

    open(name);
}

void Logger::setMaxLogFiles(int i)
{
    _maxLogFiles = std::max(i, std::max(ConfigFile().automaticDeleteOldLogs(), minLogsToKeepC));
}

void Logger::setLogDir(const QString &dir)
{
    _logDirectory = dir;
    rotateLog();
}

void Logger::setLogFlush(bool flush)
{
    _doFileFlush = flush;
}

void Logger::setLogDebug(bool debug)
{
    const QSet<QString> rules = {QStringLiteral("sync.*.debug=true"), QStringLiteral("gui.*.debug=true")};
    if (debug) {
        addLogRule(rules);
    } else {
        removeLogRule(rules);
    }
    _logDebug = debug;
}

QString Logger::temporaryFolderLogDirPath() const
{
    return QDir::temp().absoluteFilePath(QStringLiteral("%1-logdir").arg(QCoreApplication::applicationName()));
}

void Logger::setupTemporaryFolderLogDir()
{
    auto dir = temporaryFolderLogDirPath();
    if (!QDir().mkpath(dir))
        return;
    setLogDebug(true);
    setLogDir(dir);
    _temporaryFolderLogDir = true;
}

void Logger::disableTemporaryFolderLogDir()
{
    if (!_temporaryFolderLogDir)
        return;
    setLogDir(QString());
    setLogDebug(false);
    setLogFile(QString());
    _temporaryFolderLogDir = false;
}

void Logger::setLogRules(const QSet<QString> &rules)
{
    static const QString defaultRule = qEnvironmentVariable("QT_LOGGING_RULES").replace(QLatin1Char(';'), QLatin1Char('\n'));
    _logRules = rules;
    QString tmp;
    QTextStream out(&tmp);
    for (const auto &p : rules) {
        out << p << QLatin1Char('\n');
    }
    out << defaultRule;
    qDebug() << tmp;
    QLoggingCategory::setFilterRules(tmp);
}

void Logger::dumpCrashLog()
{
    QFile logFile(QStringLiteral("%1/%2-crash.log").arg(QDir::tempPath(), qApp->applicationName()));
    if (logFile.open(QFile::WriteOnly)) {
        QTextStream out(&logFile);
        out.setEncoding(QStringConverter::Utf8);
        for (int i = 1; i <= crashLogSizeC; ++i) {
            out << _crashLog[(_crashLogIndex + i) % crashLogSizeC];
        }
    }
}

static bool compressLog(const QString &originalName, const QString &targetName)
{
    QFile original(originalName);
    if (!original.open(QIODevice::ReadOnly))
        return false;
    auto compressed = gzopen(targetName.toUtf8().constData(), "wb");
    if (!compressed) {
        return false;
    }

    while (!original.atEnd()) {
        auto data = original.read(1024 * 1024);
        auto written = gzwrite(compressed, data.constData(), data.size());
        if (written != data.size()) {
            gzclose(compressed);
            return false;
        }
    }
    gzclose(compressed);
    return true;
}

void Logger::rotateLog()
{
    if (!_logDirectory.isEmpty()) {

        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        // Tentative new log name, will be adjusted if one like this already exists
        const QString logName = dir.filePath(QStringLiteral("%1.log").arg(qApp->applicationName()));
        QString previousLog;

        if (_logFile.isOpen()) {
            _logFile.close();
        }
        // rename previous log file if size != 0
        const auto info = QFileInfo(logName);
        if (info.exists(logName) && info.size() != 0) {
            previousLog =
                dir.filePath(QStringLiteral("%1-%2.log").arg(qApp->applicationName(), info.birthTime().toString(QStringLiteral("MMdd_hh.mm.ss.zzz"))));
            if (!QFile(logName).rename(previousLog)) {
                std::cerr << "Failed to rename: " << qPrintable(logName) << " to " << qPrintable(previousLog) << std::endl;
            }
        }

        const auto now = QDateTime::currentDateTime();
        open(logName);
        // set the creation time to now
        _logFile.setFileTime(now, QFileDevice::FileTime::FileBirthTime);

        QtConcurrent::run([now, previousLog, dir, maxLogFiles = _maxLogFiles] {
            // Compress the previous log file.
            if (!previousLog.isEmpty() && QFileInfo::exists(previousLog)) {
                QString compressedName = QStringLiteral("%1.gz").arg(previousLog);
                if (compressLog(previousLog, compressedName)) {
                    QFile::remove(previousLog);
                } else {
                    QFile::remove(compressedName);
                }
            }

            // Expire old log files and deal with conflicts
            {
                auto oldLogFiles = dir.entryList(QStringList(QStringLiteral("*%1-*.log.gz").arg(qApp->applicationName())), QDir::Files, QDir::Name);

                // keeping the last maxLogFiles files in total (need to subtract one from maxLogFiles to ensure the limit)
                std::sort(oldLogFiles.begin(), oldLogFiles.end(), std::greater<QString>());
                oldLogFiles.erase(oldLogFiles.begin(), oldLogFiles.begin() + std::min<qsizetype>(maxLogFiles - 1, oldLogFiles.size()));

                for (const auto &s : oldLogFiles) {
                    if (!QFile::remove(dir.absoluteFilePath(s))) {
                        std::cerr << "warning: failed to remove old log file" << qPrintable(s) << std::endl;
                    }
                }
            }
        });
    }
}

void OCC::Logger::attacheToConsole()
{
    if (_consoleIsAttached) {
        return;
    }
    _consoleIsAttached = true;
#ifdef Q_OS_WIN
    if (!isDebuggerPresent() && AttachConsole(ATTACH_PARENT_PROCESS)) {
        // atache to the parent console output, if its an interactive terminal
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
    }
#endif
}

} // namespace OCC
