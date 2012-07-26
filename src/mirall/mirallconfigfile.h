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

#ifndef MIRALLCONFIGFILE_H
#define MIRALLCONFIGFILE_H

#include <QString>

class QVariant;

namespace Mirall {


class MirallConfigFile
{
public:
    MirallConfigFile( const QString& appendix = QString() );

    enum customMediaType {
        oCSetupTop,      // ownCloud connect page
        oCSetupSide,
        oCSetupBottom,
        oCSetupFixUrl,
        oCSetupResultTop // ownCloud connect result page
    };

    QString configPath() const;
    QString configFile() const;
    QString excludeFile() const;

    bool exists();

    bool connectionExists( const QString& = QString() );
    QString defaultConnection() const;

    void writeOwncloudConfig( const QString& connection,
                              const QString& url,
                              const QString& user,
                              const QString& passwd, bool skipPwd );

    void removeConnection( const QString& connection = QString() );

    QString ownCloudUrl( const QString& connection = QString(), bool webdav = false ) const;

    QString ownCloudUser( const QString& connection = QString() ) const;

    QString ownCloudPasswd( const QString& connection = QString() ) const;

    QString ownCloudVersion() const;
    void setOwnCloudVersion( const QString& );

    QVariant customMedia( customMediaType );

    // max count of lines in the log window
    int  maxLogLines() const;

    bool ownCloudSkipUpdateCheck( const QString& connection = QString() ) const;

    /* Poll intervals in milliseconds */
    int localPollInterval ( const QString& connection = QString() ) const;
    int remotePollInterval( const QString& connection = QString() ) const;
    int pollTimerExceedFactor( const QString& connection = QString() ) const;

    QByteArray basicAuthHeader() const;

    // Custom Config: accept the custom config to become the main one.
    void acceptCustomConfig();
    // Custom Config: remove the custom config file.
    void cleanupCustomConfig();

private:
    static QString _passwd;
    static bool    _askedUser;
    static QString _oCVersion;
    QString        _customHandle;
};

}
#endif // MIRALLCONFIGFILE_H
