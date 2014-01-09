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

#include <neon/ne_socket.h>

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

struct CmdOptions {
    QString source_dir;
    QString target_url;
    QString config_directory;
    QString proxy;
};

void help()
{
    std::cout << "owncloudcmd - command line ownCloud client tool." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: owncloudcmd <sourcedir> <owncloudurl>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --confdir = configdir: Read config from there." << std::endl;
    std::cout << "  --httpproxy = proxy:   Specify a http proxy to use." << std::endl;
    std::cout << "                         Proxy is http://server:port" << std::endl;
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
        } else if( option == "--httpproxy" && !it.peekNext().startsWith("-")) {
            options->proxy = it.next();
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

    CmdOptions options;

    parseOptions( app.arguments(), &options );

    CSYNC *_csync_ctx;
    if( csync_create( &_csync_ctx, options.source_dir.toUtf8(),
                      options.target_url.toUtf8()) < 0 ) {
        qFatal("Unable to create csync-context!");
        return EXIT_FAILURE;
    }
    int rc = ne_sock_init();
    if (rc < 0) {
        qFatal("ne_sock_init failed!");
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
    if( !options.proxy.isNull() ) {
        QString host;
        int port = 0;
        bool ok;

        QStringList pList = options.proxy.split(':');
        if(pList.count() == 3) {
            // http: //192.168.178.23 : 8080
            //  0            1            2
            host = pList.at(1);
            if( host.startsWith("//") ) host.remove(0, 2);

            port = pList.at(2).toInt(&ok);

            if( !host.isNull() ) {
                csync_set_module_property(_csync_ctx, "proxy_type", (void*) "HttpProxy");
                csync_set_module_property(_csync_ctx, "proxy_host", host.toUtf8().data());
                if( ok && port ) {
                    csync_set_module_property(_csync_ctx, "proxy_port", (void*) &port);
                }
            }
        }
    }

    SyncJournalDb db(options.source_dir);
    CSyncThread csyncthread(_csync_ctx, options.source_dir, QUrl(options.target_url).path(), &db);
    QObject::connect(&csyncthread, SIGNAL(finished()), &app, SLOT(quit()));
    csyncthread.startSync();

    app.exec();

    csync_destroy(_csync_ctx);

    ne_sock_exit();

    return 0;
}
