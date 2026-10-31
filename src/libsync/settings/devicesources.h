/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEVICESOURCES_H
#define DEVICESOURCES_H

#include <memory>
#include <vector>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC
{

// Ordered device sources for the running platform.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildDeviceSources();

} // namespace OCC

#endif // DEVICESOURCES_H
