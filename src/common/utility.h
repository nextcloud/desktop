/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef UTILITY_H
#define UTILITY_H

#include "ocsynclib.h"
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMap>
#include <QUrl>
#include <memory>

class QSettings;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcUtility)

/** \addtogroup libsync
 *  @{
 */
namespace Utility {
    OCSYNC_EXPORT void sleep(int sec);
    OCSYNC_EXPORT void usleep(int usec);
    OCSYNC_EXPORT QString formatFingerprint(const QByteArray &, bool colonSeparated = true);
    OCSYNC_EXPORT void setupFavLink(const QString &folder);
    OCSYNC_EXPORT bool writeRandomFile(const QString &fname, int size = -1);
    OCSYNC_EXPORT QString octetsToString(qint64 octets);
    OCSYNC_EXPORT QByteArray userAgentString();
    OCSYNC_EXPORT bool hasLaunchOnStartup(const QString &appName);
    OCSYNC_EXPORT void setLaunchOnStartup(const QString &appName, const QString &guiName, bool launch);

    /**
     * Return the amount of free space available.
     *
     * \a path must point to a directory
     */
    OCSYNC_EXPORT qint64 freeDiskSpace(const QString &path);

    /**
     * @brief compactFormatDouble - formats a double value human readable.
     *
     * @param value the value to format.
     * @param prec the precision.
     * @param unit an optional unit that is appended if present.
     * @return the formatted string.
     */
    OCSYNC_EXPORT QString compactFormatDouble(double value, int prec, const QString &unit = QString::null);

    // porting methods
    OCSYNC_EXPORT QString escape(const QString &);

    // conversion function QDateTime <-> time_t   (because the ones builtin work on only unsigned 32bit)
    OCSYNC_EXPORT QDateTime qDateTimeFromTime_t(qint64 t);
    OCSYNC_EXPORT qint64 qDateTimeToTime_t(const QDateTime &t);

    /**
     * @brief Convert milliseconds duration to human readable string.
     * @param quint64 msecs the milliseconds to convert to string.
     * @return an HMS representation of the milliseconds value.
     *
     * durationToDescriptiveString1 describes the duration in a single
     * unit, like "5 minutes" or "2 days".
     *
     * durationToDescriptiveString2 uses two units where possible, so
     * "5 minutes 43 seconds" or "1 month 3 days".
     */
    OCSYNC_EXPORT QString durationToDescriptiveString1(quint64 msecs);
    OCSYNC_EXPORT QString durationToDescriptiveString2(quint64 msecs);

    /**
     * @brief hasDarkSystray - determines whether the systray is dark or light.
     *
     * Use this to check if the OS has a dark or a light systray.
     *
     * The value might change during the execution of the program
     * (e.g. on OS X 10.10).
     *
     * @return bool which is true for systems with dark systray.
     */
    OCSYNC_EXPORT bool hasDarkSystray();

    // convenience OS detection methods
    inline bool isWindows();
    inline bool isMac();
    inline bool isUnix();
    inline bool isLinux(); // use with care
    inline bool isBSD(); // use with care, does not match OS X

    OCSYNC_EXPORT QString platformName();
    // crash helper for --debug
    OCSYNC_EXPORT void crash();

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OCSYNC_EXPORT bool fsCasePreserving();

    // Check if two pathes that MUST exist are equal. This function
    // uses QDir::canonicalPath() to judge and cares for the systems
    // case sensitivity.
    OCSYNC_EXPORT bool fileNamesEqual(const QString &fn1, const QString &fn2);

    // Call the given command with the switch --version and rerun the first line
    // of the output.
    // If command is empty, the function calls the running application which, on
    // Linux, might have changed while this one is running.
    // For Mac and Windows, it returns QString()
    OCSYNC_EXPORT QByteArray versionOfInstalledBinary(const QString &command = QString());

    OCSYNC_EXPORT QString fileNameForGuiUse(const QString &fName);

    OCSYNC_EXPORT QByteArray normalizeEtag(QByteArray etag);

    /**
     * @brief timeAgoInWords - human readable time span
     *
     * Use this to get a string that describes the timespan between the first and
     * the second timestamp in a human readable and understandable form.
     *
     * If the second parameter is ommitted, the current time is used.
     */
    OCSYNC_EXPORT QString timeAgoInWords(const QDateTime &dt, const QDateTime &from = QDateTime());

    class OCSYNC_EXPORT StopWatch
    {
    private:
        QMap<QString, quint64> _lapTimes;
        QDateTime _startTime;
        QElapsedTimer _timer;

    public:
        void start();
        quint64 stop();
        quint64 addLapTime(const QString &lapName);
        void reset();

        // out helpers, return the measured times.
        QDateTime startTime() const;
        QDateTime timeOfLap(const QString &lapName) const;
        quint64 durationOfLap(const QString &lapName) const;
    };

    /**
     * @brief Sort a QStringList in a way that's appropriate for filenames
     */
    OCSYNC_EXPORT void sortFilenames(QStringList &fileNames);

    /** Appends concatPath and queryItems to the url */
    OCSYNC_EXPORT QUrl concatUrlPath(
        const QUrl &url, const QString &concatPath,
        const QList<QPair<QString, QString>> &queryItems = (QList<QPair<QString, QString>>()));

    /**  Returns a new settings pre-set in a specific group.  The Settings will be created
         with the given parent. If no parent is specified, the caller must destroy the settings */
    OCSYNC_EXPORT std::unique_ptr<QSettings> settingsWithGroup(const QString &group, QObject *parent = 0);

    /** Returns whether a file name indicates a conflict file
     *
     * See FileSystem::makeConflictFileName.
     */
    OCSYNC_EXPORT bool isConflictFile(const char *name);

    /** Returns whether conflict files should be uploaded.
     *
     * Experimental! Real feature planned for 2.5.
     */
    OCSYNC_EXPORT bool shouldUploadConflictFiles();
}
/** @} */ // \addtogroup

inline bool Utility::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

inline bool Utility::isMac()
{
#ifdef Q_OS_MAC
    return true;
#else
    return false;
#endif
}

inline bool Utility::isUnix()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

inline bool Utility::isLinux()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

inline bool Utility::isBSD()
{
#if defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    return true;
#else
    return false;
#endif
}

}
#endif // UTILITY_H
