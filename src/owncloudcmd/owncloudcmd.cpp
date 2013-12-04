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
#include <QFile>
#include <qdebug.h>

#include "csyncthread.h"
#include <syncjournaldb.h>
#include "logger.h"
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

struct CmdOptions {
    QString source_dir;
    QString target_url;
    QString config_directory;
};

void help()
{
    std::cout << "owncloudcmd - command line ownCloud client tool." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: owncloudcmd <sourcedir> <owncloudurl>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --confdir = configdir: Read config from there." << std::endl;
    std::cout << "" << std::endl;
    exit(1);

}

void parseOptions( const QStringList& app_args, CmdOptions *options )
{
    QStringList args(app_args);

    if( args.count() < 3 ) {
        help();
    }

    options->target_url = args.takeLast();
    options->source_dir = args.takeLast();
    if( !QFile::exists( options->source_dir )) {
        std::cerr << "Source dir does not exists.";
        exit(1);
    }

    QStringListIterator it(args);
    // skip file name;
    if (it.hasNext()) it.next();

    while(it.hasNext()) {
        const QString option = it.next();

        if( option == "--confdir" && !it.peekNext().startsWith("-") ) {
            options->config_directory = it.next();
        } else {
            help();
        }
    }

    if( options->target_url.isEmpty() || options->source_dir.isEmpty() ) {
        help();
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    ProxyInfo proxyInfo;
    CmdOptions options;

    parseOptions( app.arguments(), &options );

    CSYNC *_csync_ctx;
    if( csync_create( &_csync_ctx, options.source_dir.toUtf8(),
                      options.target_url.toUtf8()) < 0 ) {
        qFatal("Unable to create csync-context!");
        return EXIT_FAILURE;
    }

    csync_set_log_level(11);
    csync_enable_conflictcopys(_csync_ctx);
    Logger::instance()->setLogFile("-");

    csync_set_auth_callback( _csync_ctx, getauth );

    if( csync_init( _csync_ctx ) < 0 ) {
        qFatal("Could not initialize csync!");
        return EXIT_FAILURE;
        _csync_ctx = 0;
    }

    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);

    SyncJournalDb db(options.source_dir);
    CSyncThread csyncthread(_csync_ctx, options.source_dir, QUrl(options.target_url).path(), &db);
    QObject::connect(&csyncthread, SIGNAL(finished()), &app, SLOT(quit()));
    csyncthread.startSync();

    app.exec();

    csync_destroy(_csync_ctx);

    return 0;
}
