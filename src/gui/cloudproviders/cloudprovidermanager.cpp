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

extern "C" {
    #include <glib.h>
    #include <gio.h>
    #include <cloudprovidersproviderexporter.h>
}
#include "cloudproviderwrapper.h"
#include "cloudprovidermanager.h"
#include "account.h"
#include "cloudproviderconfig.h"

CloudProvidersProviderExporter *_providerExporter;

void on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    Q_UNUSED(name);
    CloudProviderManager *self;
    self = static_cast<CloudProviderManager*>(user_data);
    _providerExporter = cloud_providers_provider_exporter_new(connection, LIBCLOUDPROVIDERS_DBUS_BUS_NAME, LIBCLOUDPROVIDERS_DBUS_OBJECT_PATH);
    cloud_providers_provider_exporter_set_name (_providerExporter, APPLICATION_NAME);
    self->registerSignals();
}

void CloudProviderManager::registerSignals()
{
    OCC::FolderMan *folderManager = OCC::FolderMan::instance();
    connect(folderManager, SIGNAL(folderListChanged(const Folder::Map &)), SLOT(slotFolderListChanged(const Folder::Map &)));
    slotFolderListChanged(folderManager->map());
}

CloudProviderManager::CloudProviderManager(QObject *parent) : QObject(parent)
{
    _map = new QMap<QString, CloudProviderWrapper*>();
    QString busName = QString(LIBCLOUDPROVIDERS_DBUS_BUS_NAME);
    g_bus_own_name (G_BUS_TYPE_SESSION, busName.toAscii().data(), G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, NULL, NULL, this, NULL);
}

void CloudProviderManager::slotFolderListChanged(const Folder::Map &folderMap)
{
    QMapIterator<QString, CloudProviderWrapper*> i(*_map);
    while (i.hasNext()) {
        i.next();
        if (!folderMap.contains(i.key())) {
            cloud_providers_provider_exporter_remove_account(_providerExporter, i.value()->accountExporter());
            delete _map->find(i.key()).value();
            _map->remove(i.key());
        }
    }

    Folder::MapIterator j(folderMap);
    while (j.hasNext()) {
        j.next();
        if (!_map->contains(j.key())) {
            auto *cpo = new CloudProviderWrapper(this, j.value(), _providerExporter);
            _map->insert(j.key(), cpo);
        }
    }
}
