/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QJsonValue>
#include <QString>

namespace OCC::AssistantUtils {

/** @brief Converts a JSON number or numeric string to an integer. */
[[nodiscard]] qint64 jsonInteger(const QJsonValue &value, qint64 fallback = -1);
/** @brief Converts a task status JSON value to its string representation. */
[[nodiscard]] QString statusString(const QJsonValue &value);
/** @brief Extracts text from supported nested task input or output shapes. */
[[nodiscard]] QString textFromValue(const QJsonValue &value);
/** @brief Formats a seconds or milliseconds Unix timestamp for display. */
[[nodiscard]] QString dateText(qint64 timestamp);
/** @brief Returns whether a task status is not terminal. */
[[nodiscard]] bool taskStillRunning(const QJsonValue &status);

}
