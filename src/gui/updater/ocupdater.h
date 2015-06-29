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

#ifndef OCUPDATER_H
#define OCUPDATER_H

#include <QObject>
#include <QUrl>
#include <QTemporaryFile>

#include "updater/updateinfo.h"
#include "updater/updater.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace OCC {

/**
 * @brief Class that uses an ownCloud propritary XML format to fetch update information
 * @ingroup gui
 */
class OCUpdater : public QObject, public Updater
{
    Q_OBJECT
public:
    enum DownloadState { Unknown = 0, CheckingServer, UpToDate,
                         Downloading, DownloadComplete,
                         DownloadFailed, DownloadTimedOut,
                         UpdateOnlyAvailableThroughSystem };
    explicit OCUpdater(const QUrl &url, QObject *parent = 0);

    bool performUpdate();

    void checkForUpdate() Q_DECL_OVERRIDE;
    void backgroundCheckForUpdate() Q_DECL_OVERRIDE;

    QString statusString() const;
    int downloadState() const;
    void setDownloadState(DownloadState state);

signals:
    void downloadStateChanged();

public slots:
    void slotStartInstaller();

private slots:
    void slotOpenUpdateUrl();
    void slotVersionInfoArrived();
    void slotTimedOut();

protected:
    virtual void versionInfoArrived(const UpdateInfo &info) = 0;
    bool updateSucceeded() const;
    QNetworkAccessManager* qnam() const { return _accessManager; }
    UpdateInfo updateInfo() const { return _updateInfo; }
private:
    QUrl _updateUrl;
    int _state;
    QNetworkAccessManager *_accessManager;
    QTimer *_timer;
    UpdateInfo _updateInfo;
};

/**
 * @brief Windows Updater Using NSIS
 * @ingroup gui
 */
class NSISUpdater : public OCUpdater {
    Q_OBJECT
public:
    enum UpdateState { NoUpdate = 0, UpdateAvailable, UpdateFailed };
    explicit NSISUpdater(const QUrl &url, QObject *parent = 0);
    bool handleStartup() Q_DECL_OVERRIDE;
private slots:
    void slotSetSeenVersion();
    void slotDownloadFinished();
    void slotWriteFile();
private:
    NSISUpdater::UpdateState updateStateOnStart();
    void showDialog(const UpdateInfo &info);
    void versionInfoArrived(const UpdateInfo &info) Q_DECL_OVERRIDE;
    QScopedPointer<QTemporaryFile> _file;
    QString _targetFile;
    bool _showFallbackMessage;

};

/**
 *  @brief Updater that only implements notification for use in settings
 *
 *  The implementation does how show popups
 *
 *  @ingroup gui
 */
class PassiveUpdateNotifier : public OCUpdater {
    Q_OBJECT
public:
    explicit PassiveUpdateNotifier(const QUrl &url, QObject *parent = 0);
    bool handleStartup() Q_DECL_OVERRIDE { return false; }

private:
    void versionInfoArrived(const UpdateInfo &info) Q_DECL_OVERRIDE;
};



}

#endif // OC_UPDATER
