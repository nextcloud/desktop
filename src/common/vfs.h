/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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
#include <QScopedPointer>
#include <QSharedPointer>

#include "ocsynclib.h"

typedef struct csync_file_stat_s csync_file_stat_t;

namespace OCC {

class Account;
typedef QSharedPointer<Account> AccountPtr;
class SyncJournalDb;
class VfsPrivate;
class SyncFileItem;
typedef QSharedPointer<SyncFileItem> SyncFileItemPtr;

struct OCSYNC_EXPORT VfsSetupParams
{
    QString filesystemPath;
    QString remotePath;

    AccountPtr account;
    // The journal must live at least until the stop() call
    SyncJournalDb *journal;

    QString providerName;
    QString providerVersion;
};

class OCSYNC_EXPORT Vfs : public QObject
{
    Q_OBJECT

public:
    enum Mode
    {
        Off,
        WithSuffix,
        WindowsCfApi,
    };
    static QString modeToString(Mode mode);
    static bool modeFromString(const QString &str, Mode *mode);

public:
    Vfs(QObject* parent = nullptr);
    virtual ~Vfs();

    virtual Mode mode() const = 0;

    // For WithSuffix modes: what's the suffix (including the dot)?
    virtual QString fileSuffix() const = 0;

    virtual void registerFolder(const VfsSetupParams &params) = 0;
    virtual void start(const VfsSetupParams &params) = 0;
    virtual void stop() = 0;
    virtual void unregisterFolder() = 0;

    virtual bool isHydrating() const = 0;

    // Update placeholder metadata during discovery
    virtual bool updateMetadata(const QString &filePath, time_t modtime, quint64 size, const QByteArray &fileId, QString *error) = 0;

    // Create and convert placeholders in PropagateDownload
    virtual void createPlaceholder(const QString &syncFolder, const SyncFileItemPtr &item) = 0;
    virtual void convertToPlaceholder(const QString &filename, const SyncFileItemPtr &item) = 0;

    // Determine whether something is a placeholder
    virtual bool isDehydratedPlaceholder(const QString &filePath) = 0;

    // Determine whether something is a placeholder in discovery
    // stat has at least 'path' filled
    // the stat_data argument has platform specific data
    // returning true means that the file_stat->type was set and should be fixed
    virtual bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) = 0;

signals:
    void beginHydrating();
    void doneHydrating();
};

bool isVfsPluginAvailable(Vfs::Mode mode) OCSYNC_EXPORT;
Vfs::Mode bestAvailableVfsMode() OCSYNC_EXPORT;
Vfs *createVfsFromPlugin(Vfs::Mode mode, QObject *parent) OCSYNC_EXPORT;

} // namespace OCC
