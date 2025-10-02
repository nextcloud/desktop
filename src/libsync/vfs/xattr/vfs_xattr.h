/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QObject>
#include <QScopedPointer>

#include "common/vfs.h"
#include "common/plugin.h"

namespace OCC {

class VfsXAttr : public Vfs
{
    Q_OBJECT

public:
    explicit VfsXAttr(QObject *parent = nullptr);
    ~VfsXAttr() override;

    [[nodiscard]] Mode mode() const override;
    [[nodiscard]] QString fileSuffix() const override;

    void stop() override;
    void unregisterFolder() override;

    [[nodiscard]] bool socketApiPinStateActionsShown() const override;
    [[nodiscard]] bool isHydrating() const override;

    OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile) override;
    Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const SyncFileItem &syncItem) override {Q_UNUSED(filePath) Q_UNUSED(syncItem) return {QString{}};}
    [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override { Q_UNUSED(filePath) return true; }

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;
    Result<void, QString> createPlaceholders(const QList<SyncFileItemPtr> &items) override;

    Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) override;
    Result<ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &filename,
                                                                     const SyncFileItem &item,
                                                                     const QString &replacesFile,
                                                                     UpdateMetadataTypes updateType) override;

    bool needsMetadataUpdate(const SyncFileItem &item) override;
    bool isDehydratedPlaceholder(const QString &filePath) override;
    bool statTypeVirtualFile(csync_file_stat_t *stat, void *statData) override;

    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> pinState(const QString &folderPath) override;
    AvailabilityResult availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck) override;

public slots:
    void fileStatusChanged(const QString &systemFileName, OCC::SyncFileStatus fileStatus) override;

protected:
    void startImpl(const VfsSetupParams &params) override;
};

class XattrVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsXAttr>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)
};

} // namespace OCC
