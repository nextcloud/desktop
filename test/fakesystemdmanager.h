/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>

class FakeSystemdManager : public QDBusVirtualObject
{
public:
    [[nodiscard]] bool start(const QString &address);
    void stop();
    [[nodiscard]] bool reset();
    void unregisterService();

    [[nodiscard]] QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

    QString state;
    QString queryError;
    bool queryTimeout = false;
    bool malformedQueryReply = false;
    bool extraQueryArgument = false;
    bool maskFailure = false;
    bool malformedMaskReply = false;
    bool emptyMaskReply = false;
    bool reloadFailure = false;
    bool malformedReloadReply = false;
    QList<QDBusMessage> requests;

private:
    QDBusConnection _connection{QString()};
};
