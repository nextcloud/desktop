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
#include <QtNetwork>

namespace Mirall
{

class ownCloudInfo : public QObject
{
    Q_OBJECT
public:

  static ownCloudInfo *instance();

    bool isConfigured();

    /**
      * call status.php
      */
    QNetworkReply* checkInstallation();

    /**
      * convenience: GET request to the WebDAV server.
      */
    QNetworkReply* getWebDAVPath( const QString& );

    /**
      * There is a global flag here if the user once decided against trusting the
      * SSL connection. This method resets it so that the ssl dialog is shown again.
      */
    void resetSSLUntrust();

    /**
      * Set wether or not to trust errorneus SSL certificates
      */
    void setCertsUntrusted(bool donttrust);

    /**
      * Do we trust the certificate?.
      */
    bool certsUntrusted();

    /**
     * Set a NetworkAccessManager to be used
     *
     * This method will take ownership of the NetworkAccessManager, so you can just
     * set it initially and forget about its memory management.
     */
    void setNetworkAccessManager( QNetworkAccessManager *qnam );

    /**
      * Create a collection via owncloud. Provide a relative path.
      */
    QNetworkReply* mkdirRequest( const QString& );

    /**
      * Retrieve quota for a path. Provide a relative path.
      */
    QNetworkReply* getQuotaRequest( const QString& );

    /**
      * provide collections in a directory via owncloud. Provide a relative path.
      */
    QNetworkReply* getDirectoryListing( const QString& dir );

    /**
     * Use a custom ownCloud configuration file identified by handle
     */
    void setCustomConfigHandle( const QString& );

    /**
     * Accessor to the config handle.
     */
    QString configHandle(QNetworkReply *reply = 0);

    /**
     * Certificate chain of the connection est. with ownCloud.
     * Empty if the connection is HTTP-based
     */
    QList<QSslCertificate> certificateChain() const;

    /**
     * returns the owncloud webdav url.
     * It may be different from the one in the config if there was a HTTP redirection
     */
    QString webdavUrl(const QString& connection = QString());

    qint64 lastQuotaUsedBytes() const { return _lastQuotaUsedBytes; }
    qint64 lastQuotaTotalBytes() const  { return _lastQuotaTotalBytes; }

    QList<QNetworkCookie> getLastAuthCookies();

signals:
    // result signal with url- and version string.
    void ownCloudInfoFound( const QString&, const QString&, const QString&, const QString& );
    void noOwncloudFound( QNetworkReply* );
    void ownCloudDirExists( const QString&, QNetworkReply* );

    void webdavColCreated( QNetworkReply::NetworkError );
    void sslFailed( QNetworkReply *reply, QList<QSslError> errors );
    void guiLog( const QString& title, const QString& content );
    void quotaUpdated( qint64 total, qint64 quotaUsedBytes );
    void directoryListingUpdated(const QStringList &directories);

protected slots:
    void slotReplyFinished( );
    void slotError( QNetworkReply::NetworkError );

    void slotMkdirFinished();
    void slotGetQuotaFinished();
    void slotGetDirectoryListingFinished();

private:
    explicit ownCloudInfo();

    /**
     * a general GET request to the ownCloud WebDAV.
     */
    QNetworkReply* getRequest( const QUrl &url);
    QNetworkReply* davRequest(const QByteArray&, QNetworkRequest&, QIODevice* );

    ~ownCloudInfo();

    void setupHeaders(QNetworkRequest &req, quint64 size );

    static ownCloudInfo           *_instance;

    QNetworkAccessManager         *_manager;
    QString                        _connection;
    QString                        _configHandle;
    QUrl                           _urlRedirectedTo;
    QHash<QNetworkReply*, QString> _directories;
    QHash<QNetworkReply*, QString> _configHandleMap;
    QList<QSslCertificate>         _certificateChain;
    bool                           _certsUntrusted;
    int                            _authAttempts;
    QMutex                         _certChainMutex;
    int                            _redirectCount;
    qint64                         _lastQuotaUsedBytes;
    qint64                         _lastQuotaTotalBytes;
};

} // ns Mirall

#endif // OWNCLOUDINFO_H
