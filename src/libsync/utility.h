/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef UTILITY_H
#define UTILITY_H

#include "owncloudlib.h"
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>

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
    OWNCLOUDSYNC_EXPORT qint64 freeDiskSpace(const QString &path, bool *ok = 0);
    OWNCLOUDSYNC_EXPORT QString toCSyncScheme(const QString &urlStr);
    /** Like QLocale::toString(double, 'f', prec), but drops trailing zeros after the decimal point */

    OWNCLOUDSYNC_EXPORT bool doesSetContainPrefix(const QSet<QString> &l, const QString &p);



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
     */
    OWNCLOUDSYNC_EXPORT QString durationToDescriptiveString(quint64 msecs);

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

    // convinience OS detection methods
    OWNCLOUDSYNC_EXPORT bool isWindows();
    OWNCLOUDSYNC_EXPORT bool isMac();
    OWNCLOUDSYNC_EXPORT bool isUnix();
    OWNCLOUDSYNC_EXPORT bool isLinux(); // use with care
    OWNCLOUDSYNC_EXPORT bool isBSD(); // use with care, does not match OS X

    // crash helper for --debug
    OWNCLOUDSYNC_EXPORT void crash();

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OWNCLOUDSYNC_EXPORT bool fsCasePreserving();

    class OWNCLOUDSYNC_EXPORT StopWatch {
    private:
        QHash<QString, quint64> _lapTimes;
        QDateTime _startTime;
        QElapsedTimer _timer;
    public:
        void start();
        quint64 stop();
        quint64 addLapTime( const QString& lapName );
        void reset();

        // out helpers, return the masured times.
        QDateTime startTime() const;
        QDateTime timeOfLap( const QString& lapName ) const;
        quint64 durationOfLap( const QString& lapName ) const;
    };

}
/** @} */ // \addtogroup

}
#endif // UTILITY_H
