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

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <qmutex.h>
#include <chrono>

#include "common/utility.h"
#include "owncloudlib.h"

namespace OCC {

/**
 * @brief The Logger class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Logger : public QObject
{
    Q_OBJECT
public:
    static QString loggerPattern();

    bool isLoggingToFile() const;

    void attacheToConsole();

    void doLog(QtMsgType type, const QMessageLogContext &ctx, const QString &message);

    static Logger *instance();

    void setLogFile(const QString &name);
    void setLogExpire(std::chrono::hours expire);
    void setLogDir(const QString &dir);
    void setLogFlush(bool flush);

    bool logDebug() const { return _logDebug; }
    void setLogDebug(bool debug);

    /** Returns where the automatic logdir would be */
    QString temporaryFolderLogDirPath() const;

    /** Sets up default dir log setup.
     *
     * logdir: a temporary folder
     * logdebug: true
     *
     * Used in conjunction with ConfigFile::automaticLogDir,
     * see LogBrowser::setupLoggingFromConfig.
     */
    void setupTemporaryFolderLogDir();

    /** For switching off via logwindow */
    void disableTemporaryFolderLogDir();

    void addLogRule(const QSet<QString> &rules) {
        setLogRules(_logRules + rules);
    }
    void removeLogRule(const QSet<QString> &rules) {
        setLogRules(_logRules - rules);
    }
    void setLogRules(const QSet<QString> &rules);

private:
    Logger(QObject *parent = nullptr);
    ~Logger() override;

    void rotateLog();

    void open(const QString &name);
    void close();
    void dumpCrashLog();

    QFile _logFile;
    bool _doFileFlush = false;
    std::chrono::hours _logExpire;
    bool _logDebug = false;
    QScopedPointer<QTextStream> _logstream;
    mutable QMutex _mutex;
    QString _logDirectory;
    bool _temporaryFolderLogDir = false;
    QSet<QString> _logRules;
    QVector<QString> _crashLog;
    int _crashLogIndex = 0;
    bool _consoleIsAttached = false;
};

} // namespace OCC

#endif // LOGGER_H
