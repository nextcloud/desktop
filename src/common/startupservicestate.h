/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

namespace OCC::Utility
{
struct StartupServiceState {
    enum class Status {
        NotApplicable,
        QueryFailed,
        Known,
    };

    Status status;
    QString unitFileState; //!< The systemd unit state when status is Known.
};
}
