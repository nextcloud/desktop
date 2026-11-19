/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "linuxsingleapplication.h"

#include <QDir>
#include <QLockFile>

#include <unistd.h>

using namespace Qt::StringLiterals;

namespace OCC::Utility
{

LinuxSingleApplication::LinuxSingleApplication(const QString &name, const QString &legacyName)
{
    constexpr auto startupLockTimeoutMs = 5000;
    // A second upgraded launch must wait until both guards have been checked before choosing where to send its message.
    auto startupLock = QLockFile(QDir::temp().filePath(u"%1-%2-instance-startup.lock"_s.arg(name).arg(::getuid())));
    startupLock.setStaleLockTime(0);
    if (!startupLock.tryLock(startupLockTimeoutMs)) {
        qWarning("Could not acquire the Linux single-instance startup lock.");
        return;
    }

    _sharedGuard.emplace(name, KDSingleApplication::Option::IncludeUsernameInSocketName);
    if (!_sharedGuard->isPrimaryInstance()) {
        _secondaryGuard = &*_sharedGuard;
        return;
    }

    // KDAB uses the same socket for both guards if neither the executable name nor the session suffix differs.
    if (name != legacyName || !qEnvironmentVariableIsEmpty("XDG_SESSION_ID")) {
        _legacyGuard.emplace(legacyName);
        if (!_legacyGuard->isPrimaryInstance()) {
            _sharedGuard.reset();
            _secondaryGuard = &*_legacyGuard;
            return;
        }
        connect(&*_legacyGuard, &KDSingleApplication::messageReceived, this, &LinuxSingleApplication::messageReceived);
    }
    connect(&*_sharedGuard, &KDSingleApplication::messageReceived, this, &LinuxSingleApplication::messageReceived);
}

bool LinuxSingleApplication::isPrimaryInstance() const
{
    return _sharedGuard.has_value() && !_secondaryGuard;
}

bool LinuxSingleApplication::sendMessage(const QByteArray &message)
{
    return _secondaryGuard && _secondaryGuard->sendMessage(message);
}

}
