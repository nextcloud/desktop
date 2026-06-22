/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "themeutils.h"

FakePaintDevice::FakePaintDevice() = default;

QPaintEngine *FakePaintDevice::paintEngine() const
{
    return nullptr;
}

void FakePaintDevice::setHidpi(bool value)
{
    _hidpi = value;
}

int FakePaintDevice::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    switch (metric) {
    case QPaintDevice::PdmDevicePixelRatio:
        if (_hidpi) {
            return 2;
        }
        return 1;
    default:
        return QPaintDevice::metric(metric);
    }
}
