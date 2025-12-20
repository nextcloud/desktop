/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef APPIMAGEUPDATER_H
#define APPIMAGEUPDATER_H

#include "updater/ocupdater.h"

#include <QTemporaryFile>

namespace OCC {

/**
 * @brief Linux AppImage Updater
 * @ingroup gui
 *
 * Downloads the new AppImage and replaces the current one on restart.
 * Only active when running inside an AppImage (detected via APPIMAGE env var).
 */
class AppImageUpdater : public OCUpdater
{
    Q_OBJECT
public:
    explicit AppImageUpdater(const QUrl &url);
    bool handleStartup() override;

    /**
     * @brief Check if the application is running as an AppImage
     * @return true if APPIMAGE environment variable is set and points to existing file
     */
    static bool isRunningAppImage();

    /**
     * @brief Get the path to the currently running AppImage
     * @return Path from APPIMAGE environment variable, or empty string if not an AppImage
     */
    static QString currentAppImagePath();

public slots:
    void slotStartInstaller() override;

private slots:
    void slotDownloadFinished();
    void slotWriteFile();

private:
    void wipeUpdateData();
    void showUpdateErrorDialog(const QString &targetVersion);
    void versionInfoArrived(const UpdateInfo &info) override;
    bool canWriteToAppImageLocation() const;

    QScopedPointer<QTemporaryFile> _file;
    QString _targetFile;
};

} // namespace OCC

#endif // APPIMAGEUPDATER_H
