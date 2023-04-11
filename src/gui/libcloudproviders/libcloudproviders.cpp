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

#include <cloudproviders/cloudprovidersaccountexporter.h>
#include <cloudproviders/cloudprovidersproviderexporter.h>
#include <gio/gio.h>
#include <glib.h>

#include "libcloudproviders.h"
#include "libcloudproviders_p.h"

#include <QMap>
#include "folder.h"
#include "folderman.h"
#include "theme.h"
#include "accountstate.h"
#include "config.h"
#include "account.h"
#include "accountmanager.h"

// These must match the name and path defined in a file in $DATADIR/cloud-providers/
const gchar dbusName[] = APPLICATION_CLOUDPROVIDERS_DBUS_NAME;
const gchar dbusPath[] = APPLICATION_CLOUDPROVIDERS_DBUS_PATH;

namespace OCC {

LibCloudProvidersPrivate::~LibCloudProvidersPrivate()
{
    for (const auto folder : _folderExports.keys())
        unexportFolder(folder);
    g_clear_object(&_exporter);
    if (_busOwnerId)
        g_bus_unown_name(_busOwnerId);
}

static void onBusAcquired(GDBusConnection *connection, const gchar *, gpointer userData)
{
    auto d = static_cast<LibCloudProvidersPrivate *>(userData);

    d->_exporter = cloud_providers_provider_exporter_new(connection, dbusName, dbusPath);
    cloud_providers_provider_exporter_set_name(d->_exporter, Theme::instance()->appNameGUI().toUtf8().constData());

    d->updateExportedFolderList();
}

static void onNameLost(GDBusConnection *, const gchar *, gpointer userData)
{
    auto d = static_cast<LibCloudProvidersPrivate *>(userData);

    d->_folderExports.clear();
    g_clear_object(&d->_exporter);
}

void LibCloudProvidersPrivate::start()
{
    _busOwnerId = g_bus_own_name(
        G_BUS_TYPE_SESSION, dbusName, G_BUS_NAME_OWNER_FLAGS_NONE,
        &onBusAcquired,
        nullptr, // onNameAcquired
        &onNameLost,
        this, // user data
        nullptr // user data free func
    );

    auto folderMan = FolderMan::instance();
    connect(folderMan, &FolderMan::folderListChanged,
            this, &LibCloudProvidersPrivate::updateExportedFolderList);
    connect(folderMan, &FolderMan::folderSyncStateChange,
            this, &LibCloudProvidersPrivate::updateFolderExport);
}

void LibCloudProvidersPrivate::updateExportedFolderList()
{
    const auto newFolders = FolderMan::instance()->folders();
    const auto oldFolders = _folderExports.keys();

    // Remove folders that are no longer exported
    for (const auto old : oldFolders) {
        if (!newFolders.contains(old))
            unexportFolder(old);
    }

    // Add new folders
    for (const auto n : newFolders) {
        if (!oldFolders.contains(n))
            exportFolder(n);
    }
}

static void actionDispatcher(GSimpleAction *action, GVariant *, gpointer userData)
{
    gchar *strval;
    g_object_get(action, "name", &strval, NULL);
    QByteArray name(strval);
    g_free(strval);

    auto d = static_cast<LibCloudProvidersPrivate *>(userData);
    auto q = d->_q;
    if (name == "settings")
        q->showSettings();
    else
        OC_ASSERT_X(false, "unknown action string");
}


void LibCloudProvidersPrivate::exportFolder(Folder *folder)
{
    if (!_exporter)
        return;

    GError *error = nullptr;
    auto icon = g_icon_new_for_string(APPLICATION_ICON_NAME, &error);
    if (error) {
        qWarning() << "Could not create icon for" << APPLICATION_ICON_NAME << "error" << error->message;
        g_error_free(error);
    }

    // DBus object paths must not contain characters other than [A-Z][a-z][0-9]_, see g_variant_is_object_path
    const QByteArray dBusCompatibleFolderId = folder->id().replace('-', nullptr);

    auto exporter = cloud_providers_account_exporter_new(_exporter, dBusCompatibleFolderId.constData());
    cloud_providers_account_exporter_set_path(exporter, folder->path().toUtf8().constData());
    cloud_providers_account_exporter_set_icon(exporter, icon);
    cloud_providers_account_exporter_set_status(exporter, CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE);

    auto menu = g_menu_new();
    // The "cloudprovider" scope is hardcoded into the gtk code that uses this data.
    // Different scopes will not work.
    g_menu_append(menu, tr("Settings").toUtf8().constData(), "cloudprovider.settings");

    auto actionGroup = g_simple_action_group_new();
    const GActionEntry entries[] = {
        { "settings", actionDispatcher, nullptr, nullptr, nullptr, {}},
      };
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup), entries, G_N_ELEMENTS(entries), this);

    cloud_providers_account_exporter_set_menu_model(exporter, G_MENU_MODEL(menu));
    cloud_providers_account_exporter_set_action_group(exporter, G_ACTION_GROUP(actionGroup));

    // Currently there's no reason for us to keep these around: no further modifications are done
    g_clear_object(&menu);
    g_clear_object(&actionGroup);

    _folderExports[folder] = FolderExport{folder, exporter};
    updateFolderExport();
}

void LibCloudProvidersPrivate::unexportFolder(Folder *folder)
{
    if (!_folderExports.contains(folder))
        return;
    auto folderExporter = _folderExports[folder]._exporter;
    cloud_providers_provider_exporter_remove_account(_exporter, folderExporter);
    // the remove_account already calls _unref
    _folderExports.remove(folder);
}

void LibCloudProvidersPrivate::updateFolderExport()
{
    for (auto folderExport : _folderExports) {
        if (!folderExport._folder)
            continue;
        Folder *folder = folderExport._folder;

        // Update the name, may change if accounts are added/removed
        QString displayName = folder->displayName();
        if (AccountManager::instance()->accounts().size() > 1) {
            displayName = QStringLiteral("%1 (%2)").arg(
                    displayName, folder->accountState()->account()->displayName());
        }
        cloud_providers_account_exporter_set_name(folderExport._exporter, displayName.toUtf8().constData());

        CloudProvidersAccountStatus status = CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID;
        auto syncResult = folder->syncResult();
        switch (syncResult.status()) {
        case SyncResult::Undefined:
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID;
            break;
        case SyncResult::NotYetStarted:
        case SyncResult::Success:
        case SyncResult::Problem:
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE;
            break;
        case SyncResult::SyncPrepare:
        case SyncResult::SyncRunning:
        case SyncResult::SyncAbortRequested:
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING;
            break;
        case SyncResult::Error:
        case SyncResult::SetupError:
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR;
            break;
        case SyncResult::Paused:
            [[fallthrough]];
        case SyncResult::Offline:
            // There's no status that fits exactly. If our choice is only
            // between IDLE And ERROR, let's go for ERROR to show that no
            // syncing is happening.
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR;
            break;
        }

        // Similarly to Paused: If disconnected, show something's wrong!
        if (!folder->accountState()->isConnected())
            status = CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR;

        auto message = FolderMan::trayTooltipStatusString(
            syncResult,
            folder->syncPaused());

        cloud_providers_account_exporter_set_status(folderExport._exporter, status);
        cloud_providers_account_exporter_set_status_details(folderExport._exporter, message.toUtf8().constData());
    }
}

LibCloudProviders::LibCloudProviders(QObject *parent)
    : QObject(parent)
    , d_ptr(new LibCloudProvidersPrivate)
{
    d_ptr->_q = this;
}

void LibCloudProviders::start()
{
    d_ptr->start();
}

} // namespace OCC
