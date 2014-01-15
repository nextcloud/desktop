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

#include "mirall/genericupdater.h"
#include "mirall/theme.h"
#include "mirall/version.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/utility.h"
#include "mirall/mirallaccessmanager.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include <stdio.h>

namespace Mirall {

static const char updateAvailableC[] = "Updater/updateAvailable";
static const char lastVersionC[] = "Updater/lastVersion";
static const char ranUpdateC[] = "Updater/ranUpdate";

GenericUpdater::GenericUpdater(const QUrl &url, QObject *parent) :
    QObject(parent)
  , _updateUrl(url)
  , _accessManager(new MirallAccessManager(this))
  , _state(Unknown)
  , _showFallbackMessage(false)
{
}

Updater::UpdateState GenericUpdater::updateState() const
{
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    bool exists = QFile(updateFile).exists();
    if (!updateFile.isEmpty() && exists) {
        // we have an update, did we succeed running it?
        bool ranUpdate = settings.value(ranUpdateC, false).toBool();
        if (ranUpdate) {
            if (updateSucceeded()) {
                settings.remove(ranUpdateC);
                return NoUpdate;
            } else {
                return UpdateFailed;
            }
        } else {
            return UpdateAvailable;
        }
    } else {
        return NoUpdate;
    }
}

void GenericUpdater::performUpdate()
{
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    if (!updateFile.isEmpty() && QFile(updateFile).exists()) {
        if (QMessageBox::information(0, tr("New Update Ready"),
                                     tr("A new update is about to be installed. The updater may ask\n"
                                        "for additional privileges during the process."), QMessageBox::Ok)) {
            slotStartInstaller();
        }
    }
}

void GenericUpdater::backgroundCheckForUpdate()
{
    // FIXME
    checkForUpdate();
}

void GenericUpdater::showFallbackMessage()
{
    _showFallbackMessage = true;
}

QString GenericUpdater::statusString() const
{
    QString updateVersion = _updateInfo.version();

    switch (state()) {
    case DownloadingUpdate:
        return tr("Downloading version %1. Please wait...").arg(updateVersion);
    case DownloadedUpdate:
        return tr("Version %1 available. Restart application to start the update.").arg(updateVersion);
    case DownloadFailed:
        return tr("Could not download update. Please click <a href='%1'>here</a> %2 to download the update manually").arg(_updateInfo.web(), updateVersion);
    case UpdateButNoDownloadAvailable:
        return tr("New version %1 available. Please use the systems update tool to install it.").arg(updateVersion);
    case Unknown:
        return tr("Checking update server...");
    case UpToDate:
        // fall through
    default:
        return tr("Your installation is at the latest version");
    }
}

int GenericUpdater::state() const
{
    return _state;
}

void GenericUpdater::setState(int state)
{
    _state = state;
    emit stateChanged();
}

void GenericUpdater::slotStartInstaller()
{
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    settings.setValue(ranUpdateC, true);
    qDebug() << "Running updater" << updateFile;
    QProcess::startDetached(updateFile, QStringList() << "/S");
    qApp->quit();
}

void GenericUpdater::checkForUpdate()
{
    Theme *theme = Theme::instance();
    QUrl url(_updateUrl);
    QString platform = QLatin1String("stranger");
    if (Utility::isLinux()) {
        platform = QLatin1String("linux");
    } else if (Utility::isWindows()) {
        platform = QLatin1String("win32");
    } else if (Utility::isMac()) {
        platform = QLatin1String("macos");
    }
    qDebug() << "00 client update check to " << url.toString();

    QString sysInfo = getSystemInfo();
    if( !sysInfo.isEmpty() ) {
        url.addQueryItem(QLatin1String("client"), sysInfo );
    }
    url.addQueryItem( QLatin1String("version"), clientVersion() );
    url.addQueryItem( QLatin1String("platform"), platform );
    url.addQueryItem( QLatin1String("oem"), theme->appName() );

    QNetworkReply *reply = _accessManager->get( QNetworkRequest(url) );
    connect(reply, SIGNAL(finished()), this,
            SLOT(slotVersionInfoArrived()) );

}

void GenericUpdater::slotOpenUpdateUrl()
{
    QDesktopServices::openUrl(_updateInfo.web());
}

void GenericUpdater::slotSetVersionSeen()
{
    MirallConfigFile cfg;
    cfg.setSeenVersion(_updateInfo.version());
}

QString GenericUpdater::getSystemInfo()
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

void GenericUpdater::showDialog()
{
    // if the version tag is set, there is a newer version.
    QDialog *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

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
            .arg(Theme::instance()->appNameGUI()).arg(_updateInfo.versionString()).arg(clientVersion());

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    QDialogButtonBox *bb = new QDialogButtonBox;
    bb->setWindowFlags(bb->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QPushButton *skip = bb->addButton(tr("Skip this version"), QDialogButtonBox::ResetRole);
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton  *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(skip, SIGNAL(clicked()), msgBox, SLOT(reject()));
    connect(reject, SIGNAL(clicked()), msgBox, SLOT(reject()));
    connect(getupdate, SIGNAL(clicked()), msgBox, SLOT(accept()));

    connect(skip, SIGNAL(clicked()), SLOT(slotSetVersionSeen()));
    connect(getupdate, SIGNAL(clicked()), SLOT(slotOpenUpdateUrl()));

    layout->addWidget(bb);

    msgBox->open();
}

void GenericUpdater::slotVersionInfoArrived()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if( reply->error() != QNetworkReply::NoError ) {
        qDebug() << "Failed to reach version check url: " << reply->errorString();
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok;
    _updateInfo = UpdateInfo::parseString( xml, &ok );
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
        if( _updateInfo.version().isEmpty() || _updateInfo.version() == cfg.seenVersion() ) {
            qDebug() << "Client is on latest version!";
            setState(UpToDate);
        } else {
            QString url = _updateInfo.downloadUrl();
            if (url.isEmpty() || _showFallbackMessage) {
                if (Utility::isWindows()) {
                    showDialog();
                }
                setState(UpdateButNoDownloadAvailable);
            } else {
                _targetFile = cfg.configPath() + url.mid(url.lastIndexOf('/'));
                if (QFile(_targetFile).exists()) {
                    setState(DownloadedUpdate);
                } else {
                    QNetworkReply *reply = _accessManager->get(QNetworkRequest(QUrl(url)));
                    connect(reply, SIGNAL(readyRead()), SLOT(slotWriteFile()));
                    connect(reply, SIGNAL(finished()), SLOT(slotDownloadFinished()));
                    setState(DownloadingUpdate);
                    _file.reset(new QTemporaryFile);
                    _file->setAutoRemove(true);
                    _file->open();
                }
            }
        }
    } else {
        qDebug() << "Could not parse update information.";
    }
}

void GenericUpdater::slotWriteFile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if(_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void GenericUpdater::slotDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        setState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();
    QFile::copy(_file->fileName(), _targetFile);
    setState(DownloadedUpdate);
    qDebug() << "Downloaded" << url.toString() << "to" << _targetFile;
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    settings.setValue(lastVersionC, clientVersion());
    settings.setValue(updateAvailableC, _targetFile);

}

static qint64 versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build)
{
    return major << 56 | minor << 48 | patch << 40 | build;
}

bool GenericUpdater::updateSucceeded() const
{
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QByteArray lastVersion = settings.value(lastVersionC).toString().toLatin1();
    int major = 0, minor = 0, patch = 0, build = 0;
    sscanf(lastVersion, "%d.%d.%d.%d", &major, &minor, &patch, &build);
    qint64 oldVersionInt = versionToInt(major, minor, patch, build);
    qint64 versionInt = versionToInt(MIRALL_VERSION_MAJOR, MIRALL_VERSION_MINOR,
                                     MIRALL_VERSION_PATCH, MIRALL_VERSION_BUILD);
    return versionInt > oldVersionInt;
}

QString GenericUpdater::clientVersion() const
{
    return QString::fromLatin1(MIRALL_STRINGIFY(MIRALL_VERSION_FULL));
}

}
