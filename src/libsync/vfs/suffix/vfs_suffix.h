/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile) override;
    Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const SyncFileItem &item) override {Q_UNUSED(filePath) Q_UNUSED(item) return {QString{}};}
    [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override { Q_UNUSED(filePath) return true; }

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;
    Result<void, QString> createPlaceholders(const QList<SyncFileItemPtr> &items) override;

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
