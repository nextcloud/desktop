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

#include "mirall/application.h"
#include "mirall/theme.h"
#include "mirall/utility.h"
#include "mirall/cocoainitializer.h"

#include "updater/updater.h"

#include <QTimer>
#include <QMessageBox>

using namespace Mirall;

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
    Q_INIT_RESOURCE(mirall);

#ifdef Q_OS_MAC
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif
    Mirall::Application app(argc, argv);
#ifndef Q_OS_WIN
    signal(SIGPIPE, SIG_IGN);
#endif
    if( app.giveHelp() ) {
        app.showHelp();
        return 0;
    }

    // check a environment variable for core dumps
#ifdef Q_OS_UNIX
    if( !qgetenv("OWNCLOUD_CORE_DUMP").isEmpty() ) {
        struct rlimit core_limit;
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
            fprintf(stderr, "Unable to set core dump limit\n");
        } else {
            qDebug() << "Core dumps enabled";
        }
    }
#endif
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    if (Updater::instance()->handleStartup()) {
        return true;
    }

    // if the application is already running, notify it.
    if( app.isRunning() ) {
        QStringList args = app.arguments();
        if ( args.size() > 1 && ! app.giveHelp() ) {
            QString msg = args.join( QLatin1String("|") );
            if( ! app.sendMessage( msg ) )
                return -1;
        }
        return 0;
    } else {
        int attempts = 0;
        forever {
            if (!QSystemTrayIcon::isSystemTrayAvailable() && qgetenv("DESKTOP_SESSION") != "ubuntu") {
                Utility::sleep(1);
                attempts++;
                if (attempts < 30) continue;
            } else {
                break;
            }
            warnSystray();
            break;
        }
    }
    return app.exec();
}

