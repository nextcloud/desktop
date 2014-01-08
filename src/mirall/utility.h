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

#include <QString>
#include <QByteArray>

class QWidget;

namespace Mirall {

namespace Utility
{
    void sleep(int sec);
    void usleep(int usec);
    QString formatFingerprint( const QByteArray& );
    void setupFavLink( const QString &folder );
    bool writeRandomFile( const QString& fname, int size = -1);
    QString octetsToString( qint64 octets );
    QString platform();
    QByteArray userAgentString();
    void raiseDialog(QWidget *);
    bool hasLaunchOnStartup(const QString &appName);
    void setLaunchOnStartup(const QString &appName, const QString& guiName, bool launch);
    qint64 freeDiskSpace(const QString &path, bool *ok = 0);
    QString toCSyncScheme(const QString &urlStr);
    void showInFileManager(const QString &localPath);
    /** Like QLocale::toString(double, 'f', prec), but drops trailing zeros after the decimal point */

    /**
     * @brief compactFormatDouble - formats a double value human readable.
     *
     * @param value the value to format.
     * @param prec the precision.
     * @param unit an optional unit that is appended if present.
     * @return the formatted string.
     */
    QString compactFormatDouble(double value, int prec, const QString& unit = QString::null);

    // porting methods
    QString escape(const QString&);
    QString dataLocation();

    // convinience OS detection methods
    bool isWindows();
    bool isMac();
    bool isUnix();
}

}
#endif // UTILITY_H
