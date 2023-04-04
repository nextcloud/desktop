/*
 * Copyright (C) 2022 by Fabian Müller <fmueller@owncloud.com>
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

#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <appimage/update.h>
#include <chrono>

#include "appimageupdater.h"
#include "common/version.h"
#include "libsync/configfile.h"
#include "settingsdialog.h"
#include "theme.h"
#include "updater_private.h"

#include "appimageupdateavailabledialog.h"
#include "application.h"

using namespace OCC;
using namespace std::chrono_literals;

namespace {

/**
 * libappimageupdate uses exceptions, but the client does not
 * This little shim adapts the interface to one usable within this project
 */
class AppImageUpdaterShim : public QObject
{
    Q_OBJECT

private:
    explicit AppImageUpdaterShim(const QString &zsyncFileUrl, QObject *parent = nullptr)
        : QObject(parent)
        , _updater(Utility::appImageLocation().toStdString(), true)
    {
        QString updateInformation(QStringLiteral("zsync|") + zsyncFileUrl);
        _updater.setUpdateInformation(updateInformation.toStdString());
    }

    void _logStatusMessages()
    {
        std::string currentStatusMessage;

        while (_updater.nextStatusMessage(currentStatusMessage)) {
            qCInfo(lcUpdater) << "AppImageUpdate:" << QString::fromStdString(currentStatusMessage);
        }
    }

public:
    static AppImageUpdaterShim *makeInstance(const QString &updateInformation, QObject *parent)
    {
        try {
            return new AppImageUpdaterShim(updateInformation, parent);
        } catch (const std::exception &e) {
            qCCritical(lcUpdater) << "Failed to create updater shim:" << e.what();
            return nullptr;
        }
    }

    bool isUpdateAvailable() noexcept
    {
        try {
            bool updateAvailable;

            if (!_updater.checkForChanges(updateAvailable)) {
                _logStatusMessages();
                return false;
            }

            _logStatusMessages();
            return updateAvailable;
        } catch (const std::exception &e) {
            _logStatusMessages();
            qCCritical(lcUpdater) << "Checking for update failed:" << e.what();
            return false;
        }
    }

    void startUpdateInBackground() noexcept
    {
        // monitor progress and log status messages
        auto *timer = new QTimer(this);

        timer->setInterval(100ms);

        connect(timer, &QTimer::timeout, this, [=]() {
            _logStatusMessages();

            if (_updater.isDone()) {
                emit finished(!_updater.hasError());
                timer->stop();
            }
        });

        _updater.start();
        timer->start();
    }

signals:
    void finished(bool successfully);

private:
    appimage::update::Updater _updater;
};

} // namespace

AppImageUpdater::AppImageUpdater(const QUrl &url)
    : OCUpdater(url)
{
}


bool AppImageUpdater::handleStartup()
{
    // nothing to do, update will be performed while app is running, if anything
    return false;
}

void AppImageUpdater::versionInfoArrived(const UpdateInfo &info)
{
    const auto &currentVersion = Version::versionWithBuildNumber();
    const auto newVersion = QVersionNumber::fromString(info.version());

    if (info.version().isEmpty() || currentVersion >= newVersion) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
        return;
    }

    const auto previouslySkippedVersion = this->previouslySkippedVersion();
    if (previouslySkippedVersion >= newVersion) {
        qCInfo(lcUpdater) << "Update" << previouslySkippedVersion << "was skipped previously by user";
        setDownloadState(UpToDate);
        return;
    }

    const auto appImageUpdaterShim = AppImageUpdaterShim::makeInstance(info.downloadUrl(), this);

    if (appImageUpdaterShim == nullptr) {
        setDownloadState(DownloadFailed);
        return;
    }

    if (!appImageUpdaterShim->isUpdateAvailable()) {
        qCCritical(lcUpdater) << "Update server reported that update is available, but AppImageUpdate disagrees, aborting";
        setDownloadState(DownloadFailed);
        return;
    }

    auto dialog = new AppImageUpdateAvailableDialog(currentVersion, newVersion, ocApp()->gui()->settingsDialog());

    connect(dialog, &Ui::AppImageUpdateAvailableDialog::skipUpdateButtonClicked, this, [newVersion]() {
        qCInfo(lcUpdater) << "Update" << newVersion << "skipped by user";
        setPreviouslySkippedVersion(newVersion);
    });

    connect(dialog, &QDialog::accepted, this, [this, appImageUpdaterShim]() {
        // binding AppImageUpdaterShim shared pointer to finished callback makes sure the updater is cleaned up when it's done
        connect(appImageUpdaterShim, &AppImageUpdaterShim::finished, this, [this](bool succeeded) {
            if (succeeded) {
                qCInfo(lcUpdater) << "AppImage update complete";
                setDownloadState(DownloadComplete);
            } else {
                qCInfo(lcUpdater) << "AppImage update failed";
                setDownloadState(DownloadFailed);
            }
        });

        setDownloadState(Downloading);
        appImageUpdaterShim->startUpdateInBackground();
    });

    dialog->show();
    ownCloudGui::raiseDialog(dialog);
}

void AppImageUpdater::backgroundCheckForUpdate()
{
    OCUpdater::backgroundCheckForUpdate();
}

#include "appimageupdater.moc"
