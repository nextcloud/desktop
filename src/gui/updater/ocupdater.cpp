/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include <cstdio>

namespace OCC {
namespace {
void raiseUpdaterDialog(QWidget *widget);

QWidget *updaterDialogParent()
{
    if (auto *active = QApplication::activeWindow()) {
        return active;
    }

    if (auto *focus = QApplication::focusWidget()) {
        return focus->window();
    }

    return nullptr;
}

bool attachAndOpenUpdaterDialog(QDialog *dialog, const bool removeContextHelpButton, const bool requireParent)
{
    if (!dialog || dialog->isVisible()) {
        return dialog != nullptr;
    }

    auto *parent = updaterDialogParent();
    if (requireParent && !parent) {
        return false;
    }

    if (parent) {
        dialog->setParent(parent, dialog->windowFlags());
        dialog->setWindowModality(Qt::WindowModal);
    } else {
        dialog->setWindowModality(Qt::ApplicationModal);
    }

    auto flags = dialog->windowFlags() | Qt::WindowStaysOnTopHint;
    if (removeContextHelpButton) {
        flags &= ~Qt::WindowContextHelpButtonHint;
    }
    dialog->setWindowFlags(flags);
    dialog->open();
    raiseUpdaterDialog(dialog);
    return true;
}

void presentUpdaterDialog(QDialog *dialog, const bool removeContextHelpButton)
{
    if (!dialog) {
        return;
    }

    if (attachAndOpenUpdaterDialog(dialog, removeContextHelpButton, true)) {
        return;
    }

    auto *timer = new QTimer(dialog);
    timer->setInterval(150);
    timer->setSingleShot(false);
    QObject::connect(timer, &QTimer::timeout, dialog, [dialog, removeContextHelpButton, timer]() {
        if (!dialog) {
            timer->stop();
            timer->deleteLater();
            return;
        }

        if (attachAndOpenUpdaterDialog(dialog, removeContextHelpButton, true)) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();

    QTimer::singleShot(2000, dialog, [dialog, removeContextHelpButton, timer]() {
        if (!dialog || dialog->isVisible()) {
            return;
        }

        attachAndOpenUpdaterDialog(dialog, removeContextHelpButton, false);
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
    });

    QObject::connect(qApp, &QApplication::focusChanged, dialog, [dialog]() {
        if (!dialog || !dialog->isVisible()) {
            return;
        }

        raiseUpdaterDialog(dialog);
    });
}

void raiseUpdaterDialog(QWidget *widget)
{
    if (!widget) {
        return;
    }

    widget->showNormal();
    widget->raise();
    widget->activateWindow();
}
} // namespace

UpdaterScheduler::UpdaterScheduler(QObject *parent)
    : QObject(parent)
{
    connect(&_updateCheckTimer, &QTimer::timeout,
        this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an OCUpdater
    if (auto *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
        connect(updater, &OCUpdater::newUpdateAvailable,
            this, &UpdaterScheduler::updaterAnnouncement);
        connect(updater, &OCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    }

    // at startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    ConfigFile cfg;
    auto checkInterval = cfg.updateCheckInterval();
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());
}

void UpdaterScheduler::slotTimerFired()
{
    ConfigFile cfg;

    // re-set the check interval if it changed in the config file meanwhile
    auto checkInterval = std::chrono::milliseconds(cfg.updateCheckInterval()).count();
    if (checkInterval != _updateCheckTimer.interval()) {
        _updateCheckTimer.setInterval(checkInterval);
        qCInfo(lcUpdater) << "Setting new update check interval " << checkInterval;
    }

    // consider the skipUpdateCheck and !autoUpdateCheck flags in the config.
    if (cfg.skipUpdateCheck() || !cfg.autoUpdateCheck()) {
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
    QString updateFile = settings.value(updateAvailableKey).toString();
    if (!updateFile.isEmpty() && QFile(updateFile).exists()
        && !updateSucceeded() /* Someone might have run the updater manually between restarts */) {
        const auto messageBoxStartInstaller = new QMessageBox(QMessageBox::Information,
            tr("New %1 update ready").arg(Theme::instance()->appNameGUI()),
            tr("A new update for %1 is about to be installed. The updater may ask "
               "for additional privileges during the process. Your computer may reboot to complete the installation.")
                .arg(Theme::instance()->appNameGUI()),
            QMessageBox::Ok | QMessageBox::Cancel,
            nullptr);

        messageBoxStartInstaller->setAttribute(Qt::WA_DeleteOnClose);

        connect(messageBoxStartInstaller, &QMessageBox::buttonClicked, this, [this, messageBoxStartInstaller](QAbstractButton *button) {
            auto clicked = messageBoxStartInstaller->standardButton(button);
            if (clicked == QMessageBox::Ok) {
                slotStartInstaller();
            }
        });
        presentUpdaterDialog(messageBoxStartInstaller, false);
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

QString OCUpdater::statusString(UpdateStatusStringFormat format) const
{
    QString updateVersion = _updateInfo.versionString();

    switch (downloadState()) {
    case Downloading:
        return tr("Downloading %1 …").arg(updateVersion);
    case DownloadComplete:
        return tr("%1 available. Restart application to start the update.").arg(updateVersion);
    case DownloadFailed: {
        if (format == UpdateStatusStringFormat::Html) {
            return tr("Could not download update. Please open <a href='%1'>%1</a> to download the update manually.").arg(_updateInfo.web());
        }
        return tr("Could not download update. Please open %1 to download the update manually.").arg(_updateInfo.web());
    }
    case DownloadTimedOut:
        return tr("Could not check for new updates.");
    case UpdateOnlyAvailableThroughSystem: {
        if (format == UpdateStatusStringFormat::Html) {
            return tr("New %1 is available. Please open <a href='%2'>%2</a> to download the update.").arg(updateVersion, _updateInfo.web());
        }
        return tr("New %1 is available. Please open %2 to download the update.").arg(updateVersion, _updateInfo.web());
    }
    case CheckingServer:
        return tr("Checking update server …");
    case Unknown:
        return tr("Update status is unknown: Did not check for new updates.");
    case UpToDate:
    // fall through
    default: {
        ConfigFile configFile;
        if (configFile.serverHasValidSubscription()) {
            return tr("You are using the %1 update channel. Your installation is the latest version.")
                .arg(configFile.currentUpdateChannel());
        }

        return tr("No updates available. Your installation is the latest version.");
    }
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
        emit newUpdateAvailable(tr("Update Check"), statusString(), _updateInfo.web());
    }
}

void OCUpdater::slotStartInstaller()
{
    qCWarning(lcUpdater) << "slotStartInstaller called on non-NSIS updater";
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

    qint64 targetVersionInt = Helper::stringVersionToInt(settings.value(updateTargetVersionKey).toString());
    qint64 currentVersion = Helper::currentVersionToInt();
    return currentVersion >= targetVersionInt;
}

void OCUpdater::slotVersionInfoArrived()
{
    qCDebug(lcUpdater) << "received a reply";
    _timeoutWatchdog->stop();
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcUpdater) << "Failed to reach version check url: " << reply->errorString();
        setDownloadState(DownloadTimedOut);
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok = false;
    _updateInfo = UpdateInfo::parseString(xml, &ok);
    if (ok) {
        versionInfoArrived(_updateInfo);
    } else {
        qCWarning(lcUpdater) << "Could not parse update information.";
        setDownloadState(DownloadTimedOut);
    }
}

void OCUpdater::slotTimedOut()
{
    setDownloadState(DownloadTimedOut);
}

////////////////////////////////////////////////////////////////////////

NSISUpdater::NSISUpdater(const QUrl &url)
    : OCUpdater(url)
{
}

void NSISUpdater::slotWriteFile()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void NSISUpdater::wipeUpdateData()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFileName = settings.value(updateAvailableKey).toString();
    if (!updateFileName.isEmpty())
        QFile::remove(updateFileName);
    settings.remove(updateAvailableKey);
    settings.remove(updateTargetVersionKey);
    settings.remove(updateTargetVersionStringKey);
    settings.remove(autoUpdateAttemptedKey);
}

void NSISUpdater::slotDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        setDownloadState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();

    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);

    // remove previously downloaded but not used installer
    QFile oldTargetFile(settings.value(updateAvailableKey).toString());
    if (oldTargetFile.exists()) {
        oldTargetFile.remove();
    }

    QFile::copy(_file->fileName(), _targetFile);
    setDownloadState(DownloadComplete);
    qCInfo(lcUpdater) << "Downloaded" << url.toString() << "to" << _targetFile;
    settings.setValue(updateTargetVersionKey, updateInfo().version());
    settings.setValue(updateTargetVersionStringKey, updateInfo().versionString());
    settings.setValue(updateAvailableKey, _targetFile);
}

void NSISUpdater::versionInfoArrived(const UpdateInfo &info)
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    qint64 infoVersion = Helper::stringVersionToInt(info.version());
    qint64 currVersion = Helper::currentVersionToInt();
    qCInfo(lcUpdater) << "Version info arrived:"
            << "Your version:" << currVersion
            << "Available version:" << infoVersion << info.version()
            << "Available version string:" << info.versionString()
            << "Web url:" << info.web()
            << "Download url:" << info.downloadUrl();
    if (info.version().isEmpty())
    {
        qCInfo(lcUpdater) << "No version information available at the moment";
        setDownloadState(UpToDate);
    } else {
        const auto currentVer = Helper::currentVersionToInt();
        const auto remoteVer = Helper::stringVersionToInt(info.version());

        if (info.version().isEmpty() || currentVer >= remoteVer) {
            qCInfo(lcUpdater) << "Client is on latest version!";
            setDownloadState(UpToDate);
        } else {
            const auto url = info.downloadUrl();
            if (url.isEmpty()) {
                showNoUrlDialog(info);
            } else {
                _targetFile = cfg.configPath() + url.mid(url.lastIndexOf('/')+1);
                if (QFile(_targetFile).exists()) {
                    setDownloadState(DownloadComplete);
                } else {
                    auto request = QNetworkRequest(QUrl(url));
                    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                    QNetworkReply *reply = qnam()->get(request);
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
}

void NSISUpdater::showNoUrlDialog(const UpdateInfo &info)
{
    // if the version tag is set, there is a newer version.
    auto *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    auto *layout = new QVBoxLayout(msgBox);
    auto *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("New Version Available"));

    auto *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    auto *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available.</p>"
                     "<p><b>%2</b> is available for download. The installed version is %3.</p>")
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(info.versionString()), Utility::escape(clientVersion()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    auto *bb = new QDialogButtonBox;
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(reject, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(getupdate, &QAbstractButton::clicked, this, &NSISUpdater::slotOpenUpdateUrl);

    layout->addWidget(bb);

    presentUpdaterDialog(msgBox, true);
}

void NSISUpdater::showUpdateErrorDialog(const QString &targetVersion)
{
    auto msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    auto layout = new QVBoxLayout(msgBox);
    auto hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("Update Failed"));

    auto ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    auto lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available but the updating process failed.</p>"
                     "<p><b>%2</b> has been downloaded. The installed version is %3. If you confirm restart and update, your computer may reboot to complete the installation.</p>")
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(targetVersion), Utility::escape(clientVersion()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    auto bb = new QDialogButtonBox;
    auto askagain = bb->addButton(tr("Ask again later"), QDialogButtonBox::ResetRole);
    auto retry = bb->addButton(tr("Restart and update"), QDialogButtonBox::AcceptRole);
    auto getupdate = bb->addButton(tr("Update manually"), QDialogButtonBox::AcceptRole);

    connect(askagain, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(retry, &QAbstractButton::clicked, msgBox, &QDialog::accept);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    // askagain: do nothing
    connect(retry, &QAbstractButton::clicked, this, [this]() {
        slotStartInstaller();
    });
    connect(getupdate, &QAbstractButton::clicked, this, [this]() {
        slotOpenUpdateUrl();
    });

    layout->addWidget(bb);

    presentUpdaterDialog(msgBox, true);
}

bool NSISUpdater::handleStartup()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);

    // no need to try to install a previously fetched update when the user doesn't want automated updates
    if (cfg.skipUpdateCheck() || !cfg.autoUpdateCheck()) {
        qCInfo(lcUpdater) << "Skipping installation of update due to config settings";
        return false;
    }

    QString updateFileName = settings.value(updateAvailableKey).toString();
    // has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        qCInfo(lcUpdater) << "An updater file is available";
        // did it try to execute the update?
        if (settings.value(autoUpdateAttemptedKey, false).toBool()) {
            if (updateSucceeded()) {
                // success: clean up
                qCInfo(lcUpdater) << "The requested update attempt has succeeded"
                        << Helper::currentVersionToInt();
                wipeUpdateData();
                return false;
            } else {
                // auto update failed. Ask user what to do
                qCInfo(lcUpdater) << "The requested update attempt has failed"
                        << settings.value(updateTargetVersionKey).toString();
                showUpdateErrorDialog(settings.value(updateTargetVersionStringKey).toString());
                return false;
            }
        } else {
            qCInfo(lcUpdater) << "Triggering an update";
            return performUpdate();
        }
    }
    return false;
}

void NSISUpdater::slotStartInstaller()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableKey).toString();
    settings.setValue(autoUpdateAttemptedKey, true);
    settings.sync();
    qCInfo(lcUpdater) << "Running updater" << updateFile;

    if (updateFile.endsWith(".exe")) {
        QProcess::startDetached(updateFile, QStringList() << "/S"
                                                          << "/launch");
    } else if (updateFile.endsWith(".msi")) {
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
        QString command = QStringLiteral("&{msiexec /i '%1' /L*V '%2'| Out-Null ; &'%3'}")
             .arg(preparePathForPowershell(updateFile))
             .arg(preparePathForPowershell(msiLogFile))
             .arg(preparePathForPowershell(QCoreApplication::applicationFilePath()));

        QProcess::startDetached("powershell.exe", QStringList{"-Command", command});
    }
    qApp->quit();
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
        qCInfo(lcUpdater) << "Client is on older version. We will update!";
        setDownloadState(UpdateOnlyAvailableThroughSystem);
    }
}

} // ns mirall
