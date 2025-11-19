/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WAIBAR_THEME_H
#define WAIBAR_THEME_H

#include "theme.h"

namespace OCC {

/**
 * @brief The WAIBARTheme class
 * @ingroup libsync
 */
class WAIBARTheme : public Theme
{
    Q_OBJECT
public:
    WAIBARTheme();

    [[nodiscard]] QString wizardUrlHint() const override;
};
}
#endif // WAIBAR_THEME_H
