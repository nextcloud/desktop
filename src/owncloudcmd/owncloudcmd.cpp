/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include <iostream>
#include <qcoreapplication.h>
#include <QStringList>
#include <QUrl>
#include <qdebug.h>

#include "csyncthread.h"
#include "csync.h"

using namespace Mirall;

int getauth(const char* prompt, char* buf, size_t len, int echo, int verify, void*)
{
    std::cout << "AUTH CALLBACK\n" << prompt << std::endl;
    std::string s;
    std::getline(std::cin, s);
    strncpy( buf, s.c_str(), len );
    return 0;
}

void help() {
    std::cout << "Usage: owncloudcmd [OPTION...] LOCAL REMOTE\n";
//                 "     --dry-run              This runs only update detection and reconcilation.\n"
//                  "    --proxy=<host:port>    Use an http proxy (ownCloud module only)\n"
}

struct ProxyInfo {
    const char *proxyType;
    const char *proxyHost;
    int         proxyPort;
    const char *proxyUser;
    const char *proxyPwd;

    ProxyInfo() {
        proxyType = 0;
        proxyHost = 0;
        proxyPort = 0;
        proxyUser = 0;
        proxyPwd = 0;
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);


    const char *source_dir = 0;
    const char *target_url = 0;
    const char *config_directory = 0;
    int verbosity = 0;

    ProxyInfo proxyInfo;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            //TODO: parse arguments
            std::cerr << "Argument not impemented " << argv[i] << std::endl;
        } else if (!source_dir) {
            source_dir =  argv[i];
        } else if (!target_url) {
            target_url =  argv[i];
        } else if (!config_directory) {
            config_directory = argv[i];
        } else {
            std::cerr << "Too many arguments" << std::endl;
            return 1;
        }
    }

    if (!source_dir || !target_url) {
        std::cerr << "Too few arguments" << std::endl;
        return 1;
    }


    CSYNC *_csync_ctx;
    if( csync_create( &_csync_ctx, source_dir, target_url) < 0 ) {
        qFatal("Unable to create csync-context!");
        return EXIT_FAILURE;
    }

    //csync_set_log_callback(   _csync_ctx, csyncLogCatcher );
    csync_set_log_verbosity(_csync_ctx, 11);
    csync_enable_conflictcopys(_csync_ctx);


    csync_set_auth_callback( _csync_ctx, getauth );

    if( csync_init( _csync_ctx ) < 0 ) {
        qFatal("Could not initialize csync!");
        return EXIT_FAILURE;
        _csync_ctx = 0;
    }

    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);


    CSyncThread csyncthread(_csync_ctx, QString::fromLocal8Bit(source_dir), QUrl(target_url).path());
    QObject::connect(&csyncthread, SIGNAL(finished()), &app, SLOT(quit()));
    csyncthread.startSync();

    app.exec();

    csync_destroy(_csync_ctx);
    return 0;
}
