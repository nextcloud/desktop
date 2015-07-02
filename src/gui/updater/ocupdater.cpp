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

#include "theme.h"
#include "configfile.h"
#include "utility.h"
#include "accessmanager.h"

#include "updater/ocupdater.h"

#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include <stdio.h>

namespace OCC {

static const char updateAvailableC[] = "Updater/updateAvailable";
static const char updateTargetVersionC[] = "Updater/updateTargetVersion";
static const char seenVersionC[] = "Updater/seenVersion";
static const char autoUpdateFailedVersionC[] = "Updater/autoUpdateFailedVersion";
static const char autoUpdateAttemptedC[] = "Updater/autoUpdateAttempted";

OCUpdater::OCUpdater(const QUrl &url, QObject *parent) :
    QObject(parent)
  , _updateUrl(url)
  , _state(Unknown)
  , _accessManager(new AccessManager(this))
  , _timer(new QTimer(this))
{
}

bool OCUpdater::performUpdate()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    if (!updateFile.isEmpty() && QFile(updateFile).exists()) {
        const QString name = Theme::instance()->appNameGUI();
        if (QMessageBox::information(0, tr("New %1 Update Ready").arg(name),
                                     tr("A new update for %1 is about to be installed. The updater may ask\n"
                                        "for additional privileges during the process.").arg(name), QMessageBox::Ok)) {
            slotStartInstaller();
            return true;
        }
    }
    return false;
}

void OCUpdater::backgroundCheckForUpdate()
{
    // FIXME
    checkForUpdate();
}

QString OCUpdater::statusString() const
{
    QString updateVersion = _updateInfo.version();

    switch (downloadState()) {
    case Downloading:
        return tr("Downloading version %1. Please wait...").arg(updateVersion);
    case DownloadComplete:
        return tr("Version %1 available. Restart application to start the update.").arg(updateVersion);
    case DownloadFailed:
        return tr("Could not download update. Please click <a href='%1'>here</a> to download the update manually.").arg(_updateInfo.web());
    case DownloadTimedOut:
        return tr("Could not check for new updates.");
    case UpdateOnlyAvailableThroughSystem:
        return tr("New version %1 available. Please use the system's update tool to install it.").arg(updateVersion);
    case CheckingServer:
        return tr("Checking update server...");
    case Unknown:
        return tr("Update status is unknown: Did not check for new updates.");
    case UpToDate:
        // fall through
    default:
        return tr("No updates available. Your installation is at the latest version.");
    }
}

int OCUpdater::downloadState() const
{
    return _state;
}

void OCUpdater::setDownloadState(DownloadState state)
{
    _state = state;
    emit downloadStateChanged();
}

void OCUpdater::slotStartInstaller()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    settings.setValue(autoUpdateAttemptedC, true);
    settings.sync();
    qDebug() << "Running updater" << updateFile;
    QProcess::startDetached(updateFile, QStringList() << "/S" << "/launch");
}

void OCUpdater::checkForUpdate()
{
    QNetworkReply *reply = _accessManager->get(QNetworkRequest(_updateUrl));
    connect(_timer, SIGNAL(timeout()), this, SLOT(slotTimedOut()));
    _timer->start(30*1000);
    connect(reply, SIGNAL(finished()), this, SLOT(slotVersionInfoArrived()));

    setDownloadState(CheckingServer);
}

void OCUpdater::slotOpenUpdateUrl()
{
    QDesktopServices::openUrl(_updateInfo.web());
}

bool OCUpdater::updateSucceeded() const
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);

    qint64 targetVersionInt = Helper::stringVersionToInt(settings.value(updateTargetVersionC).toString());
    qint64 currentVersion = Helper::currentVersionToInt();
    return currentVersion >= targetVersionInt;
}

void OCUpdater::slotVersionInfoArrived()
{
    _timer->stop();
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if( reply->error() != QNetworkReply::NoError ) {
        qDebug() << "Failed to reach version check url: " << reply->errorString();
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok;
    _updateInfo = UpdateInfo::parseString( xml, &ok );
    if( ok ) {
        versionInfoArrived(_updateInfo);
    } else {
        qDebug() << "Could not parse update information.";
    }
}

void OCUpdater::slotTimedOut()
{
    setDownloadState(DownloadTimedOut);
}

////////////////////////////////////////////////////////////////////////

NSISUpdater::NSISUpdater(const QUrl &url, QObject *parent)
    : OCUpdater(url, parent)
    , _showFallbackMessage(false)
{
}

void NSISUpdater::slotWriteFile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if(_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void NSISUpdater::slotDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        setDownloadState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();
    QFile::copy(_file->fileName(), _targetFile);
    setDownloadState(DownloadComplete);
    qDebug() << "Downloaded" << url.toString() << "to" << _targetFile;
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    settings.setValue(updateTargetVersionC, updateInfo().version());
    settings.setValue(updateAvailableC, _targetFile);
}

void NSISUpdater::versionInfoArrived(const UpdateInfo &info)
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    qint64 infoVersion = Helper::stringVersionToInt(info.version());
    qint64 seenVersion = Helper::stringVersionToInt(settings.value(seenVersionC).toString());
    qint64 currVersion = Helper::currentVersionToInt();
    if(info.version().isEmpty()
       || infoVersion <= currVersion
       || infoVersion <= seenVersion)
    {
        qDebug() << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        QString url = info.downloadUrl();
        qint64 autoUpdateFailedVersion =
                Helper::stringVersionToInt(settings.value(autoUpdateFailedVersionC).toString());
        if (url.isEmpty() || _showFallbackMessage || infoVersion == autoUpdateFailedVersion) {
            showDialog(info);
        }
        if (!url.isEmpty()) {
            _targetFile = cfg.configPath() + url.mid(url.lastIndexOf('/'));
            if (QFile(_targetFile).exists()) {
                setDownloadState(DownloadComplete);
            } else {
                QNetworkReply *reply = qnam()->get(QNetworkRequest(QUrl(url)));
                connect(reply, SIGNAL(readyRead()), SLOT(slotWriteFile()));
                connect(reply, SIGNAL(finished()), SLOT(slotDownloadFinished()));
                setDownloadState(Downloading);
                _file.reset(new QTemporaryFile);
                _file->setAutoRemove(true);
                _file->open();
            }
        }
    }
}

void NSISUpdater::showDialog(const UpdateInfo &info)
{
    // if the version tag is set, there is a newer version.
    QDialog *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation, 0, 0);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, 0);

    msgBox->setWindowIcon(infoIcon);

    QVBoxLayout *layout = new QVBoxLayout(msgBox);
    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("New Version Available"));

    QLabel *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available.</p>"
                     "<p><b>%2</b> is available for download. The installed version is %3.</p>")
            .arg(Theme::instance()->appNameGUI()).arg(info.versionString()).arg(clientVersion());

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

    connect(skip, SIGNAL(clicked()), SLOT(slotSetSeenVersion()));
    connect(getupdate, SIGNAL(clicked()), SLOT(slotOpenUpdateUrl()));

    layout->addWidget(bb);

    msgBox->open();
}

NSISUpdater::UpdateState NSISUpdater::updateStateOnStart()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFileName = settings.value(updateAvailableC).toString();
    // has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        // did it try to execute the update?
        if (settings.value(autoUpdateAttemptedC, false).toBool()) {
            // clean up
            settings.remove(autoUpdateAttemptedC);
            settings.remove(updateAvailableC);
            QFile::remove(updateFileName);
            if (updateSucceeded()) {
                // success: clean up even more
                settings.remove(updateTargetVersionC);
                settings.remove(autoUpdateFailedVersionC);
                return NoUpdate;
            } else {
                // auto update failed. Set autoUpdateFailedVersion as a hint
                // for visual fallback notification
                QString targetVersion = settings.value(updateTargetVersionC).toString();
                settings.setValue(autoUpdateFailedVersionC, targetVersion);
                settings.remove(updateTargetVersionC);
                return UpdateFailed;
            }
        } else {
            if (!settings.contains(autoUpdateFailedVersionC)) {
                return UpdateAvailable;
            }
        }
    }
        return NoUpdate;
}

bool NSISUpdater::handleStartup()
{
    switch (updateStateOnStart()) {
    case NSISUpdater::UpdateAvailable:
        return performUpdate();
    case NSISUpdater::UpdateFailed:
        _showFallbackMessage = true;
        return false;
    case NSISUpdater::NoUpdate:
    default:
        return false;
    }
}

void NSISUpdater::slotSetSeenVersion()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    settings.setValue(seenVersionC, updateInfo().version());
}

////////////////////////////////////////////////////////////////////////

PassiveUpdateNotifier::PassiveUpdateNotifier(const QUrl &url, QObject *parent)
    : OCUpdater(url, parent)
{

}

void PassiveUpdateNotifier::versionInfoArrived(const UpdateInfo &info)
{
    if( info.version().isEmpty() ||
            Helper::stringVersionToInt(info.version())
            >= Helper::currentVersionToInt() )
    {
        qDebug() << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        setDownloadState(UpdateOnlyAvailableThroughSystem);
    }
}

} // ns mirall
