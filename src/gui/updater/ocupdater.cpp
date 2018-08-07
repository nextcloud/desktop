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
#include "common/utility.h"
#include "accessmanager.h"

#include "updater/ocupdater.h"

#include <QObject>
#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include <QtWidgets>

#include <stdio.h>

namespace OCC {

static const char updateAvailableC[] = "Updater/updateAvailable";
static const char updateTargetVersionC[] = "Updater/updateTargetVersion";
static const char seenVersionC[] = "Updater/seenVersion";
static const char autoUpdateFailedVersionC[] = "Updater/autoUpdateFailedVersion";
static const char autoUpdateAttemptedC[] = "Updater/autoUpdateAttempted";


UpdaterScheduler::UpdaterScheduler(QObject *parent)
    : QObject(parent)
{
    connect(&_updateCheckTimer, &QTimer::timeout,
        this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an OCUpdater
    if (OCUpdater *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
        connect(updater, &OCUpdater::newUpdateAvailable,
            this, &UpdaterScheduler::updaterAnnouncement);
        connect(updater, &OCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    }

    // at startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    ConfigFile cfg;
    auto checkInterval = cfg.updateCheckInterval();
    _updateCheckTimer.start(checkInterval);
}

void UpdaterScheduler::slotTimerFired()
{
    ConfigFile cfg;

    // re-set the check interval if it changed in the config file meanwhile
    auto checkInterval = cfg.updateCheckInterval();
    if (checkInterval != _updateCheckTimer.interval()) {
        _updateCheckTimer.setInterval(checkInterval);
        qCInfo(lcUpdater) << "Setting new update check interval " << checkInterval;
    }

    // consider the skipUpdateCheck flag in the config.
    if (cfg.skipUpdateCheck()) {
        qCInfo(lcUpdater) << "Skipping update check because of config file";
        return;
    }

    Updater *updater = Updater::instance();
    if (updater) {
        updater->backgroundCheckForUpdate();
    }
}


/* ----------------------------------------------------------------- */

OCUpdater::OCUpdater(const QUrl &url)
    : Updater()
    , _updateUrl(url)
    , _state(Unknown)
    , _accessManager(new AccessManager(this))
    , _timeoutWatchdog(new QTimer(this))
{
}

void OCUpdater::setUpdateUrl(const QUrl &url)
{
    _updateUrl = url;
}

bool OCUpdater::performUpdate()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    if (!updateFile.isEmpty() && QFile(updateFile).exists()
        && !updateSucceeded() /* Someone might have run the updater manually between restarts */) {
        const QString name = Theme::instance()->appNameGUI();
        if (QMessageBox::information(0, tr("New %1 Update Ready").arg(name),
                tr("A new update for %1 is about to be installed. The updater may ask\n"
                   "for additional privileges during the process.")
                    .arg(name),
                QMessageBox::Ok)) {
            slotStartInstaller();
            return true;
        }
    }
    return false;
}

void OCUpdater::backgroundCheckForUpdate()
{
    int dlState = downloadState();

    // do the real update check depending on the internal state of updater.
    switch (dlState) {
    case Unknown:
    case UpToDate:
    case DownloadFailed:
    case DownloadTimedOut:
        qCInfo(lcUpdater) << "Checking for available update";
        checkForUpdate();
        break;
    case DownloadComplete:
        qCInfo(lcUpdater) << "Update is downloaded, skip new check.";
        break;
    case UpdateOnlyAvailableThroughSystem:
        qCInfo(lcUpdater) << "Update is only available through system, skip check.";
        break;
    }
}

QString OCUpdater::statusString() const
{
    QString updateVersion = _updateInfo.version();

    switch (downloadState()) {
    case Downloading:
        return tr("Downloading version %1. Please wait...").arg(updateVersion);
    case DownloadComplete:
        return tr("%1 version %2 available. Restart application to start the update.").arg(Theme::instance()->appNameGUI(), updateVersion);
    case DownloadFailed:
        return tr("Could not download update. Please click <a href='%1'>here</a> to download the update manually.").arg(_updateInfo.web());
    case DownloadTimedOut:
        return tr("Could not check for new updates.");
    case UpdateOnlyAvailableThroughSystem:
        return tr("New %1 version %2 available. Please use the system's update tool to install it.").arg(Theme::instance()->appNameGUI(), updateVersion);
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
    auto oldState = _state;
    _state = state;
    emit downloadStateChanged();

    // show the notification if the download is complete (on every check)
    // or once for system based updates.
    if (_state == OCUpdater::DownloadComplete || (oldState != OCUpdater::UpdateOnlyAvailableThroughSystem
                                                     && _state == OCUpdater::UpdateOnlyAvailableThroughSystem)) {
        emit newUpdateAvailable(tr("Update Check"), statusString());
    }
}

#if defined(Q_OS_WIN)
// Following functions are taken from https://github.com/qt/qtbase/blob/5.8/src/corelib/io/qprocess_win.cpp
// to make use of this fix https://github.com/qt/qtbase/commit/bec2fc19fd18768b16925597871c77a61e716abd
// for QTBUG-53833: Without this we get an ugly powershell window on update. In 2.5/master we use Qt 5.10
// which obviously already has the fix.
static QString qt_create_commandline(const QString &program, const QStringList &arguments)
{
    QString args;
    if (!program.isEmpty()) {
        QString programName = program;
        if (!programName.startsWith(QLatin1Char('\"')) && !programName.endsWith(QLatin1Char('\"')) && programName.contains(QLatin1Char(' ')))
            programName = QLatin1Char('\"') + programName + QLatin1Char('\"');
        programName.replace(QLatin1Char('/'), QLatin1Char('\\'));

        // add the prgram as the first arg ... it works better
        args = programName + QLatin1Char(' ');
    }

    for (int i=0; i<arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // Quotes are escaped and their preceding backslashes are doubled.
        tmp.replace(QRegExp(QLatin1String("(\\\\*)\"")), QLatin1String("\\1\\1\\\""));
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            int i = tmp.length();
            while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\'))
                --i;
            tmp.insert(i, QLatin1Char('"'));
            tmp.prepend(QLatin1Char('"'));
        }
        args += QLatin1Char(' ') + tmp;
    }
    return args;
}

bool startDetached(const QString &program, const QStringList &arguments, const QString &workingDir = QString(), qint64 *pid = 0)
{
    // static const DWORD errorElevationRequired = 740;

    QString args = qt_create_commandline(program, arguments);
    bool success = false;
    PROCESS_INFORMATION pinfo;

    DWORD dwCreationFlags = (GetConsoleWindow() ? 0 : CREATE_NO_WINDOW);
    dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
    STARTUPINFOW startupInfo = { sizeof( STARTUPINFO ), 0, 0, 0,
                                 (ulong)CW_USEDEFAULT, (ulong)CW_USEDEFAULT,
                                 (ulong)CW_USEDEFAULT, (ulong)CW_USEDEFAULT,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                               };
    success = CreateProcess(0, (wchar_t*)args.utf16(),
                            0, 0, FALSE, dwCreationFlags, 0,
                            workingDir.isEmpty() ? 0 : (wchar_t*)workingDir.utf16(),
                            &startupInfo, &pinfo);

    // if (success) {
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
        if (pid)
            *pid = pinfo.dwProcessId;
    // } else if (GetLastError() == errorElevationRequired) {
    //     success = startDetachedUacPrompt(program, arguments, workingDir, pid);
    // }

    return success;
}
#else
bool startDetached(const QString &program, const QStringList &arguments, const QString &workingDir = QString(), qint64 *pid = 0)
{
    return QProcess::startDetached(program, arguments, workingDir, pid);
}
#endif

void OCUpdater::slotStartInstaller()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    settings.setValue(autoUpdateAttemptedC, true);
    settings.sync();
    qCInfo(lcUpdater) << "Running updater" << updateFile;

    if(updateFile.endsWith(".exe")) {
        QProcess::startDetached(updateFile, QStringList() << "/S"
                                                          << "/launch");
    } else if(updateFile.endsWith(".msi")) {
        // When MSIs are installed without gui they cannot launch applications
        // as they lack the user context. That is why we need to run the client
        // manually here. We wrap the msiexec and client invocation in a powershell
        // script because owncloud.exe will be shut down for installation.
        // | Out-Null forces powershell to wait for msiexec to finish.
        auto preparePathForPowershell = [](QString path) {
            path.replace("'", "''");

            return QDir::toNativeSeparators(path);
        };

        QString msiLogFile = cfg.configPath() + "msi.log";
        QString command = QString("&{msiexec /norestart /passive /i '%1' /L*V '%2'| Out-Null ; &'%3'}")
             .arg(preparePathForPowershell(updateFile))
             .arg(preparePathForPowershell(msiLogFile))
             .arg(preparePathForPowershell(QCoreApplication::applicationFilePath()));

        startDetached("powershell.exe", QStringList{"-Command", command});
    }
}

void OCUpdater::checkForUpdate()
{
    QNetworkReply *reply = _accessManager->get(QNetworkRequest(_updateUrl));
    connect(_timeoutWatchdog, &QTimer::timeout, this, &OCUpdater::slotTimedOut);
    _timeoutWatchdog->start(30 * 1000);
    connect(reply, &QNetworkReply::finished, this, &OCUpdater::slotVersionInfoArrived);

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
    _timeoutWatchdog->stop();
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcUpdater) << "Failed to reach version check url: " << reply->errorString();
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok;
    _updateInfo = UpdateInfo::parseString(xml, &ok);
    if (ok) {
        versionInfoArrived(_updateInfo);
    } else {
        qCWarning(lcUpdater) << "Could not parse update information.";
    }
}

void OCUpdater::slotTimedOut()
{
    setDownloadState(DownloadTimedOut);
}

////////////////////////////////////////////////////////////////////////

NSISUpdater::NSISUpdater(const QUrl &url)
    : OCUpdater(url)
    , _showFallbackMessage(false)
{
}

void NSISUpdater::slotWriteFile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void NSISUpdater::slotDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        setDownloadState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();
    QFile::copy(_file->fileName(), _targetFile);
    setDownloadState(DownloadComplete);
    qCInfo(lcUpdater) << "Downloaded" << url.toString() << "to" << _targetFile;
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
    if (info.version().isEmpty()
        || infoVersion <= currVersion
        || infoVersion <= seenVersion) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        QString url = info.downloadUrl();
        qint64 autoUpdateFailedVersion =
            Helper::stringVersionToInt(settings.value(autoUpdateFailedVersionC).toString());
        if (url.isEmpty() || _showFallbackMessage || infoVersion == autoUpdateFailedVersion) {
            showDialog(info);
        }
        if (!url.isEmpty()) {
            _targetFile = cfg.configPath() + url.mid(url.lastIndexOf('/')+1);
            if (QFile(_targetFile).exists()) {
                setDownloadState(DownloadComplete);
            } else {
                QNetworkReply *reply = qnam()->get(QNetworkRequest(QUrl(url)));
                connect(reply, &QIODevice::readyRead, this, &NSISUpdater::slotWriteFile);
                connect(reply, &QNetworkReply::finished, this, &NSISUpdater::slotDownloadFinished);
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
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(info.versionString()), Utility::escape(clientVersion()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    QDialogButtonBox *bb = new QDialogButtonBox;
    bb->setWindowFlags(bb->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QPushButton *skip = bb->addButton(tr("Skip this version"), QDialogButtonBox::ResetRole);
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(skip, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(reject, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(skip, &QAbstractButton::clicked, this, &NSISUpdater::slotSetSeenVersion);
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

PassiveUpdateNotifier::PassiveUpdateNotifier(const QUrl &url)
    : OCUpdater(url)
{
    // remember the version of the currently running binary. On Linux it might happen that the
    // package management updates the package while the app is running. This is detected in the
    // updater slot: If the installed binary on the hd has a different version than the one
    // running, the running app is restarted. That happens in folderman.
    _runningAppVersion = Utility::versionOfInstalledBinary();
}

void PassiveUpdateNotifier::backgroundCheckForUpdate()
{
    if (Utility::isLinux()) {
        // on linux, check if the installed binary is still the same version
        // as the one that is running. If not, restart if possible.
        const QByteArray fsVersion = Utility::versionOfInstalledBinary();
        if (!(fsVersion.isEmpty() || _runningAppVersion.isEmpty()) && fsVersion != _runningAppVersion) {
            emit requestRestart();
        }
    }

    OCUpdater::backgroundCheckForUpdate();
}

void PassiveUpdateNotifier::versionInfoArrived(const UpdateInfo &info)
{
    qint64 currentVer = Helper::currentVersionToInt();
    qint64 remoteVer = Helper::stringVersionToInt(info.version());

    if (info.version().isEmpty() || currentVer >= remoteVer) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        setDownloadState(UpdateOnlyAvailableThroughSystem);
    }
}

} // ns mirall
