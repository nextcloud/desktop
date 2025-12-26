/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
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
#include <QProcess>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStyle>

namespace OCC {

Q_LOGGING_CATEGORY(lcAppImageUpdater, "nextcloud.gui.updater.appimage", QtInfoMsg)

// Config key constants - use string literals to avoid conflicts with ocupdater.cpp in Unity builds
static constexpr auto updateAvailableKey = "Updater/updateAvailable";
static constexpr auto updateTargetVersionKey = "Updater/updateTargetVersion";
static constexpr auto updateTargetVersionStringKey = "Updater/updateTargetVersionString";
static constexpr auto autoUpdateAttemptedKey = "Updater/autoUpdateAttempted";

AppImageUpdater::AppImageUpdater(const QUrl &url)
    : OCUpdater(url)
{
    qCInfo(lcAppImageUpdater) << "AppImageUpdater constructed with URL:" << url.toString();
    qCInfo(lcAppImageUpdater) << "Current AppImage path:" << currentAppImagePath();
    qCInfo(lcAppImageUpdater) << "Can write to location:" << canWriteToAppImageLocation();
}

bool AppImageUpdater::isRunningAppImage()
{
    const QString appImagePath = qEnvironmentVariable("APPIMAGE");
    return !appImagePath.isNull() && QFile::exists(appImagePath);
}

QString AppImageUpdater::currentAppImagePath()
{
    return qEnvironmentVariable("APPIMAGE");
}

bool AppImageUpdater::canWriteToAppImageLocation() const
{
    const QString appImagePath = currentAppImagePath();
    if (appImagePath.isEmpty()) {
        return false;
    }

    QFileInfo appImageInfo(appImagePath);
    // We only need to check if the directory is writable because we replace the file
    // by moving the new one over it (which requires directory write permissions).
    // The file itself might report not writable if it is currently running (ETXTBSY).

    // Check if the directory is writable (needed for backup during replacement)
    QFileInfo dirInfo(appImageInfo.dir().path());
    if (!dirInfo.isWritable()) {
        qCInfo(lcAppImageUpdater) << "AppImage directory is not writable:" << dirInfo.path();
        return false;
    }

    return true;
}

void AppImageUpdater::wipeUpdateData()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFileName = settings.value(QLatin1String(updateAvailableKey)).toString();
    if (!updateFileName.isEmpty()) {
        QFile::remove(updateFileName);
    }
    settings.remove(QLatin1String(updateAvailableKey));
    settings.remove(QLatin1String(updateTargetVersionKey));
    settings.remove(QLatin1String(updateTargetVersionStringKey));
    settings.remove(QLatin1String(autoUpdateAttemptedKey));
}

void AppImageUpdater::slotWriteFile()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        QByteArray data = reply->readAll();
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

    // Handle Redirects Manually
    // GitHub releases often use 302 redirects which might not be automatically followed
    // depending on the QNetworkRequest::RedirectPolicy. We handle them explicitly here
    // to ensure the download continues to the final location.
    QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!redirectionTarget.isNull()) {
        QUrl newUrl = reply->url().resolved(redirectionTarget.toUrl());
        
        // Clear any data written from the redirect response (e.g. HTML body)
        // Some servers send a small HTML body with the 302 redirect.
        // Since we are writing to the file in slotWriteFile, we must discard
        // this data to avoid corrupting the start of the AppImage binary.
        if (_file->isOpen()) {
            _file->resize(0);
            _file->seek(0);
        }

        auto request = QNetworkRequest(newUrl);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *newReply = qnam()->get(request);
        connect(newReply, &QIODevice::readyRead, this, &AppImageUpdater::slotWriteFile);
        connect(newReply, &QNetworkReply::finished, this, &AppImageUpdater::slotDownloadFinished);
        return;
    }

    QUrl url(reply->url());
    _file->close();

    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);

    // Remove previously downloaded but not used update file
    QFile oldTargetFile(settings.value(QLatin1String(updateAvailableKey)).toString());
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
    QFile targetFile(_targetFile);
    targetFile.setPermissions(targetFile.permissions() | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOwner);

    setDownloadState(DownloadComplete);
    qCInfo(lcAppImageUpdater) << "Downloaded" << url.toString() << "to" << _targetFile;
    settings.setValue(QLatin1String(updateTargetVersionKey), updateInfo().version());
    settings.setValue(QLatin1String(updateTargetVersionStringKey), updateInfo().versionString());
    settings.setValue(QLatin1String(updateAvailableKey), _targetFile);
}

void AppImageUpdater::versionInfoArrived(const UpdateInfo &info)
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    qint64 infoVersion = Helper::stringVersionToInt(info.version());
    qint64 currVersion = Helper::currentVersionToInt();
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

    QString url = info.downloadUrl();
    if (url.isEmpty()) {
        qCInfo(lcAppImageUpdater) << "No download URL provided";
        setDownloadState(UpdateOnlyAvailableThroughSystem);
        return;
    }

    // Fix for incorrect architecture in download URL from server
    if (url.endsWith(QLatin1String("-x64.AppImage"))) {
        qCInfo(lcAppImageUpdater) << "Correcting download URL architecture from x64 to x86_64";
        url.replace(QLatin1String("-x64.AppImage"), QLatin1String("-x86_64.AppImage"));
    }

    // Download to config directory
    _targetFile = cfg.configPath() + QStringLiteral("nextcloud-update.AppImage");

    // Check if already downloaded
    if (QFile(_targetFile).exists()) {
        qCInfo(lcAppImageUpdater) << "Update already downloaded at" << _targetFile;
        setDownloadState(DownloadComplete);
        return;
    }

    // Start download
    auto request = QNetworkRequest(QUrl(url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = qnam()->get(request);
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
    msgBox->setWindowFlags(msgBox->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    auto *layout = new QVBoxLayout(msgBox);
    auto *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("Update Failed"));

    auto *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    auto *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available but the updating process failed.</p>"
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

    msgBox->open();
}

bool AppImageUpdater::handleStartup()
{
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);

    // No need to try to install a previously fetched update when the user doesn't want automated updates
    if (cfg.skipUpdateCheck() || !cfg.autoUpdateCheck()) {
        qCInfo(lcAppImageUpdater) << "Skipping installation of update due to config settings";
        return false;
    }

    QString updateFileName = settings.value(QLatin1String(updateAvailableKey)).toString();
    // Has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        qCInfo(lcAppImageUpdater) << "An updater file is available:" << updateFileName;
        // Did it try to execute the update?
        if (settings.value(QLatin1String(autoUpdateAttemptedKey), false).toBool()) {
            if (updateSucceeded()) {
                // Success: clean up
                qCInfo(lcAppImageUpdater) << "The requested update attempt has succeeded"
                                          << Helper::currentVersionToInt();
                wipeUpdateData();
                return false;
            } else {
                // Auto update failed. Ask user what to do
                qCInfo(lcAppImageUpdater) << "The requested update attempt has failed"
                                          << settings.value(QLatin1String(updateTargetVersionKey)).toString();
                showUpdateErrorDialog(settings.value(QLatin1String(updateTargetVersionStringKey)).toString());
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
    ConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(QLatin1String(updateAvailableKey)).toString();
    QString currentAppImage = currentAppImagePath();

    if (updateFile.isEmpty() || currentAppImage.isEmpty()) {
        qCWarning(lcAppImageUpdater) << "Missing update file or current AppImage path";
        return;
    }

    settings.setValue(QLatin1String(autoUpdateAttemptedKey), true);
    settings.sync();
    qCInfo(lcAppImageUpdater) << "Starting AppImage update from" << updateFile << "to" << currentAppImage;

    // Create a shell script that will:
    // 1. Wait for this process to exit
    // 2. Replace the current AppImage with the new one
    // 3. Relaunch the application
    //
    // We use a script because we can't replace ourselves while running
    QString scriptContent = QStringLiteral(
        "#!/bin/bash\n"
        "sleep 2\n"
        "CURRENT=\"%1\"\n"
        "NEW=\"%2\"\n"
        "BACKUP=\"${CURRENT}.backup\"\n"
        "# Backup current AppImage\n"
        "mv -f \"$CURRENT\" \"$BACKUP\" 2>/dev/null\n"
        "# Replace with new version\n"
        "if mv -f \"$NEW\" \"$CURRENT\"; then\n"
        "    chmod +x \"$CURRENT\"\n"
        "    rm -f \"$BACKUP\"\n"
        "    \"$CURRENT\" &\n"
        "else\n"
        "    # Restore backup on failure\n"
        "    mv -f \"$BACKUP\" \"$CURRENT\" 2>/dev/null\n"
        "    \"$CURRENT\" &\n"
        "fi\n"
    ).arg(currentAppImage, updateFile);

    QString scriptPath = QDir::tempPath() + QStringLiteral("/nextcloud-appimage-update.sh");
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        scriptFile.write(scriptContent.toUtf8());
        scriptFile.setPermissions(scriptFile.permissions() | QFile::ExeUser | QFile::ExeGroup);
        scriptFile.close();

        qCInfo(lcAppImageUpdater) << "Created update script at" << scriptPath;
        QProcess::startDetached(QStringLiteral("/bin/bash"), QStringList{scriptPath});
        qApp->quit();
    } else {
        qCWarning(lcAppImageUpdater) << "Failed to create update script at" << scriptPath;
    }
}

} // namespace OCC
