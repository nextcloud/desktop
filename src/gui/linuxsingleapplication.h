/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

namespace OCC::Utility
{

/** Socket name component shared by all launches of the same application and config directory.
 * KDSingleApplication adds the Linux user to the name.
 */
[[nodiscard]] QString linuxSingleApplicationName(const QString &applicationName, const QString &configDirectory);

}
