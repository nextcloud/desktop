/*
 * Copyright (C) by Julius Härtl <jus@bitgrid.net>
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

#include <glib.h>
#include <gio/gio.h>
#include <cloudprovidersproviderexporter.h>

#include "cloudproviderwrapper.h"
#include "cloudprovidermanager.h"
#include "account.h"
#include "cloudproviderconfig.h"

CloudProvidersProviderExporter *_providerExporter;

void on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    Q_UNUSED(name);
    CloudProviderManager *self = nullptr;
    self = static_cast<CloudProviderManager*>(user_data);
    _providerExporter = cloud_providers_provider_exporter_new(connection, LIBCLOUDPROVIDERS_DBUS_BUS_NAME, LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH);
    cloud_providers_provider_exporter_set_name (_providerExporter, APPLICATION_NAME);
    self->registerSignals();
}

void on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    Q_UNUSED(connection);
    Q_UNUSED(name);
    Q_UNUSED(user_data);
    g_clear_object (&_providerExporter);
}

void CloudProviderManager::registerSignals()
{
    OCC::FolderMan *folderManager = OCC::FolderMan::instance();
    connect(folderManager, &OCC::FolderMan::folderListChanged,
            this, &CloudProviderManager::slotFolderListChanged);
    slotFolderListChanged(folderManager->map());
}

CloudProviderManager::CloudProviderManager(QObject *parent) : QObject(parent)
{
    _folder_index = 0;
    g_bus_own_name (G_BUS_TYPE_SESSION, LIBCLOUDPROVIDERS_DBUS_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, nullptr, on_name_acquired, nullptr, this, nullptr);
}

void CloudProviderManager::slotFolderListChanged(const Folder::Map &folderMap)
{
    QMapIterator<QString, CloudProviderWrapper*> i(_map);
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
