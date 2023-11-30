/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#pragma once
#include "owncloudlib.h"

#include <QtGlobal>
#include <QByteArray>

namespace OCC
{
/**
 * @brief The ClientStatusReportingRecord class
 * @ingroup libsync
 */

struct OWNCLOUDSYNC_EXPORT ClientStatusReportingRecord {
    QByteArray _name;
    int _status = -1;
    quint64 _numOccurences = 1;
    quint64 _lastOccurence = 0;

    [[nodiscard]] inline bool isValid() const
    {
        return _status >= 0 && !_name.isEmpty() && _lastOccurence > 0;
    }
};
}
