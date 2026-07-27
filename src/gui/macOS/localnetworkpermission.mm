/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "localnetworkpermission.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>

#include <Network/Network.h>

#include <functional>
#include <memory>
#include <utility>

namespace {

void invokeCallback(const QPointer<QObject> &context, std::function<void(bool)> callback, bool denied)
{
    if (!context) {
        return;
    }

    QMetaObject::invokeMethod(context, [context, callback = std::move(callback), denied] {
        if (context) {
            callback(denied);
        }
    }, Qt::QueuedConnection);
}

bool isLocalNetworkDenied(nw_path_t path)
{
    return path
        && nw_path_get_status(path) == nw_path_status_unsatisfied
        && nw_path_get_unsatisfied_reason(path) == nw_path_unsatisfied_reason_local_network_denied;
}

bool checkAvailable()
{
    if (@available(macOS 15.0, *)) {
        return true;
    }

    return false;
}

struct ConnectionProbe
{
    nw_connection_t connection = nullptr;
    QPointer<QObject> context;
    std::function<void(bool)> callback;
    bool completed = false;

    void finish(bool denied)
    {
        if (completed) {
            return;
        }

        completed = true;
        nw_connection_cancel(connection);
        nw_release(connection);
        connection = nullptr;
        invokeCallback(context, std::move(callback), denied);
    }

};

} // namespace

namespace OCC::LocalNetworkPermission {

void checkDeniedForConnection(const QUrl &url, QObject *context, std::function<void(bool)> callback)
{
    if (!checkAvailable()) {
        invokeCallback(context, std::move(callback), false);
        return;
    }
    
    const auto host = url.host(QUrl::FullyEncoded);
    if (!context || host.isEmpty()) {
        invokeCallback(context, std::move(callback), false);
        return;
    }

    const auto port = QString::number(url.port(url.scheme() == QStringLiteral("https") ? 443 : 80));
    const auto hostUtf8 = host.toUtf8();
    const auto portUtf8 = port.toUtf8();
    const auto endpoint = nw_endpoint_create_host(hostUtf8.constData(), portUtf8.constData());
    const auto parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL,
                                                            NW_PARAMETERS_DEFAULT_CONFIGURATION);
    if (parameters) {
        nw_parameters_set_prefer_no_proxy(parameters, true);
    }
    if (!endpoint || !parameters) {
        if (endpoint) {
            nw_release(endpoint);
        }
        if (parameters) {
            nw_release(parameters);
        }
        invokeCallback(context, std::move(callback), false);
        return;
    }

    const auto connection = nw_connection_create(endpoint, parameters);
    nw_release(endpoint);
    nw_release(parameters);
    if (!connection) {
        invokeCallback(context, std::move(callback), false);
        return;
    }

    const auto probe = std::make_shared<ConnectionProbe>();
    probe->connection = connection;
    probe->context = context;
    probe->callback = std::move(callback);

    const auto queue = dispatch_get_main_queue();
    nw_connection_set_queue(connection, queue);
    nw_connection_set_path_changed_handler(connection, ^(nw_path_t path) {
        if (isLocalNetworkDenied(path)) {
            probe->finish(true);
        }
    });
    nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t state, nw_error_t) {
        switch (state) {
        case nw_connection_state_ready:
            probe->finish(false);
            break;
        case nw_connection_state_waiting:
        case nw_connection_state_failed: {
            if (probe->completed) {
                break;
            }

            const auto path = nw_connection_copy_current_path(connection);
            const auto denied = isLocalNetworkDenied(path);
            if (path) {
                nw_release(path);
            }
            if (denied) {
                probe->finish(true);
            }
        } break;
        case nw_connection_state_invalid:
        case nw_connection_state_preparing:
        case nw_connection_state_cancelled:
            break;
        }
    });
    nw_connection_start(connection);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), queue, ^{
        if (probe->completed) {
            return;
        }

        const auto path = nw_connection_copy_current_path(connection);
        const auto denied = isLocalNetworkDenied(path);
        if (path) {
            nw_release(path);
        }
        probe->finish(denied);
    });
}

QString deniedError()
{
    return QCoreApplication::translate("LocalNetworkPermission",
                                       "Local Network access is disabled. Enable it in System Settings → Privacy & Security → Local Network.");
}

} // namespace OCC::LocalNetworkPermission
