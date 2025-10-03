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
#include <QScopedPointer>

#include "common/vfs.h"

namespace OCC {

class VfsXAttr : public Vfs
{
    Q_OBJECT

public:
    explicit VfsXAttr(QObject *parent = nullptr);
    ~VfsXAttr();

    Mode mode() const override;
    QString fileSuffix() const override;

    void stop() override;
    void unregisterFolder() override;

    bool socketApiPinStateActionsShown() const override;
    bool isHydrating() const override;

    Result<void, QString> updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId) override;

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;
    Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) override;
    Result<void, QString> convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &replacesFile) override;

    bool needsMetadataUpdate(const SyncFileItem &item) override;
    bool isDehydratedPlaceholder(const QString &filePath) override;
    bool statTypeVirtualFile(csync_file_stat_t *stat, void *statData) override;

    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> pinState(const QString &folderPath) override;
    AvailabilityResult availability(const QString &folderPath) override;

public slots:
    void fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus) override;

protected:
    void startImpl(const VfsSetupParams &params) override;
};

} // namespace OCC
