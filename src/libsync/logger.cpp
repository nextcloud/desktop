/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
constexpr auto MaxLogLinesBeforeFlush = 10;

static QtMessageHandler s_originalMessageHandler = nullptr;

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

Q_LOGGING_CATEGORY(lcPermanentLog, "nextcloud.log.permanent")

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
    s_originalMessageHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
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
#if defined Q_OS_WIN && ((defined NEXTCLOUD_DEV && NEXTCLOUD_DEV) || defined QT_DEBUG)
    // write logs to Output window of Visual Studio
    {
        const auto msgW = QStringLiteral("%1\n").arg(msg).toStdWString();
        OutputDebugString(msgW.c_str());
    }
#elif defined Q_OS_MACOS && defined QT_DEBUG
    // write logs to Xcode console (stderr)
    {
        std::cerr << msg.toStdString() << std::endl;
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
            enterNextLogFileNoLock(QStringLiteral("nextcloud.log"), LogType::Log);
        }
        ++linesCounter;

        _crashLogIndex = (_crashLogIndex + 1) % CrashLogSize;
        _crashLog[_crashLogIndex] = msg;

        if (_logstream) {
            (*_logstream) << msg << "\n";
            ++_linesCounter;
            if (_doFileFlush ||
                _linesCounter >= MaxLogLinesBeforeFlush ||
                type == QtMsgType::QtWarningMsg || type == QtMsgType::QtCriticalMsg || type == QtMsgType::QtFatalMsg) {
                _logstream->flush();
                _linesCounter = 0;
            }
        }
        if (_permanentDeleteLogStream && ctx.category && strcmp(ctx.category, lcPermanentLog().categoryName()) == 0) {
            (*_permanentDeleteLogStream) << msg << "\n";
            _permanentDeleteLogStream->flush();
            if (_permanentDeleteLogFile.size() > 10LL * 1024LL) {
                enterNextLogFileNoLock(QStringLiteral("permanent_delete.log"), LogType::DeleteLog);
            }
        }
        if (type == QtFatalMsg) {
            dumpCrashLog();
            closeNoLock();
            s_originalMessageHandler(type, ctx, message);
        }
    }
    emit logWindowLog(msg);
}

void Logger::closeNoLock()
{
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

void Logger::setPermanentDeleteLogFile(const QString &name)
{
    QMutexLocker locker(&_mutex);
    setPermanentDeleteLogFileNoLock(name);
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
    if (!QDir().mkpath(dir)) {
        return;
    }

    // Since we're using the temp folder, lock down permissions to owner only
    QFile::Permissions perm = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    QFile file(dir);
    file.setPermissions(perm);
    
    setLogDebug(true);
    setLogExpire(4 /*hours*/);
    setLogDir(dir);
    _temporaryFolderLogDir = true;
}

void Logger::disableTemporaryFolderLogDir()
{
    if (!_temporaryFolderLogDir)
        return;

    enterNextLogFile("nextcloud.log", LogType::Log);
    setLogDir(QString());
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
    qDebug() << "crash log written in" << logFile.fileName();
}

void Logger::enterNextLogFileNoLock(const QString &baseFileName, LogType type)
{
    if (!_logDirectory.isEmpty()) {

        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Tentative new log name, will be adjusted if one like this already exists
        const auto now = QDateTime::currentDateTime();
        const auto cLocale = QLocale::c(); // Some system locales generate strings that are incompatible with filesystem
        QString newLogName = cLocale.toString(now, QStringLiteral("yyyyMMdd_HHmm")) + QStringLiteral("_%1").arg(baseFileName);

        // Expire old log files and deal with conflicts
        const auto files = dir.entryList({QStringLiteral("*owncloud.log.*"), QStringLiteral("*%1.*").arg(baseFileName)}, QDir::Files, QDir::Name);
        for (const auto &s : files) {
            if (_logExpire > 0) {
                QFileInfo fileInfo(dir.absoluteFilePath(s));
                if (fileInfo.lastModified().addSecs(60 * 60 * _logExpire) < now) {
                    dir.remove(s);
                }
            }
        }

        const auto regexpText = QString{"%1\\.(\\d+).*"}.arg(QRegularExpression::escape(newLogName));
        const auto anchoredPatternRegexpText = QRegularExpression::anchoredPattern(regexpText);
        const QRegularExpression rx(regexpText);
        int maxNumber = -1;
        const auto collidingFileNames = dir.entryList({QStringLiteral("%1.*").arg(newLogName)}, QDir::Files, QDir::Name);
        for(const auto &fileName : collidingFileNames) {
            const auto rxMatch = rx.match(fileName);
            if (rxMatch.hasMatch()) {
                maxNumber = qMax(maxNumber, rxMatch.captured(1).toInt());
            }
        }
        newLogName.append("." + QString::number(maxNumber + 1));

        auto previousLog = QString{};
        switch (type)
        {
        case OCC::Logger::LogType::Log:
            previousLog = _logFile.fileName();
            setLogFileNoLock(dir.filePath(newLogName));
            break;
        case OCC::Logger::LogType::DeleteLog:
            previousLog = _permanentDeleteLogFile.fileName();
            setPermanentDeleteLogFileNoLock(dir.filePath(newLogName));
            break;
        }

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

void Logger::setPermanentDeleteLogFileNoLock(const QString &name)
{
    if (_permanentDeleteLogStream) {
        _permanentDeleteLogStream.reset(nullptr);
        _permanentDeleteLogFile.close();
    }

    if (name.isEmpty()) {
        return;
    }

    bool openSucceeded = false;
    if (name == QLatin1String("-")) {
        openSucceeded = _permanentDeleteLogFile.open(stdout, QIODevice::WriteOnly);
    } else {
        _permanentDeleteLogFile.setFileName(name);
        openSucceeded = _permanentDeleteLogFile.open(QIODevice::WriteOnly);
    }

    if (!openSucceeded) {
        postGuiMessage(tr("Error"),
                       QString(tr("<nobr>File \"%1\"<br/>cannot be opened for writing.<br/><br/>"
                                  "The log output <b>cannot</b> be saved!</nobr>"))
                           .arg(name));
        return;
    }

    _permanentDeleteLogStream.reset(new QTextStream(&_permanentDeleteLogFile));
}

void Logger::enterNextLogFile(const QString &baseFileName, LogType type)
{
    QMutexLocker locker(&_mutex);
    enterNextLogFileNoLock(baseFileName, type);
}

} // namespace OCC
