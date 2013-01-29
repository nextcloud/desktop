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

#include "mirall/application.h"

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(mirall);

    Mirall::Application app(argc, argv);
    app.initialize();
   
    // if the application is already running, notify it.
    if( app.isRunning() ) {
        QStringList args = app.arguments();
        if ( args.size() > 1 && ! app.giveHelp() ) {
            QString msg = args.join( QLatin1String("|") );
            if( ! app.sendMessage( msg ) )
                return -1;
        }
        return 0;
    }
    // if help requested, show on command line and exit.
    if( ! app.giveHelp() ) {
        return app.exec();
    } else {
        app.showHelp();
    }
}

