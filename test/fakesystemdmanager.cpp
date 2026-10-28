/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fakesystemdmanager.h"
#include "systemdunitfilechange.h"

#include <QDBusError>
#include <QDBusMetaType>

using namespace Qt::StringLiterals;

bool FakeSystemdManager::start(const QString &address)
{
    qDBusRegisterMetaType<SystemdUnitFileChange>();
    qDBusRegisterMetaType<QList<SystemdUnitFileChange>>();
    _connection = QDBusConnection::connectToBus(address, u"startup-test-manager"_s);
    return _connection.isConnected() && _connection.registerVirtualObject(u"/org/freedesktop/systemd1"_s, this) && reset();
}

void FakeSystemdManager::stop()
{
    unregisterService();
    _connection.unregisterObject(u"/org/freedesktop/systemd1"_s);
    QDBusConnection::disconnectFromBus(_connection.name());
}

bool FakeSystemdManager::reset()
{
    state = u"enabled"_s;
    queryError.clear();
    queryTimeout = false;
    malformedQueryReply = false;
    extraQueryArgument = false;
    maskFailure = false;
    malformedMaskReply = false;
    emptyMaskReply = false;
    reloadFailure = false;
    malformedReloadReply = false;
    requests.clear();
    unregisterService();
    return _connection.registerService(u"org.freedesktop.systemd1"_s);
}

void FakeSystemdManager::unregisterService()
{
    _connection.unregisterService(u"org.freedesktop.systemd1"_s);
}

QString FakeSystemdManager::introspect(const QString &path) const
{
    Q_UNUSED(path)
    return uR"xml(<interface name="org.freedesktop.systemd1.Manager">
<method name="GetUnitFileState"><arg type="s" direction="in"/><arg type="s" direction="out"/></method>
<method name="MaskUnitFiles"><arg type="as" direction="in"/><arg type="b" direction="in"/><arg type="b" direction="in"/><arg type="a(sss)" direction="out"/></method>
<method name="Reload"/>
</interface>)xml"_s;
}

bool FakeSystemdManager::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.interface() != u"org.freedesktop.systemd1.Manager") {
        return false;
    }
    requests.append(message);
    if (message.member() == u"GetUnitFileState") {
        if (queryTimeout) {
            return true;
        }
        if (!queryError.isEmpty()) {
            return connection.send(message.createErrorReply(queryError, u"State unavailable"_s));
        }
        if (extraQueryArgument) {
            return connection.send(message.createReply(QVariantList{state, true}));
        }
        return connection.send(message.createReply(malformedQueryReply ? QVariant(true) : QVariant(state)));
    }
    if (message.member() == u"MaskUnitFiles") {
        if (maskFailure) {
            return connection.send(message.createErrorReply(QDBusError::AccessDenied, u"Mask denied"_s));
        }
        if (malformedMaskReply) {
            return connection.send(message.createReply(QVariant(true)));
        }
        state = u"masked"_s;
        const auto changes =
            emptyMaskReply ? QList<SystemdUnitFileChange>{} : QList<SystemdUnitFileChange>{{u"symlink"_s, u"/test/user/unit.service"_s, u"/dev/null"_s}};
        return connection.send(message.createReply(QVariant::fromValue(changes)));
    }
    if (message.member() == u"Reload") {
        if (reloadFailure) {
            return connection.send(message.createErrorReply(QDBusError::AccessDenied, u"Reload denied"_s));
        }
        return connection.send(malformedReloadReply ? message.createReply(QVariant(true)) : message.createReply());
    }
    return false;
}
