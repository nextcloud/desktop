/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QRecursiveMutex>

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
    enum class LogType {
        Log,
        DeleteLog,
    };
    Q_ENUM(LogType)

    bool isLoggingToFile() const;

    void doLog(QtMsgType type, const QMessageLogContext &ctx, const QString &message);

    static Logger *instance();

    void postGuiLog(const QString &title, const QString &message);
    void postGuiMessage(const QString &title, const QString &message);

    QString logFile() const;
    void setLogFile(const QString &name);

    void setPermanentDeleteLogFile(const QString &name);

    void setLogExpire(int expire);

    QString logDir() const;
    void setLogDir(const QString &dir);

    void setLogFlush(bool flush);

    bool logDebug() const { return _logDebug; }
    void setLogDebug(bool debug);

    /** Returns where the automatic logdir would be */
    QString temporaryFolderLogDirPath() const;

    /** Sets up default dir log setup.
     *
     * logdir: a temporary folder
     * logexpire: 4 hours
     * logdebug: true
     *
     * Used in conjunction with ConfigFile::automaticLogDir
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

signals:
    void logWindowLog(const QString &);

    void guiLog(const QString &, const QString &);
    void guiMessage(const QString &, const QString &);

public slots:
    void enterNextLogFile(const QString &baseFileName, OCC::Logger::LogType type);

private:
    Logger(QObject *parent = nullptr);
    ~Logger() override;

    void closeNoLock();
    void dumpCrashLog();
    void enterNextLogFileNoLock(const QString &baseFileName, LogType type);
    void setLogFileNoLock(const QString &name);
    void setPermanentDeleteLogFileNoLock(const QString &name);

    QFile _logFile;
    bool _doFileFlush = false;
    int _linesCounter = 0;
    int _logExpire = 0;
    bool _logDebug = false;
    QScopedPointer<QTextStream> _logstream;
    mutable QRecursiveMutex _mutex;
    QString _logDirectory;
    bool _temporaryFolderLogDir = false;
    QSet<QString> _logRules;
    QVector<QString> _crashLog;
    int _crashLogIndex = 0;
    QFile _permanentDeleteLogFile;
    QScopedPointer<QTextStream> _permanentDeleteLogStream;
    bool _didShowLogWriteError = false;
};

} // namespace OCC

#endif // LOGGER_H
