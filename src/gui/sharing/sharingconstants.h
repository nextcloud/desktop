/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLatin1StringView>

namespace OCC::Gui::Sharing::SourceTypeClasses
{

inline constexpr auto node = QLatin1StringView{"OCA\\Files\\Sharing\\Source\\NodeShareSourceType"};

}

namespace OCC::Gui::Sharing::RecipientTypeClasses
{

inline constexpr auto email = QLatin1StringView{"OC\\Core\\Sharing\\Recipient\\EmailShareRecipientType"};
inline constexpr auto group = QLatin1StringView{"OC\\Core\\Sharing\\Recipient\\GroupShareRecipientType"};
inline constexpr auto team = QLatin1StringView{"OC\\Core\\Sharing\\Recipient\\TeamShareRecipientType"};
inline constexpr auto token = QLatin1StringView{"OC\\Core\\Sharing\\Recipient\\TokenShareRecipientType"};
inline constexpr auto user = QLatin1StringView{"OC\\Core\\Sharing\\Recipient\\UserShareRecipientType"};

}
