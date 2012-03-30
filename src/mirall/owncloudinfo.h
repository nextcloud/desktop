/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#ifndef OWNCLOUDINFO_H
#define OWNCLOUDINFO_H

#include <QObject>
#include <QNetworkReply>

class QNetworkAccessManager;

namespace Mirall
{

class ownCloudInfo : public QObject
{
    Q_OBJECT
public:
    explicit ownCloudInfo( const QString& = QString(), QObject *parent = 0);
    ~ownCloudInfo();

    bool isConfigured();

    /**
      * call status.php
      */
    void checkInstallation();

    /**
      * a general GET request to the ownCloud. If the second bool parameter is
      * true, the WebDAV server is queried.
      */
    void getRequest( const QString&, bool );

    /**
      * convenience: GET request to the WebDAV server.
      */
    void getWebDAVPath( const QString& );

signals:
    // result signal with url- and version string.
    void ownCloudInfoFound( const QString&,  const QString& );
    void noOwncloudFound( QNetworkReply::NetworkError );
    void ownCloudDirExists( const QString&, QNetworkReply* );

public slots:

protected slots:
    void slotReplyFinished( QNetworkReply* );
    void slotReadyRead();
    void slotError( QNetworkReply::NetworkError );
    void slotAuthentication( QNetworkReply*, QAuthenticator *);

private:
    QNetworkAccessManager *_manager;
    QNetworkReply *_reply;
    QByteArray    _readBuffer;
    QString       _connection;
    bool          _versionInfoCall;
    QString       _directory;
};

};

#endif // OWNCLOUDINFO_H
