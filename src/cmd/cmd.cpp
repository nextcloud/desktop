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

#include "account.h"
#include "clientproxy.h"
#include "creds/httpcredentials.h"
#include "csync.h"
#include "simplesslerrorhandler.h"
#include "syncengine.h"
#include "syncjournaldb.h"
#include "config.h"

#include "cmd.h"

using namespace Mirall;

struct CmdOptions {
    QString source_dir;
    QString target_url;
    QString config_directory;
    QString proxy;
    bool silent;
    bool trustSSL;
    QString exclude;
};

// we can't use csync_set_userdata because the SyncEngine sets it already.
// So we have to use a global variable
CmdOptions *opts = 0;

int getauth(const char* prompt, char* buf, size_t len, int a, int b, void *userdata)
{
    Q_UNUSED(a) Q_UNUSED(b) Q_UNUSED(userdata)

    std::cout << "** Authentication required: \n" << prompt << std::endl;
    std::string s;
    if(opts && opts->trustSSL) {
        s = "yes";
    } else {
        std::getline(std::cin, s);
    }
    strncpy( buf, s.c_str(), len );
    return 0;
}

void help()
{
    const char* appName = APPLICATION_EXECUTABLE "cmd";
    std::cout << appName << " - command line " APPLICATION_NAME " client tool." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: " << appName << " <source_dir> <server_url>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "A proxy can either be set manually using --httpproxy." << std::endl;
    std::cout << "Otherwise, the setting from a configured sync client will be used." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --silent               Don't be so verbose" << std::endl;
    std::cout << "  --confdir = configdir: Read config from there." << std::endl;
    std::cout << "  --httpproxy = proxy:   Specify a http proxy to use." << std::endl;
    std::cout << "                         Proxy is http://server:port" << std::endl;
    std::cout << "  --trust                Trust the SSL certification." << std::endl;
    std::cout << "  --exclude [file]       exclude list file" << std::endl;
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
    // check if the remote.php/webdav tail was added and append if not.
    if( !options->target_url.contains("remote.php/webdav")) {
        if(!options->target_url.endsWith("/")) {
            options->target_url.append("/");
        }
        options->target_url.append("remote.php/webdav/");
    }
    if (options->target_url.startsWith("http"))
        options->target_url.replace(0, 4, "owncloud");
    options->source_dir = args.takeLast();
    if( !QFile::exists( options->source_dir )) {
        std::cerr << "Source dir does not exists." << std::endl;
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
        } else if( option == "--silent") {
            options->silent = true;
        } else if( option == "--trust") {
            options->trustSSL = true;
        } else if( option == "--exclude" && !it.peekNext().startsWith("-") ) {
                options->exclude = it.next();
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
    options.silent = false;
    options.trustSSL = false;
    ClientProxy clientProxy;

    parseOptions( app.arguments(), &options );


    QUrl url(options.target_url.toUtf8());
    Account account;

    // Find the folder and the original owncloud url
    QStringList splitted = url.path().split(account.davPath());
    url.setPath(splitted.value(0));
    url.setScheme(url.scheme().replace("owncloud", "http"));
    QString folder = splitted.value(1);

    SimpleSslErrorHandler *sslErrorHandler = new SimpleSslErrorHandler;

    account.setUrl(url);
    account.setCredentials(new HttpCredentials(url.userName(), url.password()));
    account.setSslErrorHandler(sslErrorHandler);
    AccountManager::instance()->setAccount(&account);

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

    csync_set_log_level(options.silent ? 1 : 11);

    opts = &options;
    csync_set_auth_callback( _csync_ctx, getauth );

    if( csync_init( _csync_ctx ) < 0 ) {
        qFatal("Could not initialize csync!");
        return EXIT_FAILURE;
    }

    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);
    if( !options.proxy.isNull() ) {
        QString host;
        int port = 0;
        bool ok;

        // Set as default and let overwrite later
        csync_set_module_property(_csync_ctx, "proxy_type", (void*) "NoProxy");

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
    } else {
        clientProxy.setupQtProxyFromConfig();
        QString url( options.target_url );
        if( url.startsWith("owncloud")) {
            url.remove(0, 8);
            url = QString("http%1").arg(url);
        }
        clientProxy.setCSyncProxy(QUrl(url), _csync_ctx);
    }

    if (!options.exclude.isEmpty()) {
        csync_add_exclude_list(_csync_ctx, options.exclude.toLocal8Bit());
    }

    Cmd cmd;
    SyncJournalDb db(options.source_dir);
    SyncEngine engine(_csync_ctx, options.source_dir, QUrl(options.target_url).path(), folder, &db);
    QObject::connect(&engine, SIGNAL(finished()), &app, SLOT(quit()));
    QObject::connect(&engine, SIGNAL(transmissionProgress(Progress::Info)), &cmd, SLOT(transmissionProgressSlot()));

    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(&engine, "startSync", Qt::QueuedConnection);

    app.exec();

    csync_destroy(_csync_ctx);

    ne_sock_exit();

    return 0;
}

