/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QObject>
#include <QScopedPointer>

#include "common/vfs.h"
#include "common/plugin.h"

namespace OCC {
class HydrationJob;
class VfsDBusApiPrivate;
class SyncJournalFileRecord;

class VfsDBusApi : public Vfs
{
    Q_OBJECT

public:
    explicit VfsDBusApi(QObject *parent = nullptr);
    ~VfsDBusApi();

    Mode mode() const override;
    QString fileSuffix() const override;

    void stop() override;
    void unregisterFolder() override;

    bool socketApiPinStateActionsShown() const override;
    bool isHydrating() const override;

    Result<void, QString> updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId) override;

    Result<Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &filePath, const QByteArray &fileId) override;

    [[nodiscard]] bool isPlaceHolderInSync(const QString &filePath) const override;

    Result<void, QString> createPlaceholder(const SyncFileItem &item) override;
    Result<void, QString> dehydratePlaceholder(const SyncFileItem &item) override;
    Result<Vfs::ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &replacesFile, UpdateMetadataTypes updateType) override;

    bool needsMetadataUpdate(const SyncFileItem &) override;
    bool isDehydratedPlaceholder(const QString &filePath) override;
    bool statTypeVirtualFile(csync_file_stat_t *stat, void *statData) override;

    bool setPinState(const QString &folderPath, PinState state) override;
    Optional<PinState> pinState(const QString &folderPath) override;
    AvailabilityResult availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck) override;

    void cancelHydration(const QString &requestId, const QString &path);

    int finalizeHydrationJob(const QString &requestId);

public slots:
    void requestHydration(const QString &requestId, const QString &path);
    void fileStatusChanged(const QString &systemFileName, OCC::SyncFileStatus fileStatus) override;

signals:
    void hydrationRequestReady(const QString &requestId);
    void hydrationRequestFailed(const QString &requestId);
    void hydrationRequestFinished(const QString &requestId);

protected:
    void startImpl(const VfsSetupParams &params) override;

private:
    struct HasHydratedDehydrated {
        bool hasHydrated = false;
        bool hasDehydrated = false;
    };
    struct HydratationAndPinStates {
        Optional<PinState> pinState;
        HasHydratedDehydrated hydrationStatus;
    };
    HydratationAndPinStates computeRecursiveHydrationAndPinStates(const QString &path, const Optional<PinState> &basePinState);

    QScopedPointer<VfsDBusApiPrivate> d;
};

class DBusApiVfsPluginFactory : public QObject, public DefaultPluginFactory<VfsDBusApi>
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.owncloud.PluginFactory" FILE "vfspluginmetadata.json")
    Q_INTERFACES(OCC::PluginFactory)
};

} // namespace OCC
