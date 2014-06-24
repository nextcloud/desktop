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

#include "mirall/syncengine.h"
#include "mirall/syncjournaldb.h"
#include "mirall/logger.h"

#include "creds/httpcredentials.h"
#include "owncloudcmd.h"
#include "simplesslerrorhandler.h"


OwncloudCmd::OwncloudCmd(CmdOptions options)
 : QObject(), _options(options)
{

}

void OwncloudCmd::slotConnectionValidatorResult(ConnectionValidator::Status stat)
{
    // call csync_create even if the connect fails, since csync_destroy is
    // called later in any case, and that needs a proper initialized csync_ctx.
    if( csync_create( &_csync_ctx, _options.source_dir.toUtf8(),
                      _options.target_url.toUtf8()) < 0 ) {
        qCritical("Unable to create csync-context!");
        emit( finished() );
        return;
    }

    if( stat != ConnectionValidator::Connected ) {
        qCritical("Connection cound not be established!");
        emit( finished() );
        return;

    }

    int rc = ne_sock_init();
    if (rc < 0) {
        qCritical("ne_sock_init failed!");
        emit( finished() );
        return;
    }

    csync_set_userdata(_csync_ctx, &_options);

    if( csync_init( _csync_ctx ) < 0 ) {
        qCritical("Could not initialize csync!");
        _csync_ctx = 0;
        emit( finished() );
        return;
    }

    csync_set_module_property(_csync_ctx, "csync_context", _csync_ctx);
    if( !_options.proxy.isNull() ) {
        QString host;
        int port = 0;
        bool ok;
        // Set as default and let overwrite later
        csync_set_module_property(_csync_ctx, "proxy_type", (void*) "NoProxy");

        QStringList pList = _options.proxy.split(':');
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
        _clientProxy.setupQtProxyFromConfig();
        QString url( _options.target_url );
        if( url.startsWith("owncloud")) {
            url.remove(0, 8);
            url = QString("http%1").arg(url);
        }
        _clientProxy.setCSyncProxy(QUrl(url), _csync_ctx);
    }

    SyncJournalDb *db = new SyncJournalDb(_options.source_dir);
    SyncEngine *engine = new SyncEngine(_csync_ctx, _options.source_dir, QUrl(_options.target_url).path(), _folder, db);
    connect( engine, SIGNAL(finished()), this, SIGNAL(finished()) );
    QObject::connect(engine, SIGNAL(transmissionProgress(Progress::Info)), this, SLOT(transmissionProgressSlot()));

    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(engine, "startSync", Qt::QueuedConnection);

}

bool OwncloudCmd::runSync()
{
    QUrl url(_options.target_url.toUtf8());

    _account = new Account;

    // Find the folder and the original owncloud url
    QStringList splitted = url.path().split(_account->davPath());
    url.setPath(splitted.value(0));
    url.setScheme(url.scheme().replace("owncloud", "http"));
    _folder = splitted.value(1);

    SimpleSslErrorHandler *sslErrorHandler = new SimpleSslErrorHandler;

    csync_set_log_level(_options.silent ? 1 : 11);
    Logger::instance()->setLogFile("-");

    if( url.userInfo().isEmpty() ) {
        // If there was no credentials coming in commandline url
        // than restore the credentials from a configured client
        delete _account;
        _account = Account::restore();
    } else {
        // set the commandline credentials
        _account->setCredentials(new HttpCredentials(url.userName(), url.password()));
    }

    if (_account) {
        _account->setUrl(url);
        _account->setSslErrorHandler(sslErrorHandler);
        AccountManager::instance()->setAccount(_account);

        _conValidator = new ConnectionValidator(_account);
        connect( _conValidator, SIGNAL(connectionResult(ConnectionValidator::Status)),
                 this, SLOT(slotConnectionValidatorResult(ConnectionValidator::Status)) );
        _conValidator->checkConnection();
    } else {
        // no account found
        return false;
    }

    return true;
}

void OwncloudCmd::destroy()
{
    csync_destroy(_csync_ctx);
}

void OwncloudCmd::transmissionProgressSlot()
{
    // do something nice here.
}


void help()
{
    std::cout << "owncloudcmd - command line ownCloud client tool." << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: owncloudcmd <sourcedir> <owncloudurl>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "A proxy can either be set manually using --httpproxy or it" << std::endl;
    std::cout << "uses the setting from a configured sync client." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --silent               Don't be so verbose" << std::endl;
    std::cout << "  --confdir = configdir: Read config from there." << std::endl;
    std::cout << "  --httpproxy = proxy:   Specify a http proxy to use." << std::endl;
    std::cout << "                         Proxy is http://server:port" << std::endl;
    std::cout << "  --trust                Trust the SSL certification." << std::endl;
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

    parseOptions( app.arguments(), &options );


    OwncloudCmd owncloudCmd(options);

    QObject::connect(&owncloudCmd, SIGNAL(finished()), &app, SLOT(quit()));
    if( !owncloudCmd.runSync() ) {
        return 1;
    }

    app.exec();

    owncloudCmd.destroy();

    ne_sock_exit();

    return 0;
}

