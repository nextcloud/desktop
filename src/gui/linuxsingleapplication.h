/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KDSingleApplication>

#include <optional>

namespace OCC::Utility
{

/** Coordinates instances of the same application for one Linux user.
 *
 * A temporary startup lock serializes guard acquisition so another upgraded
 * launch cannot forward to a shared listener that is about to be released.
 * The startup lock is released when the constructor returns.
 */
class LinuxSingleApplication final : public QObject
{
    Q_OBJECT

public:
    LinuxSingleApplication(const QString &name, const QString &legacyName);

    [[nodiscard]] bool isPrimaryInstance() const;
    bool sendMessage(const QByteArray &message);

Q_SIGNALS:
    void messageReceived(const QByteArray &message);

private:
    // Shared across this user's login sessions for the lifetime of the primary instance.
    std::optional<KDSingleApplication> _sharedGuard;
    // Keeps the old executable-and-session socket available for mixed-version launches
    // in this session. Omitted when it would use the same socket as the shared guard.
    std::optional<KDSingleApplication> _legacyGuard;
    // Points to the guard used to forward messages when this launch is secondary.
    KDSingleApplication *_secondaryGuard = nullptr;
};

}
