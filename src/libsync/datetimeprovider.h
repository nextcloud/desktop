/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"

#include <QDateTime>

namespace OCC {

class OWNCLOUDSYNC_EXPORT DateTimeProvider
{
public:
    virtual ~DateTimeProvider();

    [[nodiscard]] virtual QDateTime currentDateTime() const;

    [[nodiscard]] virtual QDate currentDate() const;
};
}
