/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cloudprovidersproviderexporter.h>
#include <gio/gio.h>
#include <glib.h>

#include "account.h"
#include "cloudproviderconfig.h"
#include "cloudprovidermanager.h"
#include "cloudproviderwrapper.h"

#include <QPointer>

using CloudProviderManagerCallbackData = QPointer<CloudProviderManager>;

static void destroyManagerCallbackData(gpointer user_data)
{
    delete static_cast<CloudProviderManagerCallbackData *>(user_data);
}

void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    Q_UNUSED(name);
    auto *self = static_cast<CloudProviderManagerCallbackData *>(user_data)->data();
    if (!self) {
        return;
    }

    self->registerProvider(connection);
}

void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    Q_UNUSED(connection);
    Q_UNUSED(name);
    auto *self = static_cast<CloudProviderManagerCallbackData *>(user_data)->data();
    if (!self) {
        return;
    }

    // Also called when the name could not be acquired at all, in which case there is nothing to release.
    self->unregisterProvider();
}

void CloudProviderManager::registerProvider(GDBusConnection *connection)
{
    // Registering again would leak the exporter and connect the signals twice.
    if (_providerExporter) {
        return;
    }
    _providerExporter = cloud_providers_provider_exporter_new(connection, LIBCLOUDPROVIDERS_DBUS_BUS_NAME, LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH);
    cloud_providers_provider_exporter_set_name(_providerExporter, APPLICATION_NAME);
    registerSignals();
}

void CloudProviderManager::registerSignals()
{
    OCC::FolderMan *folderManager = OCC::FolderMan::instance();
    // The name can be acquired again after being lost, so avoid duplicate connections.
    connect(folderManager, &OCC::FolderMan::folderListChanged, this, &CloudProviderManager::slotFolderListChanged, Qt::UniqueConnection);
    slotFolderListChanged(folderManager->map());
}

void CloudProviderManager::unregisterProvider()
{
    // The wrappers reference the exporter, so they have to go first.
    for (auto *wrapper : std::as_const(_map)) {
        if (_providerExporter) {
            cloud_providers_provider_exporter_remove_account(_providerExporter, wrapper->accountExporter());
        }
        delete wrapper;
    }
    _map.clear();
    g_clear_object(&_providerExporter);
}

CloudProviderManager::CloudProviderManager(QObject *parent)
    : QObject(parent)
{
    _folder_index = 0;
    _busOwnerId = g_bus_own_name(G_BUS_TYPE_SESSION,
                                 LIBCLOUDPROVIDERS_DBUS_BUS_NAME,
                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                 nullptr,
                                 on_name_acquired,
                                 on_name_lost,
                                 new CloudProviderManagerCallbackData(this),
                                 destroyManagerCallbackData);
}

CloudProviderManager::~CloudProviderManager()
{
    if (_busOwnerId != 0) {
        g_bus_unown_name(_busOwnerId);
    }
    unregisterProvider();
}

void CloudProviderManager::slotFolderListChanged(const Folder::Map &folderMap)
{
    if (!_providerExporter) {
        return;
    }

    QMapIterator<QString, CloudProviderWrapper *> i(_map);
    while (i.hasNext()) {
        i.next();
        if (!folderMap.contains(i.key())) {
            cloud_providers_provider_exporter_remove_account(_providerExporter, i.value()->accountExporter());
            delete _map.find(i.key()).value();
            _map.remove(i.key());
        }
    }

    Folder::MapIterator j(folderMap);
    while (j.hasNext()) {
        j.next();
        if (!_map.contains(j.key())) {
            auto *cpo = new CloudProviderWrapper(this, j.value(), _folder_index++, _providerExporter);
            _map.insert(j.key(), cpo);
        }
    }
}
