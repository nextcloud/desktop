/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KDSingleApplication>

namespace OCC::Utility
{

/** Creates an application guard shared by the user's login sessions. */
[[nodiscard]] inline KDSingleApplication makeLinuxSingleApplication(const QString &name)
{
    return KDSingleApplication(name, KDSingleApplication::Option::IncludeUsernameInSocketName);
}

}
