/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UISCREENSHOTRUNNER_H
#define UISCREENSHOTRUNNER_H

#include <QString>

#include <optional>

namespace OCC::UiScreenshots {

/**
 * @brief Runs the requested phase inside the standalone screenshot executable.
 * @param argc Process argument count passed to QApplication.
 * @param argv Process arguments passed to QApplication.
 * @param widgetsStyle Optional platform widget style selected by the production entry point.
 * @return The screenshot process exit code, or `std::nullopt` when no phase was requested.
 */
[[nodiscard]] std::optional<int> runIfRequested(int &argc, char **argv, const QString &widgetsStyle);

}

#endif // UISCREENSHOTRUNNER_H
