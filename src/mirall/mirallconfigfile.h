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
    /* let only CredentialStore read the password from the file. All other classes
     *  should work with CredentialStore to get the credentials.  */
    friend class CredentialStore;
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
                              const QString& passwd,
                              bool https, bool skipPwd );

    void removeConnection( const QString& connection = QString() );

    QString ownCloudUrl( const QString& connection = QString(), bool webdav = false ) const;

    void setOwnCloudUrl(const QString &connection, const QString& );

    // the certs do not depend on a connection.
    QByteArray caCerts();
    void setCaCerts( const QByteArray& );

    bool passwordStorageAllowed(const QString &);

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

    // Custom Config: accept the custom config to become the main one.
    void acceptCustomConfig();
    // Custom Config: remove the custom config file.
    void cleanupCustomConfig();

    // proxy settings
    void setProxyType(int proxyType,
                      const QString& host = QString(),
                      int port = 0,
                      const QString& user = QString(),
                      const QString& pass = QString());

    int proxyType() const;
    QString proxyHostName() const;
    int proxyPort() const;
    QString proxyUser() const;
    QString proxyPassword() const;
    
    static void setConfDir(const QString &value);

protected:
    // these classes can only be access from CredentialStore as a friend class.
    QString ownCloudPasswd( const QString& connection = QString() ) const;
    QString ownCloudUser( const QString& connection = QString() ) const;
    void clearPasswordFromConfig( const QString& connect = QString() );
    bool writePassword( const QString& passwd, const QString& connection = QString() );

private:
    QVariant getValue(const QString& param, const QString& group) const;


private:
    static bool    _askedUser;
    static QString _oCVersion;
    static QString _confDir;
    QString        _customHandle;

};

}
#endif // MIRALLCONFIGFILE_H
