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

#include "common/vfs.h"
#include "common/plugin.h"

namespace OCC {

class VfsSuffix : public Vfs
{
    Q_OBJECT

public:
    explicit VfsSuffix(QObject *parent = nullptr);
    ~VfsSuffix() override;

    Mode mode() const override;
    QString fileSuffix() const override;
    QString underlyingFileName(const QString &fileName) const override;


    void stop() override;
    void unregisterFolder() override;

    bool socketApiPinStateActionsShown() const override { return true; }


    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;

    bool needsMetadataUpdate(const SyncFileItem &) override { return false; }
    bool isDehydratedPlaceholder(const QString &filePath) override;
    bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) override;

    bool setPinState(const QString &folderPath, PinState state) override
    { return setPinStateInDb(folderPath, state); }
    Optional<PinState> pinState(const QString &folderPath) override
    { return pinStateInDb(folderPath); }
    AvailabilityResult availability(const QString &folderPath) override;

public slots:
    void fileStatusChanged(const QString &, SyncFileStatus) override {}

protected:
    Result<ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &item, const QString &filePath, const QString &replacesFile) override;
    void startImpl(const VfsSetupParams &params) override;
};

class SuffixVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsSuffix>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)
};

} // namespace OCC
