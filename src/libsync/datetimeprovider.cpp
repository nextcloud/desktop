/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "datetimeprovider.h"

namespace OCC {

DateTimeProvider::~DateTimeProvider() = default;

QDateTime DateTimeProvider::currentDateTime() const
{
    return QDateTime::currentDateTime();
}

QDate DateTimeProvider::currentDate() const
{
    return QDate::currentDate();
}

}
