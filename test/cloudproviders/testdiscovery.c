/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cloudproviderscollector.h>
#include <cloudprovidersprovider.h>
#include <cloudprovidersproviderexporter.h>
#include <gio/gio.h>

static const gchar *provider_name = "Nextcloud passive discovery test";
static const gint64 discovery_timeout = 5 * G_TIME_SPAN_SECOND;
static const guint poll_interval_ms = 10;

typedef struct {
    CloudProvidersCollector *collector;
    CloudProvidersProviderExporter *exporter;
    GDBusConnection *connection;
    GMainLoop *loop;
    const gchar *bus_name;
    const gchar *object_path;
    gint64 deadline;
    guint owner_serial;
    gint passive_ready;
    guint owner_id;
    gboolean discovered;
} Discovery;

static GDBusMessage *observe_passive_discovery(GDBusConnection *connection, GDBusMessage *message,
    gboolean incoming, gpointer data)
{
    Discovery *discovery = data;
    (void)connection;

    if (!incoming && g_dbus_message_get_message_type(message) == G_DBUS_MESSAGE_TYPE_METHOD_CALL) {
        const gchar *member = g_dbus_message_get_member(message);
        GVariant *body = g_dbus_message_get_body(message);
        const gchar *name;
        if (g_strcmp0(member, "GetNameOwner") == 0) {
            g_variant_get(body, "(&s)", &name);
            if (g_strcmp0(name, discovery->bus_name) == 0) {
                discovery->owner_serial = g_dbus_message_get_serial(message);
            }
        }
    } else if (incoming && g_dbus_message_get_message_type(message) == G_DBUS_MESSAGE_TYPE_ERROR
        && discovery->owner_serial != 0
        && g_dbus_message_get_reply_serial(message) == discovery->owner_serial
        && g_strcmp0(g_dbus_message_get_error_name(message), "org.freedesktop.DBus.Error.NameHasNoOwner") == 0) {
        g_atomic_int_set(&discovery->passive_ready, TRUE);
    }
    return message;
}

static gboolean discover_manual_start(gpointer data)
{
    Discovery *discovery = data;
    GList *providers = cloud_providers_collector_get_providers(discovery->collector);

    if (g_get_monotonic_time() > discovery->deadline) {
        g_error("CloudProviders did not discover %s after manual startup", discovery->bus_name);
    }

    if (providers == NULL || !g_atomic_int_get(&discovery->passive_ready)) {
        return G_SOURCE_CONTINUE;
    }

    if (discovery->owner_id == 0) {
        g_assert_cmpuint(g_list_length(providers), ==, 1);
        g_assert_null(cloud_providers_provider_get_name(providers->data));
        discovery->exporter = cloud_providers_provider_exporter_new(
            discovery->connection, discovery->bus_name, discovery->object_path);
        cloud_providers_provider_exporter_set_name(discovery->exporter, provider_name);
        discovery->owner_id = g_bus_own_name_on_connection(
            discovery->connection, discovery->bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
            NULL, NULL, NULL, NULL);
        return G_SOURCE_CONTINUE;
    }

    if (g_strcmp0(cloud_providers_provider_get_name(providers->data), provider_name) == 0) {
        discovery->discovered = TRUE;
        g_main_loop_quit(discovery->loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 3);
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_no_error(error);

    g_autoptr(GVariant) result = g_dbus_connection_call_sync(
        connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "ListActivatableNames", NULL, G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error);
    g_assert_no_error(error);
    g_auto(GStrv) activatable_names = NULL;
    g_variant_get(result, "(^as)", &activatable_names);
    g_assert_false(g_strv_contains((const gchar *const *)activatable_names, argv[1]));

    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
    Discovery discovery = {
        .connection = connection,
        .loop = loop,
        .bus_name = argv[1],
        .object_path = argv[2],
        .deadline = g_get_monotonic_time() + discovery_timeout,
    };
    guint filter_id = g_dbus_connection_add_filter(connection, observe_passive_discovery, &discovery, NULL);
    g_autoptr(CloudProvidersCollector) collector = cloud_providers_collector_dup_singleton();
    discovery.collector = collector;
    g_timeout_add(poll_interval_ms, discover_manual_start, &discovery);
    g_main_loop_run(loop);
    g_assert_true(discovery.discovered);
    g_dbus_connection_remove_filter(connection, filter_id);
    g_bus_unown_name(discovery.owner_id);
    g_clear_object(&discovery.exporter);
    g_print("PASS: no D-Bus activation; existing collector discovered manual startup\n");
    return 0;
}
