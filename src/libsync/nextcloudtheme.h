/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NEXTCLOUD_THEME_H
#define NEXTCLOUD_THEME_H

#include "theme.h"

namespace OCC {

/**
 * @brief The NextcloudTheme class
 * @ingroup libsync
 */
class NextcloudTheme : public Theme
{
    Q_OBJECT
public:
    NextcloudTheme();

    [[nodiscard]] QString wizardUrlHint() const override;
};
}
#endif // NEXTCLOUD_THEME_H
