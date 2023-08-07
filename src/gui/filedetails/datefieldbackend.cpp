/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "datefieldbackend.h"

namespace OCC
{
namespace Quick
{

QDateTime DateFieldBackend::dateTime() const
{
    return m_dateTime;
}

void DateFieldBackend::setDateTime(const QDateTime &dateTime)
{
    if (m_dateTime == dateTime) {
        return;
    }

    m_dateTime = dateTime;
    Q_EMIT dateTimeChanged();
}
}
}