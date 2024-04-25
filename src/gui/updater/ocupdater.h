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

#pragma once
#include "gui/owncloudguilib.h"

#include "application.h"
#include "updater/updatedownloadeddialog.h"
#include "updater/updateinfo.h"
#include "updater/updater.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>


class QNetworkAccessManager;
class QNetworkReply;

namespace OCC {

/**
 * @brief Schedule update checks every couple of hours if the client runs.
 * @ingroup gui
 *
 * This class schedules regular update checks. It also checks the config
 * if update checks are wanted at all.
 *
 * To reflect that all platforms have their own update scheme, a little
 * complex class design was set up:
 *
 * For Windows and Linux, the updaters are inherited from OCUpdater, while
 * the MacOSX SparkleUpdater directly uses the class Updater. On windows,
 * NSISUpdater starts the update if a new version of the client is available.
 * On MacOSX, the sparkle framework handles the installation of the new
 * version. On Linux, the update capabilities of the underlying linux distro
 * are relied on, and thus the PassiveUpdateNotifier just shows a notification
 * if there is a new version once at every start of the application.
 *
 * Simple class diagram of the updater:
 *
 *           +---------------------------+
 *     +-----+   UpdaterScheduler        +-----+------------------+
 *     |     +------------+--------------+     |                  |
 *     v                  v                    v                  v
 * +------------+ +---------------------+ +----------------+ +-----------------+
 * |NSISUpdater | |PassiveUpdateNotifier| | SparkleUpdater | | AppImageUpdater |
 * +-+----------+ +---+-----------------+ +-----+----------+ +-----------------+
 *   |                |                         |                 |
 *   |                v      +------------------+                 |
 *   |   +---------------+   v                                    |
 *   +-->|   OCUpdater   +------+                                 |
 *       +--------+------+      |<--------------------------------+
 *                |   Updater   |
 *                +-------------+
 */

class OWNCLOUDGUI_EXPORT UpdaterScheduler : public QObject
{
    Q_OBJECT
public:
    // must pass app explicitly (as we're instantiated from Application's ctor, we cannot use ocApp())
    explicit UpdaterScheduler(Application *app, QObject *parent = nullptr);

Q_SIGNALS:
    /**
     * Show an update-related status message on the UI.
     * @param title message title
     * @param msg message content
     */
    void updaterAnnouncement(const QString &title, const QString &msg);

private Q_SLOTS:
    void slotTimerFired();

private:
    QTimer _updateCheckTimer; /** Timer for the regular update check. */

    // make sure we are going to show only one of them at once
    QPointer<UpdateDownloadedDialog> _updateDownloadedDialog = nullptr;
};

/**
 * @brief Class that uses an ownCloud proprietary XML format to fetch update information
 * @ingroup gui
 */
class OWNCLOUDGUI_EXPORT OCUpdater : public Updater
{
    Q_OBJECT
public:
    enum DownloadState {
        Unknown = 0,
        CheckingServer,
        UpToDate,
        Downloading,
        DownloadComplete,
        DownloadFailed,
        DownloadTimedOut,
        UpdateOnlyAvailableThroughSystem
    };
    Q_ENUM(DownloadState);

    explicit OCUpdater(const QUrl &url);

    void setUpdateUrl(const QUrl &url);

    void checkForUpdate() override;

    QString statusString() const;
    DownloadState downloadState() const;
    void setDownloadState(DownloadState state);

    QString availableVersionString() const;

Q_SIGNALS:
    void downloadStateChanged();

    // it is up to the scheduler how to display either of these
    // while in one case a system native notification may be adequate, the other may require a sophisticated dialog with action buttons
    void updateAvailableThroughSystem();
    void updateDownloaded();

    /**
     * Schedule retry of update check in the future. For use when an update failed previously due to a (temporary) problem which might be resolved in a reasonable amount of time.
     */
    void retryUpdateCheckLater();

protected Q_SLOTS:
    void backgroundCheckForUpdate() override;
    void slotOpenUpdateUrl();

private Q_SLOTS:
    void slotVersionInfoArrived();
    void slotTimedOut();

protected:
    static QVersionNumber previouslySkippedVersion();
    static void setPreviouslySkippedVersion(const QVersionNumber &previouslySkippedVersion);
    static void setPreviouslySkippedVersion(const QString &previouslySkippedVersionString);

    virtual void versionInfoArrived(const UpdateInfo &info) = 0;
    bool updateSucceeded() const;
    QNetworkAccessManager *qnam() const { return _accessManager; }
    UpdateInfo updateInfo() const { return _updateInfo; }

private:
    QUrl _updateUrl;
    DownloadState _state;
    QNetworkAccessManager *_accessManager;
    QTimer *_timeoutWatchdog; /** Timer to guard the timeout of an individual network request */
    UpdateInfo _updateInfo;
};

/**
 * @brief Windows Updater Using NSIS
 * @ingroup gui
 */
class OWNCLOUDGUI_EXPORT WindowsUpdater : public OCUpdater
{
    Q_OBJECT
public:
    explicit WindowsUpdater(const QUrl &url);
    bool handleStartup() override;

    void startInstallerAndQuit();

private Q_SLOTS:
    void slotSetPreviouslySkippedVersion();
    void slotDownloadFinished();
    void slotWriteFile();

private:
    void wipeUpdateData();
    void showNewVersionAvailableDialog(const UpdateInfo &info);
    void showUpdateErrorDialog(const QString &targetVersion);
    void versionInfoArrived(const UpdateInfo &info) override;
    QScopedPointer<QTemporaryFile> _file;
    QString _targetFile;

    friend class TestUpdater;
};

/**
 *  @brief Updater that only implements notification for use in settings
 *
 *  The implementation does not show popups
 *
 *  @ingroup gui
 */
class PassiveUpdateNotifier : public OCUpdater
{
    Q_OBJECT
public:
    explicit PassiveUpdateNotifier(const QUrl &url);
    bool handleStartup() override { return false; }
    void backgroundCheckForUpdate() override;

private:
    void versionInfoArrived(const UpdateInfo &info) override;
    const QDateTime _initialAppMTime;
};
}
