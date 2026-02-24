/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updater/appimageupdater.h"

#include "theme.h"
#include "configfile.h"
#include "common/utility.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkReply>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStyle>
#include <QApplication>
#include <QTimer>

namespace OCC {
namespace {
void raiseAppImageDialog(QWidget *widget);

QWidget *appImageDialogParent()
{
    if (auto *active = QApplication::activeWindow()) {
        return active;
    }

    if (auto *focus = QApplication::focusWidget()) {
        return focus->window();
    }

    return nullptr;
}

bool attachAndOpenAppImageDialog(QDialog *dialog, const bool removeContextHelpButton, const bool requireParent)
{
    if (!dialog || dialog->isVisible()) {
        return dialog != nullptr;
    }

    auto *parent = appImageDialogParent();
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
    raiseAppImageDialog(dialog);
    return true;
}

void presentAppImageDialog(QDialog *dialog, const bool removeContextHelpButton)
{
    if (!dialog) {
        return;
    }

    if (attachAndOpenAppImageDialog(dialog, removeContextHelpButton, true)) {
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

        if (attachAndOpenAppImageDialog(dialog, removeContextHelpButton, true)) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();

    QTimer::singleShot(2000, dialog, [dialog, removeContextHelpButton, timer]() {
        if (!dialog || dialog->isVisible()) {
            return;
        }

        attachAndOpenAppImageDialog(dialog, removeContextHelpButton, false);
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
    });

    QObject::connect(qApp, &QApplication::focusChanged, dialog, [dialog]() {
        if (!dialog || !dialog->isVisible()) {
            return;
        }

        raiseAppImageDialog(dialog);
    });
}

void raiseAppImageDialog(QWidget *widget)
{
    if (!widget) {
        return;
    }

    widget->showNormal();
    widget->raise();
    widget->activateWindow();
}
} // namespace

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcAppImageUpdater, "nextcloud.gui.updater.appimage", QtInfoMsg)

AppImageUpdater::AppImageUpdater(const QUrl &url)
    : OCUpdater(url)
{
    qCInfo(lcAppImageUpdater) << "AppImageUpdater constructed with URL:" << url.toString();
    qCInfo(lcAppImageUpdater) << "Current AppImage path:" << currentAppImagePath();
    qCInfo(lcAppImageUpdater) << "Can write to location:" << canWriteToAppImageLocation();
}

bool AppImageUpdater::isRunningAppImage()
{
    return Utility::isRunningInAppImage();
}

QString AppImageUpdater::currentAppImagePath()
{
    return Utility::appImagePath();
}

bool AppImageUpdater::canWriteToAppImageLocation() const
{
    const auto appImagePath = currentAppImagePath();
    if (appImagePath.isEmpty()) {
        return false;
    }

    const auto appImageInfo = QFileInfo{appImagePath};
    // We only need to check if the directory is writable because we replace the file
    // by moving the new one over it (which requires directory write permissions).
    // The file itself might report not writable if it is currently running (ETXTBSY).

    // Check if the directory is writable (needed for backup during replacement)
    const auto dirInfo = QFileInfo{appImageInfo.dir().path()};
    if (!dirInfo.isWritable()) {
        qCInfo(lcAppImageUpdater) << "AppImage directory is not writable:" << dirInfo.path();
        return false;
    }

    return true;
}

void AppImageUpdater::wipeUpdateData()
{
    const auto cfg = ConfigFile{};
    auto settings = QSettings{cfg.configFile(), QSettings::IniFormat};
    const auto updateFileName = settings.value(updateAvailableKey).toString();
    if (!updateFileName.isEmpty()) {
        QFile::remove(updateFileName);
    }
    settings.remove(updateAvailableKey);
    settings.remove(updateTargetVersionKey);
    settings.remove(updateTargetVersionStringKey);
    settings.remove(autoUpdateAttemptedKey);
}

void AppImageUpdater::slotWriteFile()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        const auto data = reply->readAll();
        _file->write(data);
    }
}

void AppImageUpdater::slotDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcAppImageUpdater) << "Download failed:" << reply->errorString();
        setDownloadState(DownloadFailed);
        return;
    }

    const auto url = reply->url();
    _file->close();

    const auto cfg = ConfigFile{};
    auto settings = QSettings{cfg.configFile(), QSettings::IniFormat};

    // Remove previously downloaded but not used update file
    auto oldTargetFile = QFile{settings.value(updateAvailableKey).toString()};
    if (oldTargetFile.exists()) {
        oldTargetFile.remove();
    }

    // Copy downloaded file to target location
    if (!QFile::copy(_file->fileName(), _targetFile)) {
        qCWarning(lcAppImageUpdater) << "Failed to copy update file to" << _targetFile;
        setDownloadState(DownloadFailed);
        return;
    }

    // Make the downloaded AppImage executable
    auto targetFile = QFile{_targetFile};
    targetFile.setPermissions(targetFile.permissions() | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOwner);

    setDownloadState(DownloadComplete);
    qCInfo(lcAppImageUpdater) << "Downloaded" << url.toString() << "to" << _targetFile;
    settings.setValue(updateTargetVersionKey, updateInfo().version());
    settings.setValue(updateTargetVersionStringKey, updateInfo().versionString());
    settings.setValue(updateAvailableKey, _targetFile);
}

void AppImageUpdater::versionInfoArrived(const UpdateInfo &info)
{
    const auto cfg = ConfigFile{};
    const auto infoVersion = Helper::stringVersionToInt(info.version());
    const auto currVersion = Helper::currentVersionToInt();
    qCInfo(lcAppImageUpdater) << "Version info arrived:"
                              << "Your version:" << currVersion
                              << "Available version:" << infoVersion << info.version()
                              << "Available version string:" << info.versionString()
                              << "Web url:" << info.web()
                              << "Download url:" << info.downloadUrl();

    if (info.version().isEmpty()) {
        qCInfo(lcAppImageUpdater) << "No version information available at the moment";
        setDownloadState(UpToDate);
        return;
    }

    const auto currentVer = Helper::currentVersionToInt();
    const auto remoteVer = Helper::stringVersionToInt(info.version());

    if (currentVer >= remoteVer) {
        qCInfo(lcAppImageUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
        return;
    }

    // Check if we can write to the AppImage location
    if (!canWriteToAppImageLocation()) {
        qCInfo(lcAppImageUpdater) << "Cannot write to AppImage location, falling back to notification only";
        setDownloadState(UpdateOnlyAvailableThroughSystem);
        return;
    }

    const auto url = info.downloadUrl();
    if (url.isEmpty()) {
        qCInfo(lcAppImageUpdater) << "No download URL provided";
        setDownloadState(UpdateOnlyAvailableThroughSystem);
        return;
    }

    // Download to config directory
    const auto fileName = QFileInfo{QUrl{url}.path()}.fileName();
    const auto targetFileName = fileName.isEmpty() ? "nextcloud-update.AppImage"_L1 : fileName;
    _targetFile = QDir(cfg.configPath()).filePath(targetFileName);

    // Check if already downloaded
    if (QFile(_targetFile).exists()) {
        qCInfo(lcAppImageUpdater) << "Update already downloaded at" << _targetFile;
        setDownloadState(DownloadComplete);
        return;
    }

    // Start download
    auto request = QNetworkRequest(QUrl{url});
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = qnam()->get(request);
    connect(reply, &QIODevice::readyRead, this, &AppImageUpdater::slotWriteFile);
    connect(reply, &QNetworkReply::finished, this, &AppImageUpdater::slotDownloadFinished);
    setDownloadState(Downloading);

    _file.reset(new QTemporaryFile);
    _file->setAutoRemove(true);
    _file->open();
}

void AppImageUpdater::showUpdateErrorDialog(const QString &targetVersion)
{
    auto *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    const auto infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    const auto iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    auto *layout = new QVBoxLayout(msgBox);
    auto *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("Update Failed"));

    auto *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    auto *lbl = new QLabel;
    const auto txt = tr("<p>A new version of the %1 Client is available but the updating process failed.</p>"
                        "<p><b>%2</b> has been downloaded. The installed version is %3.</p>")
                         .arg(Utility::escape(Theme::instance()->appNameGUI()),
                             Utility::escape(targetVersion), Utility::escape(clientVersion()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    auto *bb = new QDialogButtonBox;
    auto *askagain = bb->addButton(tr("Ask again later"), QDialogButtonBox::ResetRole);
    auto *retry = bb->addButton(tr("Restart and update"), QDialogButtonBox::AcceptRole);
    auto *getupdate = bb->addButton(tr("Update manually"), QDialogButtonBox::AcceptRole);

    connect(askagain, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(retry, &QAbstractButton::clicked, msgBox, &QDialog::accept);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(retry, &QAbstractButton::clicked, this, [this]() {
        slotStartInstaller();
    });
    connect(getupdate, &QAbstractButton::clicked, this, &AppImageUpdater::slotOpenUpdateUrl);

    layout->addWidget(bb);

    presentAppImageDialog(msgBox, true);
}

bool AppImageUpdater::handleStartup()
{
    const auto cfg = ConfigFile{};
    auto settings = QSettings{cfg.configFile(), QSettings::IniFormat};

    // No need to try to install a previously fetched update when the user doesn't want automated updates
    if (cfg.skipUpdateCheck() || !cfg.autoUpdateCheck()) {
        qCInfo(lcAppImageUpdater) << "Skipping installation of update due to config settings";
        return false;
    }

    const auto updateFileName = settings.value(updateAvailableKey).toString();
    // Has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        qCInfo(lcAppImageUpdater) << "An updater file is available:" << updateFileName;
        // Did it try to execute the update?
        if (settings.value(autoUpdateAttemptedKey, false).toBool()) {
            if (updateSucceeded()) {
                // Success: clean up
                qCInfo(lcAppImageUpdater) << "The requested update attempt has succeeded"
                                          << Helper::currentVersionToInt();
                wipeUpdateData();
                return false;
            } else {
                // Auto update failed. Ask user what to do
                qCInfo(lcAppImageUpdater) << "The requested update attempt has failed"
                                          << settings.value(updateTargetVersionKey).toString();
                showUpdateErrorDialog(settings.value(updateTargetVersionStringKey).toString());
                return false;
            }
        } else {
            qCInfo(lcAppImageUpdater) << "Triggering an update";
            return performUpdate();
        }
    }
    return false;
}

void AppImageUpdater::slotStartInstaller()
{
    const auto cfg = ConfigFile{};
    auto settings = QSettings{cfg.configFile(), QSettings::IniFormat};
    const auto updateFile = settings.value(updateAvailableKey).toString();
    const auto currentAppImage = currentAppImagePath();

    if (updateFile.isEmpty() || currentAppImage.isEmpty()) {
        qCWarning(lcAppImageUpdater) << "Missing update file or current AppImage path";
        return;
    }

    settings.setValue(autoUpdateAttemptedKey, true);
    settings.sync();
    qCInfo(lcAppImageUpdater) << "Starting AppImage update from" << updateFile << "to" << currentAppImage;

    const auto backupPath = QString{currentAppImage + ".backup"_L1};
    QFile::remove(backupPath);

    if (!QFile::rename(currentAppImage, backupPath)) {
        qCWarning(lcAppImageUpdater) << "Failed to rename running AppImage to" << backupPath;
        showUpdateErrorDialog(updateInfo().versionString());
        return;
    }

    if (!QFile::copy(updateFile, currentAppImage)) {
        qCWarning(lcAppImageUpdater) << "Failed to copy update file to" << currentAppImage;
        QFile::rename(backupPath, currentAppImage);
        showUpdateErrorDialog(updateInfo().versionString());
        return;
    }

    auto targetFile = QFile{currentAppImage};
    targetFile.setPermissions(targetFile.permissions() | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOwner);

    if (!QFile::remove(backupPath)) {
        qCWarning(lcAppImageUpdater) << "Failed to remove backup AppImage" << backupPath;
    }

    emit requestRestart();
    qApp->quit();
}

} // namespace OCC
