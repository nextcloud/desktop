/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QUrl>

namespace OCC::AddAccountUrlHandler {

[[nodiscard]] bool isAddAccountActionUrl(const QUrl &url);
[[nodiscard]] QUrl parseAddAccountServerUrl(const QUrl &url);

/**
 * @brief Handle addAccount URL if applicable.
 * @return true if URL was recognized as addAccount and handled (success or validation error), false otherwise.
 */
[[nodiscard]] bool handleAddAccountUrl(const QUrl &url);

}

