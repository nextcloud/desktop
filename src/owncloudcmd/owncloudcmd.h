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

#ifndef OWNCLOUDCMD_H
#define OWNCLOUDCMD_H

#include <QObject>

#include "csync.h"

#include "mirall/connectionvalidator.h"
#include "mirall/clientproxy.h"
#include "mirall/account.h"

using namespace Mirall;

struct CmdOptions {
    QString source_dir;
    QString target_url;
    QString config_directory;
    QString proxy;
    bool silent;
    bool trustSSL;
};

class OwncloudCmd : public QObject {
    Q_OBJECT
public:
    OwncloudCmd(CmdOptions options);
    bool runSync();
    void destroy();

public slots:
    void slotConnectionValidatorResult(ConnectionValidator::Status stat);
    void transmissionProgressSlot();

signals:
    void finished();

private:
    CmdOptions _options;
    ConnectionValidator *_conValidator;
    CSYNC *_csync_ctx;
    Account *_account;
    ClientProxy _clientProxy;
    QString _folder;
};

#endif
