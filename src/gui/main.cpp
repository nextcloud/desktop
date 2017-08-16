/*
 *
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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
#include <QtGlobal>

#include <signal.h>

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "application.h"
#include "theme.h"
#include "common/utility.h"
#include "cocoainitializer.h"

#include "updater/updater.h"

#include <QTimer>
#include <QMessageBox>

using namespace OCC;

void warnSystray()
{
    QMessageBox::critical(0, qApp->translate("main.cpp", "System Tray not available"),
        qApp->translate("main.cpp", "%1 requires on a working system tray. "
                                    "If you are running XFCE, please follow "
                                    "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                    "Otherwise, please install a system tray application such as 'trayer' and try again.")
            .arg(Theme::instance()->appNameGUI()));
}

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(client);

#ifdef Q_OS_WIN
// If the font size ratio is set on Windows, we need to
// enable the auto pixelRatio in Qt since we don't
// want to use sizes relative to the font size everywhere.
// This is automatic on OS X, but opt-in on Windows and Linux
// https://doc-snapshots.qt.io/qt5-5.6/highdpi.html#qt-support
// We do not define it on linux so the behaviour is kept the same
// as other Qt apps in the desktop environment. (which may or may
// not set this envoronment variable)
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
#else
    qputenv("QT_DEVICE_PIXEL_RATIO", "auto"); // See #4840, #4994
#endif
#endif // !Q_OS_WIN

#ifdef Q_OS_MAC
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif
    OCC::Application app(argc, argv);

#ifdef Q_OS_WIN
    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (app.devicePixelRatio() > 1)
        QApplication::setStyle(QStringLiteral("fusion"));
#endif // Q_OS_WIN

#ifndef Q_OS_WIN
    signal(SIGPIPE, SIG_IGN);
#endif
    if (app.giveHelp()) {
        app.showHelp();
        return 0;
    }
    if (app.versionOnly()) {
        app.showVersion();
        return 0;
    }

// check a environment variable for core dumps
#ifdef Q_OS_UNIX
    if (!qgetenv("OWNCLOUD_CORE_DUMP").isEmpty()) {
        struct rlimit core_limit;
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
            fprintf(stderr, "Unable to set core dump limit\n");
        } else {
            qCInfo(lcApplication) << "Core dumps enabled";
        }
    }
#endif
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    Updater *updater = Updater::instance();
    if (updater && updater->handleStartup()) {
        return 1;
    }

    // if the application is already running, notify it.
    if (app.isRunning()) {
        qCInfo(lcApplication) << "Already running, exiting...";
        if (app.isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            qCInfo(lcApplication) << "Session was restored, don't notify app!";
            return -1;
        }

        QStringList args = app.arguments();
        if (args.size() > 1) {
            QString msg = args.join(QLatin1String("|"));
            if (!app.sendMessage(QLatin1String("MSG_PARSEOPTIONS:") + msg))
                return -1;
        }
        if (!app.sendMessage(QLatin1String("MSG_SHOWSETTINGS"))) {
            return -1;
        }
        return 0;
    }
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5")
// We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray
// (issue #4693)
#endif
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // If the systemtray is not there, we will wait one second for it to maybe start
            // (eg boot time) then we show the settings dialog if there is still no systemtray.
            // On XFCE however, we show a message box with explainaition how to install a systemtray.
            Utility::sleep(1);
            auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
            if (desktopSession.isEmpty()) {
                desktopSession = qgetenv("DESKTOP_SESSION").toLower();
            }
            if (desktopSession == "xfce") {
                int attempts = 0;
                forever {
                    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                        Utility::sleep(1);
                        attempts++;
                        if (attempts < 30)
                            continue;
                    } else {
                        break;
                    }
                    warnSystray();
                }
            }
            if (!QSystemTrayIcon::isSystemTrayAvailable() && desktopSession != "ubuntu") {
                app.showSettingsDialog();
            }
        }
    }

    return app.exec();
}
