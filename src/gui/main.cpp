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

#include "application.h"
#include "application_p.h"
#include "common/utility.h"
#include "guiutility.h"
#include "platform.h"
#include "resources/loadresources.h"
#include "theme.h"

#ifdef WITH_AUTO_UPDATER
#include "updater/updater.h"
#endif

#include <QTimer>
#include <QMessageBox>

using namespace std::chrono_literals;

using namespace OCC;

void warnSystray()
{
    QMessageBox::critical(nullptr, qApp->translate("main.cpp", "System Tray not available"),
        qApp->translate("main.cpp", "%1 requires on a working system tray. "
                                    "If you are running XFCE, please follow "
                                    "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                    "Otherwise, please install a system tray application such as 'trayer' and try again.")
            .arg(Theme::instance()->appNameGUI()));
}

int main(int argc, char **argv)
{
    // load the resources
    const OCC::ResourcesLoader resource;
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    // Create a `Platform` instance so it can set-up/tear-down stuff for us, and do any
    // initialisation that needs to be done before creating a QApplication
    auto platform = Platform::create();

    // Create the (Q)Application instance:
    OCC::Application app(argc, argv, platform.get());

#ifdef Q_OS_WIN
    // TODO: 2.11 move to platform class
    Utility::startShutdownWatcher();
#endif

#ifdef WITH_AUTO_UPDATER
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    Updater *updater = Updater::instance();
    if (updater && updater->handleStartup()) {
        return 1;
    }
#endif

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
            if (!app.sendMessage(ApplicationPrivate::msgParseOptionsC() + msg))
                return -1;
        }
        return 0;
    }

    // We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray
    // (issue #4693)
    if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5")
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // If the systemtray is not there, we will wait one second for it to maybe start
            // (eg boot time) then we show the settings dialog if there is still no systemtray.
            // On XFCE however, we show a message box with explainaition how to install a systemtray.
            qCInfo(lcApplication) << "System tray is not available, waiting...";
            Utility::sleep(1);

            auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
            if (desktopSession.isEmpty()) {
                desktopSession = qgetenv("DESKTOP_SESSION").toLower();
            }
            if (desktopSession == "xfce") {
                int attempts = 0;
                while (!QSystemTrayIcon::isSystemTrayAvailable()) {
                    attempts++;
                    if (attempts >= 30) {
                        qCWarning(lcApplication) << "System tray unavailable (xfce)";
                        warnSystray();
                        break;
                    }
                    Utility::sleep(1);
                }
            }

            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                app.tryTrayAgain();
            } else if (desktopSession != "ubuntu") {
                qCInfo(lcApplication) << "System tray still not available, showing window and trying again later";
                QTimer::singleShot(10s, &app, &Application::tryTrayAgain);
            } else {
                qCInfo(lcApplication) << "System tray still not available, but assuming it's fine on 'ubuntu' desktop";
            }
        }
    }

    return app.exec();
}
