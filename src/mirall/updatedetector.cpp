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

#include "mirall/updatedetector.h"
#include "mirall/theme.h"
#include "mirall/version.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/occinfo.h"
#include "mirall/utility.h"
#include "mirall/mirallaccessmanager.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif


namespace Mirall {


UpdateDetector::UpdateDetector(QObject *parent) :
    QObject(parent)
  , _accessManager(new MirallAccessManager(this))
{
}

void UpdateDetector::versionCheck( Theme *theme )
{
    connect(_accessManager, SIGNAL(finished(QNetworkReply*)), this,
            SLOT(slotVersionInfoArrived(QNetworkReply*)) );
    QUrl url(Theme::instance()->updateCheckUrl());

    QString platform = QLatin1String("stranger");
#ifdef Q_OS_LINUX
    platform = QLatin1String("linux");
#endif
#ifdef Q_OS_WIN
    platform = QLatin1String( "win32" );
#endif
#ifdef Q_OS_MAC
    platform = QLatin1String( "macos" );
#endif
    qDebug() << "00 client update check to " << url.toString();

    QString sysInfo = getSystemInfo();
    if( !sysInfo.isEmpty() ) {
        url.addQueryItem(QLatin1String("client"), sysInfo );
    }
    url.addQueryItem( QLatin1String("version"),
                      QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION_FULL)) );
    url.addQueryItem( QLatin1String("platform"), platform );
    url.addQueryItem( QLatin1String("oem"), theme->appName() );

    QNetworkRequest req( url );
    req.setRawHeader( QByteArray("User-Agent"), Utility::userAgentString() );

    _accessManager->get( req );
}

void UpdateDetector::slotOpenUpdateUrl()
{
    QDesktopServices::openUrl(ocClient.web());
}

void UpdateDetector::slotSetVersionSeen()
{
    MirallConfigFile cfg;
    cfg.setSeenVersion(ocClient.version());
}

QString UpdateDetector::getSystemInfo()
{
#ifdef Q_OS_LINUX
    QProcess process;
    process.start( QLatin1String("lsb_release -a") );
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    qDebug() << "Sys Info size: " << output.length();
    if( output.length() > 1024 ) output.clear(); // don't send too much.

    return QString::fromLocal8Bit( output.toBase64() );
#else
    return QString::null;
#endif
}

void UpdateDetector::showDialog()
{
    QDialog *msgBox = new QDialog;

    QIcon info = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation, 0, 0);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, 0);

    msgBox->setWindowIcon(info);

    QVBoxLayout *layout = new QVBoxLayout(msgBox);
    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("New Version Available"));

    QLabel *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(info.pixmap(iconSize));
    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available.</p>"
                     "<p><b>%2</b> is available for download. The installed version is %3.<p>")
            .arg(Theme::instance()->appNameGUI()).arg(ocClient.versionstring())
            .arg(QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION_FULL)));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    QDialogButtonBox *bb = new QDialogButtonBox;
    bb->setWindowFlags(bb->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QPushButton *skip = bb->addButton(tr("Skip update"), QDialogButtonBox::ResetRole);
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton  *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(skip, SIGNAL(clicked()), msgBox, SLOT(reject()));
    connect(reject, SIGNAL(clicked()), msgBox, SLOT(reject()));
    connect(getupdate, SIGNAL(clicked()), msgBox, SLOT(accept()));

    connect(skip, SIGNAL(clicked()), SLOT(slotSetVersionSeen()));
    connect(getupdate, SIGNAL(clicked()), SLOT(slotOpenUpdateUrl()));

    layout->addWidget(bb);

    msgBox->open();
    msgBox->resize(400, msgBox->sizeHint().height());
}

void UpdateDetector::slotVersionInfoArrived( QNetworkReply* reply )
{
    if( reply->error() != QNetworkReply::NoError ) {
        qDebug() << "Failed to reach version check url: " << reply->errorString();
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok;
    ocClient = Owncloudclient::parseString( xml, &ok );
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
        MirallConfigFile cfg;
        if( ocClient.version().isEmpty() || ocClient.version() == cfg.seenVersion() ) {
            qDebug() << "Client is on latest version!";
        } else {
            showDialog();
        }
    } else {
        qDebug() << "Could not parse update information.";
    }
}

}
