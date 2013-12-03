/*
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
#include <signal.h>

#include "mirall/application.h"
#include "mirall/theme.h"
#include "mirall/utility.h"

#include <QMessageBox>
#include <QTimer>

void warnSystray()
{
    QMessageBox::critical(0, qApp->translate("main.cpp", "System Tray not available"),
                          qApp->translate("main.cpp", "%1 requires on a working system tray. "
                                          "If you are running XFCE, please follow "
                                          "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                          "Otherwise, please install a system tray application such as 'trayer' and try again.")
                          .arg(Mirall::Theme::instance()->appNameGUI()));
}

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(mirall);

    Mirall::Application app(argc, argv);
    app.initialize();
#ifndef Q_OS_WIN
    signal(SIGPIPE, SIG_IGN);
#endif
    if( app.giveHelp() ) {
        app.showHelp();
        return 0;
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
                Mirall::Utility::sleep(1);
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

