/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef UTILITY_H
#define UTILITY_H

#include "owncloudlib.h"
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>

namespace OCC {

/** \addtogroup libsync
 *  @{
 */
namespace Utility
{
    OWNCLOUDSYNC_EXPORT void sleep(int sec);
    OWNCLOUDSYNC_EXPORT void usleep(int usec);
    OWNCLOUDSYNC_EXPORT QString formatFingerprint( const QByteArray&, bool colonSeparated = true );
    OWNCLOUDSYNC_EXPORT void setupFavLink( const QString &folder );
    OWNCLOUDSYNC_EXPORT bool writeRandomFile( const QString& fname, int size = -1);
    OWNCLOUDSYNC_EXPORT QString octetsToString( qint64 octets );
    OWNCLOUDSYNC_EXPORT QByteArray userAgentString();
    OWNCLOUDSYNC_EXPORT bool hasLaunchOnStartup(const QString &appName);
    OWNCLOUDSYNC_EXPORT void setLaunchOnStartup(const QString &appName, const QString& guiName, bool launch);
    OWNCLOUDSYNC_EXPORT qint64 freeDiskSpace(const QString &path);
    OWNCLOUDSYNC_EXPORT QString toCSyncScheme(const QString &urlStr);

    /**
     * @brief compactFormatDouble - formats a double value human readable.
     *
     * @param value the value to format.
     * @param prec the precision.
     * @param unit an optional unit that is appended if present.
     * @return the formatted string.
     */
    OWNCLOUDSYNC_EXPORT QString compactFormatDouble(double value, int prec, const QString& unit = QString::null);

    // porting methods
    OWNCLOUDSYNC_EXPORT QString escape(const QString&);

    // conversion function QDateTime <-> time_t   (because the ones builtin work on only unsigned 32bit)
    OWNCLOUDSYNC_EXPORT QDateTime qDateTimeFromTime_t(qint64 t);
    OWNCLOUDSYNC_EXPORT qint64 qDateTimeToTime_t(const QDateTime &t);

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
    OWNCLOUDSYNC_EXPORT QString durationToDescriptiveString1(quint64 msecs);
    OWNCLOUDSYNC_EXPORT QString durationToDescriptiveString2(quint64 msecs);

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
    OWNCLOUDSYNC_EXPORT bool hasDarkSystray();

    // convenience OS detection methods
    OWNCLOUDSYNC_EXPORT bool isWindows();
    OWNCLOUDSYNC_EXPORT bool isMac();
    OWNCLOUDSYNC_EXPORT bool isUnix();
    OWNCLOUDSYNC_EXPORT bool isLinux(); // use with care
    OWNCLOUDSYNC_EXPORT bool isBSD(); // use with care, does not match OS X

    OWNCLOUDSYNC_EXPORT QString platformName();
    // crash helper for --debug
    OWNCLOUDSYNC_EXPORT void crash();

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OWNCLOUDSYNC_EXPORT bool fsCasePreserving();

    // Call the given command with the switch --version and rerun the first line
    // of the output.
    // If command is empty, the function calls the running application which, on
    // Linux, might have changed while this one is running.
    // For Mac and Windows, it returns QString()
    OWNCLOUDSYNC_EXPORT QByteArray versionOfInstalledBinary(const QString& command = QString() );

    OWNCLOUDSYNC_EXPORT QString fileNameForGuiUse(const QString& fName);

    /**
     * @brief timeAgoInWords - human readable time span
     *
     * Use this to get a string that describes the timespan between the first and
     * the second timestamp in a human readable and understandable form.
     *
     * If the second parameter is ommitted, the current time is used.
     */
    OWNCLOUDSYNC_EXPORT QString timeAgoInWords(const QDateTime& dt, const QDateTime& from = QDateTime() );

    class OWNCLOUDSYNC_EXPORT StopWatch {
    private:
        QMap<QString, quint64> _lapTimes;
        QDateTime _startTime;
        QElapsedTimer _timer;
    public:
        void start();
        quint64 stop();
        quint64 addLapTime( const QString& lapName );
        void reset();

        // out helpers, return the measured times.
        QDateTime startTime() const;
        QDateTime timeOfLap( const QString& lapName ) const;
        quint64 durationOfLap( const QString& lapName ) const;
    };

    /**
     * @brief Sort a QStringList in a way that's appropriate for filenames
     */
    OWNCLOUDSYNC_EXPORT void sortFilenames(QStringList& fileNames);

}
/** @} */ // \addtogroup

}
#endif // UTILITY_H
