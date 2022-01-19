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

#include <QTimer>
#include <appimage/update.h>
#include <chrono>

#include "appimageupdater.h"
#include "common/version.h"

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
    if (info.version().isEmpty() || Version::versionWithBuildNumber() >= QVersionNumber::fromString(info.version())) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
        return;
    }

    const auto AppImageUpdaterShim = AppImageUpdaterShim::makeInstance(info.downloadUrl(), this);

    if (AppImageUpdaterShim == nullptr) {
        setDownloadState(DownloadFailed);
        return;
    }

    if (!AppImageUpdaterShim->isUpdateAvailable()) {
        qCCritical(lcUpdater) << "Update server reported that update is available, but AppImageUpdate disagrees, aborting";
        setDownloadState(DownloadFailed);
        return;
    }

    // binding AppImageUpdaterShim shared pointer to finished callback makes sure the updater is cleaned up when it's done
    connect(AppImageUpdaterShim, &AppImageUpdaterShim::finished, this, [this](bool succeeded) {
        if (succeeded) {
            qCInfo(lcUpdater) << "AppImage update complete";
            setDownloadState(DownloadComplete);
        } else {
            qCInfo(lcUpdater) << "AppImage update failed";
            setDownloadState(DownloadFailed);
        }
    });

    setDownloadState(Downloading);
    AppImageUpdaterShim->startUpdateInBackground();
}

void AppImageUpdater::backgroundCheckForUpdate()
{
    OCUpdater::backgroundCheckForUpdate();
}

#include "appimageupdater.moc"
