/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2025 OpenCloud GmbH and OpenCloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "common/vfs.h"
#include "common/plugin.h"
#include "common/result.h"
#include "discoveryphase.h"
#include "chronoelapsedtimer.h"

#include <QObject>
#include <QScopedPointer>
#include <QProcess>
#include <QPointer>

#include <filesystem>

namespace OCC {
class HydrationJob;

class OpenVFS : public Vfs
{
    Q_OBJECT

public:
    explicit OpenVFS(QObject *parent = nullptr);
    ~OpenVFS() override;

    [[nodiscard]] Mode mode() const override;

    void stop() override;
    void unregisterFolder() override;

    [[nodiscard]] bool socketApiPinStateActionsShown() const override;

    Result<ConvertToPlaceholderResult, QString> updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile) override;
    // [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override { Q_UNUSED(filePath) return true; }

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;

    bool needsMetadataUpdate(const SyncFileItem &item) override;
    bool isDehydratedPlaceholder(const QString &filePath) override;
    [[nodiscard]] bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data) override;
    // LocalInfo statTypeVirtualFile(const std::filesystem::directory_entry &path, ItemType type) override;

    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> pinState(const QString &folderPath) override;
    [[nodiscard]] AvailabilityResult availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck) override;

    HydrationJob *hydrateFile(const QByteArray &fileId, const QString &targetPath);

    [[nodiscard]] QString fileSuffix() const override;

    [[nodiscard]] bool isHydrating() const override;

    [[nodiscard]] Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const SyncFileItem &item) override;

    [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override;

    [[nodiscard]] Result<void, QString> createPlaceholders(const QList<SyncFileItemPtr> &items) override;

    [[nodiscard]] Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) override;

    [[nodiscard]] Result<Vfs::ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString&, const OCC::SyncFileItem&, const QString&, UpdateMetadataTypes) override;

Q_SIGNALS:
    void finished(OCC::Result<void, QString>);

public Q_SLOTS:
    void fileStatusChanged(const QString &systemFileName, OCC::SyncFileStatus fileStatus) override;

    void slotHydrateJobFinished();

protected:
    void startImpl(const VfsSetupParams &params) override;

private:
    QMap<QByteArray, HydrationJob *> _hydrationJobs;
    QPointer<QProcess> _openVfsProcess;
};

class OpenVfsPluginFactory : public QObject, public DefaultPluginFactory<OpenVFS>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)

public:
    [[nodiscard]] bool checkAvailability() const override;
    Result<void, QString> prepare(const QString &path, const QUuid &accountUuid) const override;

private:
    mutable Utility::ChronoElapsedTimer _cacheTimer = false;
    mutable QStringList _fuseMountCache;
};

} // namespace OCC
