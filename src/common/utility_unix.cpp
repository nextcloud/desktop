/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "utility.h"
#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtGlobal>
#include <QProcess>
#include <QString>
#include <QTextStream>

namespace OCC {

QString Utility::appImagePath()
{
    return qEnvironmentVariable("APPIMAGE");
}

bool Utility::isRunningInAppImage()
{
    const auto currentAppImagePath = appImagePath();
    return !currentAppImagePath.isEmpty() && QFile::exists(currentAppImagePath);
}

QVector<Utility::ProcessInfosForOpenFile> Utility::queryProcessInfosKeepingFileOpen(const QString &filePath)
{
    Q_UNUSED(filePath)
    return {};
}

void Utility::setupFavLink(const QString &folder)
{
    // Nautilus: add to ~/.config/gtk-3.0/bookmarks
    QFile gtkBookmarks(QDir::homePath() + QLatin1String("/.config/gtk-3.0/bookmarks"));
    const auto folderUrl = QUrl::fromLocalFile(folder).toEncoded();
    if (!gtkBookmarks.open(QFile::ReadWrite)) {
        qCWarning(lcUtility).nospace() << "failed to set up fav link"
            << " folder=" << folder
            << " error=" << gtkBookmarks.error()
            << " errorString=" << gtkBookmarks.errorString();
        return;
    }

    auto places = gtkBookmarks.readAll();
    if (places.contains(folderUrl)) {
        qCDebug(lcUtility).nospace() << "fav link already exists"
            << " folder=" << folder
            << " folderUrl=" << folderUrl;
        return;
    }

    places += folderUrl;
    gtkBookmarks.reset();
    gtkBookmarks.write(places + '\n');
}

void Utility::removeFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

// returns the autostart directory the linux way
// and respects the XDG_CONFIG_HOME env variable
static QString getUserAutostartDir()
{
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    config += QLatin1String("/autostart/");
    return config;
}

bool Utility::hasSystemLaunchOnStartup(const QString &appName)
{
    Q_UNUSED(appName)
    return false;
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    const QString desktopFileLocation = getUserAutostartDir() + appName + QLatin1String(".desktop");
    return QFile::exists(desktopFileLocation);
}

void Utility::migrateFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

void Utility::setupDesktopIni(const QString &folder, const QString localizedResourceName)
{
    Q_UNUSED(folder)
    Q_UNUSED(localizedResourceName)
}

QString Utility::syncFolderDisplayName(const QString &folder, const QString &displayName)
{
    Q_UNUSED(folder)
    Q_UNUSED(displayName)
    return {};
}

void Utility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable)
{
    const auto userAutoStartPath = getUserAutostartDir();
    const QString desktopFileLocation = userAutoStartPath + appName + QLatin1String(".desktop");
    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            qCWarning(lcUtility) << "Could not create autostart folder" << userAutoStartPath;
            return;
        }
        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            qCWarning(lcUtility) << "Could not write auto start entry" << desktopFileLocation;
            return;
        }
        // When running inside an AppImage, we need to set the path to the
        // AppImage instead of the path to the executable
        const auto currentAppImagePath = appImagePath();
        const auto runningInsideAppImage = isRunningInAppImage();
        const auto executablePath = runningInsideAppImage ? currentAppImagePath : QCoreApplication::applicationFilePath();

        QTextStream ts(&iniFile);
        ts << QLatin1String("[Desktop Entry]\n")
           << QLatin1String("Name=") << guiName << QLatin1Char('\n')
           << QLatin1String("GenericName=") << QLatin1String("File Synchronizer\n")
           << QLatin1String("Exec=\"") << executablePath << "\" --background\n"
           << QLatin1String("Terminal=") << "false\n"
           << QLatin1String("Icon=") << APPLICATION_ICON_NAME << QLatin1Char('\n')
           << QLatin1String("Categories=") << QLatin1String("Network\n")
           << QLatin1String("Type=") << QLatin1String("Application\n")
           << QLatin1String("StartupNotify=") << "false\n"
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true\n"
           << QLatin1String("X-GNOME-Autostart-Delay=10") << Qt::endl;
    } else {
        if (!QFile::remove(desktopFileLocation)) {
            qCWarning(lcUtility) << "Could not remove autostart desktop file";
        }
    }
}

bool Utility::hasDarkSystray()
{
    return true;
}

QString Utility::getCurrentUserName()
{
    return {};
}

void Utility::registerUriHandlerForLocalEditing()
{
    if (!isRunningInAppImage()) {
        // only register x-scheme-handler if running inside appImage
        return;
    }

    // mirall.desktop.in must have an x-scheme-handler mime type specified
    const QString desktopFileName = QLatin1String(LINUX_APPLICATION_ID) + QLatin1String(".desktop");
    QProcess process;
    const QStringList args = {
        QLatin1String("default"),
        desktopFileName,
        QStringLiteral("x-scheme-handler/%1").arg(QStringLiteral(APPLICATION_URI_HANDLER_SCHEME))
    };
    process.start(QStringLiteral("xdg-mime"), args, QIODevice::ReadOnly);
    process.waitForFinished();
}

} // namespace OCC
