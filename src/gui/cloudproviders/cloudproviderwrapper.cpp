/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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
    #include <cloudprovidersaccountexporter.h>
    #include <cloudprovidersproviderexporter.h>
}

#include "cloudproviderwrapper.h"
#include <account.h>
#include <folder.h>
#include <accountstate.h>
#include <QDesktopServices>
#include "openfilemanager.h"
#include "owncloudgui.h"
#include "application.h"

using namespace OCC;

GSimpleActionGroup *actionGroup = nullptr;

static
gchar* qstring_to_gchar(const QString &string)
{
    QByteArray ba = string.toUtf8();
    char* data = ba.data();
    return g_strdup(data);
}

CloudProviderWrapper::CloudProviderWrapper(QObject *parent, Folder *folder, CloudProvidersProviderExporter* cloudprovider) : QObject(parent)
  , _folder(folder)
{
    _recentlyChanged = new QList<QPair<QString, QString>>();
    gchar *accountName = g_strdup_printf ("Account%sFolder%s",
                                    qstring_to_gchar(folder->alias()),
                                    qstring_to_gchar(folder->accountState()->account()->id()));

    _cloudProvider = CLOUD_PROVIDERS_PROVIDER_EXPORTER(cloudprovider);
    _cloudProviderAccount = cloud_providers_account_exporter_new(_cloudProvider, accountName);

    gchar* folderName = qstring_to_gchar(folder->shortGuiLocalPath());
    gchar* folderPath = qstring_to_gchar(folder->cleanPath());
    cloud_providers_account_exporter_set_name (_cloudProviderAccount, folderName);
    cloud_providers_account_exporter_set_icon (_cloudProviderAccount, g_icon_new_for_string(APPLICATION_ICON_NAME, nullptr));
    cloud_providers_account_exporter_set_path (_cloudProviderAccount, folderPath);
    cloud_providers_account_exporter_set_status (_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE);
    cloud_providers_account_exporter_set_menu_model (_cloudProviderAccount, getMenuModel());
    cloud_providers_account_exporter_set_action_group (_cloudProviderAccount, getActionGroup());

    connect(ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, ProgressInfo)), this, SLOT(slotUpdateProgress(QString, ProgressInfo)));
    connect(_folder, SIGNAL(syncStarted()), this, SLOT(slotSyncStarted()));
    connect(_folder, SIGNAL(syncFinished(SyncResult)), this, SLOT(slotSyncFinished(const SyncResult)));
    connect(_folder, SIGNAL(syncPausedChanged(Folder*,bool)), this, SLOT(slotSyncPausedChanged(Folder*, bool)));

    _paused = _folder->syncPaused();
    updatePauseStatus();

    g_free(accountName);
    g_free(folderName);
    g_free(folderPath);
}

CloudProviderWrapper::~CloudProviderWrapper()
{
    g_object_unref(_cloudProviderAccount);
    g_object_unref(_mainMenu);
    g_object_unref(actionGroup);
}

CloudProvidersAccountExporter* CloudProviderWrapper::accountExporter()
{
    return _cloudProviderAccount;
}

static bool shouldShowInRecentsMenu(const SyncFileItem &item)
{
    return !Progress::isIgnoredKind(item._status)
            && item._instruction != CSYNC_INSTRUCTION_EVAL
            && item._instruction != CSYNC_INSTRUCTION_NONE;
}

void CloudProviderWrapper::slotUpdateProgress(const QString &folder, const ProgressInfo &progress)
{
    // Only update progress for the current folder
    Folder *f = FolderMan::instance()->folder(folder);
    if (f != _folder)
        return;

    // Build recently changed files list
    if (!progress._lastCompletedItem.isEmpty() && shouldShowInRecentsMenu(progress._lastCompletedItem)) {
        QString kindStr = Progress::asResultString(progress._lastCompletedItem);
        QString timeStr = QTime::currentTime().toString("hh:mm");
        QString actionText = tr("%1 (%2, %3)").arg(progress._lastCompletedItem._file, kindStr, timeStr);
        if (f) {
            QString fullPath = f->path() + '/' + progress._lastCompletedItem._file;
            if (QFile(fullPath).exists()) {
                if (_recentlyChanged->length() > 5)
                    _recentlyChanged->removeFirst();
                _recentlyChanged->append(qMakePair(actionText, fullPath));
            } else {
                _recentlyChanged->append(qMakePair(actionText, QString("")));
            }
        }

    }

    // Build status details text
    QString msg;
    if (!progress._currentDiscoveredRemoteFolder.isEmpty()) {
        msg =  tr("Checking for changes in '%1'").arg(progress._currentDiscoveredRemoteFolder);
    } else if (progress.totalSize() == 0) {
        quint64 currentFile = progress.currentFile();
        quint64 totalFileCount = qMax(progress.totalFiles(), currentFile);
        if (progress.trustEta()) {
            msg = tr("Syncing %1 of %2  (%3 left)")
                    .arg(currentFile)
                    .arg(totalFileCount)
                    .arg(Utility::durationToDescriptiveString2(progress.totalProgress().estimatedEta));
        } else {
            msg = tr("Syncing %1 of %2")
                    .arg(currentFile)
                    .arg(totalFileCount);
        }
    } else {
        QString totalSizeStr = Utility::octetsToString(progress.totalSize());
        if (progress.trustEta()) {
            msg = tr("Syncing %1 (%2 left)")
                    .arg(totalSizeStr, Utility::durationToDescriptiveString2(progress.totalProgress().estimatedEta));
        } else {
            msg = tr("Syncing %1")
                    .arg(totalSizeStr);
        }
    }
    updateStatusText(msg);

    if (!progress._lastCompletedItem.isEmpty()
            && shouldShowInRecentsMenu(progress._lastCompletedItem)) {
        GMenuItem* item;
        g_menu_remove_all (G_MENU(_recentMenu));
        if(!_recentlyChanged->isEmpty()) {
            QList<QPair<QString, QString>>::iterator i;
            for (i = _recentlyChanged->begin(); i != _recentlyChanged->end(); i++) {
                gchar *file;
                QString label = i->first;
                QString fullPath = i->second;
                file = g_strdup(qstring_to_gchar(label));
                item = g_menu_item_new(file, "cloudprovider.showfile");
                g_menu_item_set_action_and_target_value(item, "cloudprovider.showfile", g_variant_new_string(qstring_to_gchar(fullPath)));
                g_menu_append_item(_recentMenu, item);
            }
        } else {
            item = g_menu_item_new("No recently changed files", nullptr);
            g_menu_append_item(_recentMenu, item);
        }
    }
}

void CloudProviderWrapper::updateStatusText(QString statusText)
{
    char* state = qstring_to_gchar(_folder->accountState()->stateString(_folder->accountState()->state()));
    char* statusChar = qstring_to_gchar(statusText);
    char* status = g_strdup_printf("%s - %s", state, statusChar);
    cloud_providers_account_exporter_set_status_details(_cloudProviderAccount, status);
    g_free(state);
    g_free(statusChar);
    g_free(status);
}

void CloudProviderWrapper::updatePauseStatus()
{
    if (_paused) {
        updateStatusText(tr("Sync paused"));
        cloud_providers_account_exporter_set_status (_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR);
    } else {
        updateStatusText(tr("Syncing"));
        cloud_providers_account_exporter_set_status (_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING);
    }
}

Folder* CloudProviderWrapper::folder()
{
    return _folder;
}

void CloudProviderWrapper::slotSyncStarted()
{
    cloud_providers_account_exporter_set_status(_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING);
}

void CloudProviderWrapper::slotSyncFinished(const SyncResult &result)
{
    if (result.status() == result.Success || result.status() == result.Problem)
    {
        cloud_providers_account_exporter_set_status(_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE);
        updateStatusText(result.statusString());
        return;
    }
    cloud_providers_account_exporter_set_status(_cloudProviderAccount, CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR);
    updateStatusText(result.statusString());
}

GMenuModel* CloudProviderWrapper::getMenuModel() {

    GMenu* section;
    GMenuItem* item;

    _mainMenu = g_menu_new();

    section = g_menu_new();
    item = g_menu_item_new("Open website", "cloudprovider.openwebsite");
    g_menu_append_item(section, item);
    g_menu_append_section(_mainMenu, nullptr, G_MENU_MODEL(section));

    _recentMenu = g_menu_new();
    item = g_menu_item_new("No recently changed files", nullptr);
    g_menu_append_item(_recentMenu, item);
    section = g_menu_new();
    item = g_menu_item_new_submenu("Recently changed", G_MENU_MODEL(_recentMenu));
    g_menu_append_item(section, item);
    g_menu_append_section(_mainMenu, nullptr, G_MENU_MODEL(section));

    section = g_menu_new();
    item = g_menu_item_new("Pause synchronization", "cloudprovider.pause");
    g_menu_append_item(section, item);
    g_menu_append_section(_mainMenu, nullptr, G_MENU_MODEL(section));

    section = g_menu_new();
    item = g_menu_item_new("Help", "cloudprovider.openhelp");
    g_menu_append_item(section, item);
    item = g_menu_item_new("Settings", "cloudprovider.opensettings");
    g_menu_append_item(section, item);
    item = g_menu_item_new("Log out", "cloudprovider.logout");
    g_menu_append_item(section, item);
    item = g_menu_item_new("Quit sync client", "cloudprovider.quit");
    g_menu_append_item(section, item);
    g_menu_append_section(_mainMenu, nullptr, G_MENU_MODEL(section));

    return G_MENU_MODEL(_mainMenu);
}

static void
activate_action_open (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Q_UNUSED(parameter);
    const gchar *name = g_action_get_name(G_ACTION(action));
    auto *self = static_cast<CloudProviderWrapper*>(user_data);
    auto *gui = dynamic_cast<ownCloudGui*>(self->parent()->parent());

    if(g_str_equal(name, "openhelp")) {
        gui->slotHelp();
    }

    if(g_str_equal(name, "opensettings")) {
        gui->slotShowSettings();
    }

    if(g_str_equal(name, "openwebsite")) {
        QDesktopServices::openUrl(self->folder()->accountState()->account()->url());
    }

    if(g_str_equal(name, "openfolder")) {
        showInFileManager(self->folder()->cleanPath());
    }

    if(g_str_equal(name, "showfile")) {
            gchar *path;
            g_variant_get (parameter, "s", &path);
            g_print("showfile => %s\n", path);
            showInFileManager(QString(path));
        }

    if(g_str_equal(name, "logout")) {
        self->folder()->accountState()->signOutByUi();
    }

    if(g_str_equal(name, "quit")) {
        qApp->quit();
    }
}

static void
activate_action_openrecentfile (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Q_UNUSED(action);
    Q_UNUSED(parameter);
    auto *self = static_cast<CloudProviderWrapper*>(user_data);
    QDesktopServices::openUrl(self->folder()->accountState()->account()->url());
}

static void
activate_action_pause (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
    Q_UNUSED(parameter);
    auto *self = static_cast<CloudProviderWrapper*>(user_data);
    GVariant *old_state, *new_state;

    old_state = g_action_get_state (G_ACTION (action));
    new_state = g_variant_new_boolean (!(bool)g_variant_get_boolean (old_state));
    self->folder()->setSyncPaused((bool)g_variant_get_boolean(new_state));
    g_simple_action_set_state (action, new_state);
    g_variant_unref (old_state);
}

static GActionEntry actions[] = {
    { "openwebsite",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "quit",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "logout",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "openfolder",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "showfile",  activate_action_open, "s", nullptr, nullptr, {0,0,0}},
    { "openhelp",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "opensettings",  activate_action_open, nullptr, nullptr, nullptr, {0,0,0}},
    { "openrecentfile",  activate_action_openrecentfile, "s", nullptr, nullptr, {0,0,0}},
    { "pause",  activate_action_pause, nullptr, "false", nullptr, {0,0,0}}
};

GActionGroup* CloudProviderWrapper::getActionGroup()
{
    actionGroup = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (actionGroup), actions, G_N_ELEMENTS (actions), this);
    bool state = _folder->syncPaused();
    GAction *pause = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "pause");
    g_simple_action_set_state(G_SIMPLE_ACTION(pause), g_variant_new_boolean(state));
    return G_ACTION_GROUP (actionGroup);
}

void CloudProviderWrapper::slotSyncPausedChanged(Folder *folder, bool state)
{
    Q_UNUSED(folder);
    _paused = state;
    GAction *pause = g_action_map_lookup_action(G_ACTION_MAP(actionGroup), "pause");
    g_simple_action_set_state (G_SIMPLE_ACTION(pause), g_variant_new_boolean(state));
    updatePauseStatus();
}
