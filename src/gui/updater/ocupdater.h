/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCUPDATER_H
#define OCUPDATER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QTemporaryFile>
#include <QTimer>

#include "updater/updateinfo.h"
#include "updater/updater.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace OCC {

using namespace Qt::StringLiterals;

constexpr auto updateAvailableKey = "Updater/updateAvailable"_L1;
constexpr auto updateTargetVersionKey = "Updater/updateTargetVersion"_L1;
constexpr auto updateTargetVersionStringKey = "Updater/updateTargetVersionString"_L1;
constexpr auto autoUpdateAttemptedKey = "Updater/autoUpdateAttempted"_L1;

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
 *     +-----+   UpdaterScheduler        +-----+
 *     |     +------------+--------------+     |
 *     v                  v                    v
 * +------------+ +---------------------+ +----------------+
 * |NSISUpdater | |PassiveUpdateNotifier| | SparkleUpdater |
 * +-+----------+ +---+-----------------+ +-----+----------+
 *   |                |                         |
 *   |                v      +------------------+
 *   |   +---------------+   v
 *   +-->|   OCUpdater   +------+
 *       +--------+------+      |
 *                |   Updater   |
 *                +-------------+
 */

class UpdaterScheduler : public QObject
{
    Q_OBJECT
public:
    UpdaterScheduler(QObject *parent);

signals:
    void updaterAnnouncement(const QString &title, const QString &msg, const QUrl &webUrl);
    void requestRestart();

private slots:
    void slotTimerFired();

private:
    QTimer _updateCheckTimer; /** Timer for the regular update check. */
};

/**
 * @brief Class that uses an ownCloud proprietary XML format to fetch update information
 * @ingroup gui
 */
class OCUpdater : public Updater
{
    Q_OBJECT
public:
    enum DownloadState { Unknown = 0,
        CheckingServer,
        UpToDate,
        Downloading,
        DownloadComplete,
        DownloadFailed,
        DownloadTimedOut,
        UpdateOnlyAvailableThroughSystem };

    enum UpdateStatusStringFormat {
        PlainText,
        Html,
    };
    explicit OCUpdater(const QUrl &url);

    void setUpdateUrl(const QUrl &url);

    bool performUpdate();

    void checkForUpdate() override;

    [[nodiscard]] QString statusString(UpdateStatusStringFormat format = PlainText) const;
    [[nodiscard]] int downloadState() const;
    void setDownloadState(DownloadState state);

signals:
    void downloadStateChanged();
    void newUpdateAvailable(const QString &header, const QString &message, const QUrl &webUrl);
    void requestRestart();

public slots:
    virtual void slotStartInstaller();

protected slots:
    void backgroundCheckForUpdate() override;
    void slotOpenUpdateUrl();

private slots:
    void slotVersionInfoArrived();
    void slotTimedOut();

protected:
    virtual void versionInfoArrived(const UpdateInfo &info) = 0;
    [[nodiscard]] bool updateSucceeded() const;
    [[nodiscard]] QNetworkAccessManager *qnam() const { return _accessManager; }
    [[nodiscard]] UpdateInfo updateInfo() const { return _updateInfo; }

private:
    QUrl _updateUrl;
    int _state = Unknown;
    QNetworkAccessManager *_accessManager;
    QTimer *_timeoutWatchdog; /** Timer to guard the timeout of an individual network request */
    UpdateInfo _updateInfo;
};

/**
 * @brief Windows Updater Using NSIS
 * @ingroup gui
 */
class NSISUpdater : public OCUpdater
{
    Q_OBJECT
public:
    explicit NSISUpdater(const QUrl &url);
    bool handleStartup() override;
    void slotStartInstaller() override;
private slots:
    void slotDownloadFinished();
    void slotWriteFile();

private:
    void wipeUpdateData();
    void showNoUrlDialog(const UpdateInfo &info);
    void showUpdateErrorDialog(const QString &targetVersion);
    void versionInfoArrived(const UpdateInfo &info) override;
    QScopedPointer<QTemporaryFile> _file;
    QString _targetFile;
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
    QByteArray _runningAppVersion;
};
}

#endif // OC_UPDATER
