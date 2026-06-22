/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
