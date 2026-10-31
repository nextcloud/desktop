/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

namespace OCC::Gui::Sharing::RecipientIconUtils
{

// Encodes server-provided SVG XML as an image URL consumable by a QML Image.
[[nodiscard]] QString svgDataUrl(const QString &svg);

}
