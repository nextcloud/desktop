/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "account.h"

class QLocalServer;
class QLocalSocket;

namespace OCC {
class GETFileJob;
class SyncJournalDb;

class OWNCLOUDSYNC_EXPORT HydrationJob : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Success = 0,
        Error,
    };
    Q_ENUM(Status)

    explicit HydrationJob(QObject *parent = nullptr);

    AccountPtr account() const;
    void setAccount(const AccountPtr &account);

    QString remotePath() const;
    void setRemotePath(const QString &remotePath);

    QString localPath() const;
    void setLocalPath(const QString &localPath);

    SyncJournalDb *journal() const;
    void setJournal(SyncJournalDb *journal);

    QString requestId() const;
    void setRequestId(const QString &requestId);

    QString folderPath() const;
    void setFolderPath(const QString &folderPath);

    Status status() const;

    void start();
    void cancel();

signals:
    void finished(HydrationJob *job);
    void canceled(HydrationJob *job);

private:
    void emitFinished(Status status);
    void emitCanceled();

    void onNewConnection();
    void onGetFinished();
    void onGetCanceled();

    AccountPtr _account;
    QString _remotePath;
    QString _localPath;
    SyncJournalDb *_journal = nullptr;

    QString _requestId;
    QString _folderPath;

    QLocalServer *_server = nullptr;
    QLocalSocket *_socket = nullptr;
    GETFileJob *_job = nullptr;
    Status _status = Success;
};

} // namespace OCC
