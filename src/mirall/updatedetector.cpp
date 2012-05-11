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
#include <QtCore>
#include <QtNetwork>
#include <QtGui>

#include "updatedetector.h"

#include "mirall/theme.h"
#include "mirall/version.h"
#include "mirall/occinfo.h"

namespace Mirall {


UpdateDetector::UpdateDetector(QObject *parent) :
    QObject(parent)
  , _accessManager( new QNetworkAccessManager(this))
{
}

void UpdateDetector::versionCheck( Theme *theme )
{
    _accessManager = new QNetworkAccessManager(this);
    connect(_accessManager, SIGNAL(finished(QNetworkReply*)), this,
            SLOT(slotVersionInfoArrived(QNetworkReply*)) );
    QUrl url("http://download.owncloud.com/clientupdater.php");
    QString ver = QString("%1.%2.%3").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR).arg(MIRALL_VERSION_MICRO);

    QString platform = QLatin1String("stranger");
#ifdef Q_OS_LINUX
    platform = QLatin1String("linux");
#endif
#ifdef Q_OS_WIN32
    platform = QLatin1String( "win32" );
#endif
#ifdef Q_OS_MACOS
    platform = QLatin1String( "macos" );
#endif

    url.addQueryItem( "version", ver );
    url.addQueryItem( "platform", platform );

    qDebug() << "00 client update check to " << url.toString();

    _accessManager->get( QNetworkRequest( url ));
}

void UpdateDetector::slotVersionInfoArrived( QNetworkReply* reply )
{
    if( reply->error() != QNetworkReply::NoError ) {
        qDebug() << "Failed to reach version check url: " << reply->errorString();
        return;
    }

    QString xml = QString::fromAscii( reply->readAll() );

    bool ok;
    Owncloudclient ocClient = Owncloudclient::parseString( xml, &ok );
    if( ok ) {

    //        Thats how it looks like if a new version is available:
    //        <?xml version="1.0"?>
    //            <owncloudclient>
    //              <version>1.0.0</version>
    //              <versionstring>ownCloud Client 1.0.0</versionstring>
    //              <web>http://ownCloud.org/client/update</web>
    //            </owncloudclient>
    //
    //        and thats if no new version available:
    //            <?xml version="1.0"?>
    //                <owncloudclient>
    //                  <version></version>
    //                  <versionstring></versionstring>
    //                  <web></web>
    //                </owncloudclient>
        if( ocClient.version().isEmpty() ) {
            qDebug() << "Client is on latest version!";
        } else {
            // if the version tag is set, there is a newer version.
            QString ver = QString("%1.%2.%3").arg(MIRALL_VERSION_MAJOR).arg(MIRALL_VERSION_MINOR).arg(MIRALL_VERSION_MICRO);
            QMessageBox msgBox;
            msgBox.setTextFormat( Qt::RichText );
            msgBox.setWindowTitle(tr("Client Version Check"));
            msgBox.setIcon( QMessageBox::Information );
            msgBox.setText(tr("<p>A new version of the ownCloud Client is available."));
            QString txt = tr("%1 is available. The installed version is %3.<p/><p>For more information see <a href=\"%2\">%2</a></p>")
                    .arg(ocClient.versionstring()).arg(ocClient.web()).arg(ver);

            msgBox.setInformativeText( txt );
            msgBox.setStandardButtons( QMessageBox::Ok );
            msgBox.setDefaultButton( QMessageBox::Ok );
            msgBox.exec();
        }
    } else {
        qDebug() << "Could not parse update information.";
    }
}

}
