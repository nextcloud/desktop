/*
 * SPDX-FileCopyrightText: 2021 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QObject>
#include <QTemporaryFile>

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
    QSharedPointer<SocketApiJobV2> _apiJob;
    QString _localPath;
    QString _remotePath;
    QString _pattern;
    QTemporaryFile _tmp;
    SyncJournalDb *_db;
    SyncEngine *_engine;
    QStringList _syncedFiles;
};
}
