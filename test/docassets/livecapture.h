/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

namespace OCC
{
class Application;
}

namespace OCC::DocAssets
{
struct LiveProfile;

[[nodiscard]] bool captureLiveScenario(Application &application,
                                       const LiveProfile &profile,
                                       const QString &scenario,
                                       const QString &stagingDirectory,
                                       int timeoutMs,
                                       QString *error,
                                       bool *skipped);
}
