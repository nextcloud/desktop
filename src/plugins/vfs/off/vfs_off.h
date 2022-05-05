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

#include "common/plugin.h"
#include "common/vfs.h"

namespace OCC {

class VfsOff : public Vfs
{
    Q_OBJECT

public:
    VfsOff(QObject *parent = nullptr);
    ~VfsOff() override;

    Mode mode() const override;

    QString fileSuffix() const override;

    void stop() override;
    void unregisterFolder() override;

    bool socketApiPinStateActionsShown() const override;
    bool isHydrating() const override;

    Result<void, QString> createPlaceholder(const SyncFileItem &) override;

    bool needsMetadataUpdate(const SyncFileItem &) override;
    bool isDehydratedPlaceholder(const QString &) override;
    bool statTypeVirtualFile(csync_file_stat_t *, void *) override;

    bool setPinState(const QString &, PinState) override;
    Optional<PinState> pinState(const QString &) override;
    AvailabilityResult availability(const QString &) override;

public slots:
    void fileStatusChanged(const QString &, SyncFileStatus) override;

protected:
    Result<ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &, const QString &, const QString &) override;
    void startImpl(const VfsSetupParams &) override;
};


class OffVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsOff>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)
};

} // namespace OCC
