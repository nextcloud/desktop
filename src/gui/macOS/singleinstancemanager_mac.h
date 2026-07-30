/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

class QLocalServer;

namespace OCC {

/**
 * @brief macOS single-instance guard using a UNIX socket in the app's
 *        sandboxed home directory.
 *
 * This replaces KDSingleApplication on macOS because KDSingleApplication's
 * default socket path ($TMPDIR/kdsingleapp-<uid>-<appname>) has two fatal
 * problems on macOS 13 Ventura:
 *
 *  1. Path too long: on macOS 13, $TMPDIR uses the /private/var/... canonical
 *     form, making the full socket path 110 bytes — 7 bytes over the 103-char
 *     limit imposed by struct sockaddr_un.sun_path[104].  On macOS 14+ the
 *     /private prefix is absent so the same path is only 102 bytes and just
 *     fits.
 *
 *  2. Sandbox denial: the macOS 13 App Sandbox does not grant network-bind for
 *     UNIX sockets placed in $TMPDIR, even when the com.apple.security.network
 *     .server entitlement is present.  That entitlement only covers TCP/UDP
 *     binding; coverage for UNIX sockets in $TMPDIR was added in macOS 14.
 *
 * The fix is to place the socket in the app's sandboxed home directory
 * (~/Library/Containers/com.nextcloud.desktopclient/Data), which the sandbox
 * has always allowed for both file I/O and UNIX socket binding.  A two-
 * character filename ("si") keeps the total path well within the 103-char
 * limit for any macOS username (max 31 chars → 96-char path total).
 *
 * The public interface is intentionally identical to the KDSingleApplication
 * methods and signals used by Application, so Application needs no logic
 * changes — only a conditional type selection in its header.
 *
 * This can be removed as soon as macOS 13 Ventura no longer is supported.
 */
class SingleInstanceManager : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceManager(QObject *parent = nullptr);
    ~SingleInstanceManager() override;

    [[nodiscard]] bool isPrimaryInstance() const;
    bool sendMessage(const QByteArray &data, int timeout = 1000);

signals:
    void messageReceived(const QByteArray &message);

private:
    [[nodiscard]] static QString socketPath();
    void onNewConnection();

    bool _isPrimary = false;
    QLocalServer *_server = nullptr;
};

} // namespace OCC
