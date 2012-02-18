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

namespace Mirall {


class MirallConfigFile
{
public:
    MirallConfigFile();

    QString mirallConfigFile() const;
    bool exists();

    bool connectionExists( const QString& = QString() );
    QString defaultConnection() const;

    void writeOwncloudConfig( const QString& connection,
                              const QString& url,
                              const QString& user,
                              const QString& passwd );

    void removeConnection( const QString& connection = QString() );

    QString ownCloudUrl( const QString& connection = QString(), bool webdav = false ) const;

    QUrl fullOwnCloudUrl( const QString& connection = QString() ) const;

    QString ownCloudUser( const QString& connection = QString() ) const;

    QString ownCloudPasswd( const QString& connection = QString() ) const;

    QByteArray basicAuthHeader() const;
};

}
#endif // MIRALLCONFIGFILE_H
