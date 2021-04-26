/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#pragma once
#include <QObject>
#include "socketapi.h"
#include "account.h"

namespace OCC {

class SyncJournalDb;
class SyncEngine;

class SocketUploadJob : public QObject
{
    Q_OBJECT
public:
    SocketUploadJob(const QSharedPointer<SocketApiJobV2> &job);
    void start();

private:
    void fail(const QString &error);

    // create a fake SyncFileItemPtr to display a message in the protocol
    void logMessage(const QString &localPath, const QString &message, bool ok = true);

    QString _localPath;
    QSharedPointer<SocketApiJobV2> _apiJob;
    QStringList _syncedFiles;
    QStringList _errorFiles;
    int _finisedTagId = -1;
    QByteArray _backupFileId;

    void prepareTag(const AccountPtr &account);
};
}
