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

    [[nodiscard]] Mode mode() const override;
    [[nodiscard]] QString fileSuffix() const override;

    void stop() override;
    void unregisterFolder() override;

    [[nodiscard]] bool socketApiPinStateActionsShown() const override { return true; }
    [[nodiscard]] bool isHydrating() const override;

    Result<void, QString> updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId) override;

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;
    Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) override;
    Result<Vfs::ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &, UpdateMetadataTypes updateType) override;

    bool needsMetadataUpdate(const SyncFileItem &) override { return false; }
    bool isDehydratedPlaceholder(const QString &filePath) override;
    bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) override;

    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> pinState(const QString &folderPath) override
    { return pinStateInDb(folderPath); }
    AvailabilityResult availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck) override;

public slots:
    void fileStatusChanged(const QString &, OCC::SyncFileStatus) override {}

protected:
    void startImpl(const VfsSetupParams &params) override;
};

class SuffixVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsSuffix>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)
};

} // namespace OCC
