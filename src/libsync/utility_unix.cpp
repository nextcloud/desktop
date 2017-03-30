/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    #include <QStandardPaths>
#endif

namespace OCC {

static void setupFavLink_private(const QString &folder) {
    // Nautilus: add to ~/.gtk-bookmarks
    QFile gtkBookmarks(QDir::homePath()+QLatin1String("/.gtk-bookmarks"));
    QByteArray folderUrl = "file://" + folder.toUtf8();
    if (gtkBookmarks.open(QFile::ReadWrite)) {
        QByteArray places = gtkBookmarks.readAll();
        if (!places.contains(folderUrl)) {
            places += folderUrl;
            gtkBookmarks.reset();
            gtkBookmarks.write(places + '\n');
        }
    }
}

// returns the autostart directory the linux way
// and respects the XDG_CONFIG_HOME env variable
QString getUserAutostartDir_private()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
#else
    QString config = QFile::decodeName(qgetenv("XDG_CONFIG_HOME"));

    if (config.isEmpty()) {
        config = QDir::homePath()+QLatin1String("/.config");
    }
#endif
    config += QLatin1String("/autostart/");
    return config;
}

bool hasLaunchOnStartup_private(const QString &appName)
{
    QString desktopFileLocation = getUserAutostartDir_private()+appName+QLatin1String(".desktop");
    return QFile::exists(desktopFileLocation);
}

void setLaunchOnStartup_private(const QString &appName, const QString& guiName, bool enable)
{
    QString userAutoStartPath = getUserAutostartDir_private();
    QString desktopFileLocation = userAutoStartPath+appName+QLatin1String(".desktop");
    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            qCWarning(lcUtility) << "Could not create autostart folder";
            return;
        }
        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            qCWarning(lcUtility) << "Could not write auto start entry" << desktopFileLocation;
            return;
        }
        QTextStream ts(&iniFile);
        ts.setCodec("UTF-8");
        ts << QLatin1String("[Desktop Entry]") << endl
           << QLatin1String("Name=") << guiName << endl
           << QLatin1String("GenericName=") << QLatin1String("File Synchronizer") << endl
           << QLatin1String("Exec=") << QCoreApplication::applicationFilePath() << endl
           << QLatin1String("Terminal=") << "false" << endl
           << QLatin1String("Icon=") << appName.toLower() << endl // always use lowercase for icons
           << QLatin1String("Categories=") << QLatin1String("Network") << endl
           << QLatin1String("Type=") << QLatin1String("Application") << endl
           << QLatin1String("StartupNotify=") << "false" << endl
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true" << endl
            ;
    } else {
        if (!QFile::remove(desktopFileLocation)) {
            qCWarning(lcUtility) << "Could not remove autostart desktop file";
        }
    }
}

static inline bool hasDarkSystray_private()
{
    return true;
}

} // namespace OCC
